#include "main.h"
#include "vfunc_hook.hpp"
#include "Protobuffs.h"
#include <future>

Protobuffs ProtoFeatures;

vfunc_hook gc_hook;
ISteamGameCoordinator* g_SteamGameCoordinator = nullptr;
IMemAlloc*  		   g_pMemAlloc = nullptr;
ISteamUser*            g_SteamUser = nullptr;

using GCRetrieveMessage = EGCResult(__thiscall*)(void*, uint32_t *punMsgType, void *pubDest, uint32_t cubDest, uint32_t *pcubMsgSize);
using GCSendMessage = EGCResult(__thiscall*)(void*, uint32_t unMsgType, const void* pubData, uint32_t cubData);

EGCResult __fastcall hkGCRetrieveMessage(void* ecx, void*, uint32_t *punMsgType, void *pubDest, uint32_t cubDest, uint32_t *pcubMsgSize)
{
	static auto oGCRetrieveMessage = gc_hook.get_original<GCRetrieveMessage>(2);
	auto status = oGCRetrieveMessage(ecx, punMsgType, pubDest, cubDest, pcubMsgSize);

	if (status == k_EGCResultOK)
	{

		void* thisPtr = nullptr;
		__asm mov thisPtr, ebx;
		auto oldEBP = *reinterpret_cast<void**>((uint32_t)_AddressOfReturnAddress() - 4);

		uint32_t messageType = *punMsgType & 0x7FFFFFFF;
		ProtoFeatures.ReceiveMessage(thisPtr, oldEBP, messageType, pubDest, cubDest, pcubMsgSize);
	}

	return status;
}

EGCResult __fastcall hkGCSendMessage(void* ecx, void*, uint32_t unMsgType, const void* pubData, uint32_t cubData)
{
	static auto oGCSendMessage = gc_hook.get_original<GCSendMessage>(0);

	bool sendMessage = ProtoFeatures.PreSendMessage(unMsgType, const_cast<void*>(pubData), cubData);

	if (!sendMessage)
		return k_EGCResultOK;

	return oGCSendMessage(ecx, unMsgType, const_cast<void*>(pubData), cubData);
}

DWORD WINAPI OnDllAttach(LPVOID base)
{
	::AllocConsole() && ::AttachConsole(GetCurrentProcessId());
	freopen("CONIN$", "r", stdin);
	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);

	auto SteamClient = ((ISteamClient*(__cdecl*)(void))GetProcAddress(GetModuleHandleA("steam_api.dll"), "SteamClient"))();
	g_SteamGameCoordinator = (ISteamGameCoordinator*)SteamClient->GetISteamGenericInterface((void*)1, (void*)1, "SteamGameCoordinator001");
	g_pMemAlloc = *(IMemAlloc**)GetProcAddress(GetModuleHandleW(L"tier0.dll"), "g_pMemAlloc");
	g_SteamUser = SteamClient->GetISteamUser((void*)1, (void*)1, "SteamUser019");

	printf("SteamClient %X\n", SteamClient);
	printf("g_SteamGameCoordinator %X\n", g_SteamGameCoordinator);
	printf("g_pMemAlloc %X\n", g_pMemAlloc);
	printf("g_SteamUser %X\n", g_SteamUser);
	printf("Initialized\n");

	gc_hook.setup(g_SteamGameCoordinator);
	gc_hook.hook_index(0, hkGCSendMessage);
	gc_hook.hook_index(2, hkGCRetrieveMessage);

	ProtoFeatures.SendClientHello();
	ProtoFeatures.SendMatchmakingClient2GCHello();

	while (!GetAsyncKeyState(VK_END))
		Sleep(100);

	gc_hook.unhook_all();

	ProtoFeatures.SendClientHello();
	ProtoFeatures.SendMatchmakingClient2GCHello();

	printf("Destroyed\n");

	FreeLibraryAndExitThread(static_cast<HMODULE>(base), 1);
	
	return 0;
}

BOOL WINAPI DllMain(
	_In_      HINSTANCE hinstDll,
	_In_      DWORD     fdwReason,
	_In_opt_  LPVOID    lpvReserved
)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(hinstDll);
		CreateThread(nullptr, 0, OnDllAttach, hinstDll, 0, nullptr);
	}
	return TRUE;
}