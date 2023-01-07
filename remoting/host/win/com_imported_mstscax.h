// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// C++ source equivalent of Win32 type library MsTscAx.MsTscAx.
// This file has to be checked-in due to a bug in the VS2013 64-32 cross
// compiler which results in invalid interfaces (crbug.com/318952).
//
// This file was generated using the following pragma directive:
// #import "PROGID:MsTscAx.MsTscAx" \
//     exclude("wireHWND", "_RemotableHandle", "__MIDL_IWinTypes_0009"), \
//     rename_namespace("mstsc") raw_interfaces_only no_implementation
//
// Created by Microsoft (R) C/C++ Compiler Version 14.00.24213.1 (d1417caf).
// compiler-generated file created 12/20/16 at 15:35:24 - DO NOT EDIT!

#pragma once
#pragma pack(push, 8)

#include <comdef.h>

namespace mstsc {

//
// Forward references and typedefs
//

struct __declspec(uuid("8c11efa1-92c3-11d1-bc1e-00c04fa31489"))
/* LIBID */ __MSTSCLib;
struct __declspec(uuid("336d5562-efa8-482e-8cb3-c5c0fc7a7db6"))
/* dispinterface */ IMsTscAxEvents;
enum __MIDL___MIDL_itf_mstsax_0000_0037_0001;
enum __MIDL___MIDL_itf_mstsax_0000_0049_0001;
enum __MIDL___MIDL_itf_mstsax_0000_0049_0002;
struct /* coclass */ MsTscAxNotSafeForScripting;
struct __declspec(uuid("92b4a539-7115-4b7c-a5a9-e5d9efc2780a"))
/* dual interface */ IMsRdpClient;
struct __declspec(uuid("8c11efae-92c3-11d1-bc1e-00c04fa31489"))
/* dual interface */ IMsTscAx;
struct __declspec(uuid("327bb5cd-834e-4400-aef2-b30e15e5d682"))
/* dual interface */ IMsTscAx_Redist;
struct __declspec(uuid("c9d65442-a0f9-45b2-8f73-d61d2db8cbb6"))
/* dual interface */ IMsTscSecuredSettings;
struct __declspec(uuid("809945cc-4b3b-4a92-a6b0-dbf9b5f2ef2d"))
/* dual interface */ IMsTscAdvancedSettings;
struct __declspec(uuid("209d0eb9-6254-47b1-9033-a98dae55bb27"))
/* dual interface */ IMsTscDebug;
struct __declspec(uuid("3c65b4ab-12b3-465b-acd4-b8dad3bff9e2"))
/* dual interface */ IMsRdpClientAdvancedSettings;
struct __declspec(uuid("605befcf-39c1-45cc-a811-068fb7be346d"))
/* dual interface */ IMsRdpClientSecuredSettings;
enum __MIDL___MIDL_itf_mstsax_0000_0000_0001;
enum __MIDL_IMsRdpClient_0001;
struct __declspec(uuid("c1e6743a-41c1-4a74-832a-0dd06c1c7a0e"))
/* interface */ IMsTscNonScriptable;
struct __declspec(uuid("2f079c4c-87b2-4afd-97ab-20cdb43038ae"))
/* interface */ IMsRdpClientNonScriptable;
struct /* coclass */ MsTscAx;
struct /* coclass */ MsRdpClientNotSafeForScripting;
struct /* coclass */ MsRdpClient;
struct /* coclass */ MsRdpClient2NotSafeForScripting;
struct __declspec(uuid("e7e17dc4-3b71-4ba7-a8e6-281ffadca28f"))
/* dual interface */ IMsRdpClient2;
struct __declspec(uuid("9ac42117-2b76-4320-aa44-0e616ab8437b"))
/* dual interface */ IMsRdpClientAdvancedSettings2;
struct /* coclass */ MsRdpClient2;
struct /* coclass */ MsRdpClient2a;
struct /* coclass */ MsRdpClient3NotSafeForScripting;
struct __declspec(uuid("91b7cbc5-a72e-4fa0-9300-d647d7e897ff"))
/* dual interface */ IMsRdpClient3;
struct __declspec(uuid("19cd856b-c542-4c53-acee-f127e3be1a59"))
/* dual interface */ IMsRdpClientAdvancedSettings3;
struct /* coclass */ MsRdpClient3;
struct /* coclass */ MsRdpClient3a;
struct /* coclass */ MsRdpClient4NotSafeForScripting;
struct __declspec(uuid("095e0738-d97d-488b-b9f6-dd0e8d66c0de"))
/* dual interface */ IMsRdpClient4;
struct __declspec(uuid("fba7f64e-7345-4405-ae50-fa4a763dc0de"))
/* dual interface */ IMsRdpClientAdvancedSettings4;
struct __declspec(uuid("17a5e535-4072-4fa4-af32-c8d0d47345e9"))
/* interface */ IMsRdpClientNonScriptable2;
struct /* coclass */ MsRdpClient4;
struct /* coclass */ MsRdpClient4a;
struct /* coclass */ MsRdpClient5NotSafeForScripting;
struct __declspec(uuid("4eb5335b-6429-477d-b922-e06a28ecd8bf"))
/* dual interface */ IMsRdpClient5;
struct __declspec(uuid("720298c0-a099-46f5-9f82-96921bae4701"))
/* dual interface */ IMsRdpClientTransportSettings;
struct __declspec(uuid("fba7f64e-6783-4405-da45-fa4a763dabd0"))
/* dual interface */ IMsRdpClientAdvancedSettings5;
struct __declspec(uuid("fdd029f9-467a-4c49-8529-64b521dbd1b4"))
/* dual interface */ ITSRemoteProgram;
struct __declspec(uuid("d012ae6d-c19a-4bfe-b367-201f8911f134"))
/* dual interface */ IMsRdpClientShell;
struct __declspec(uuid("b3378d90-0728-45c7-8ed7-b6159fb92219"))
/* interface */ IMsRdpClientNonScriptable3;
struct __declspec(uuid("56540617-d281-488c-8738-6a8fdf64a118"))
/* interface */ IMsRdpDeviceCollection;
struct __declspec(uuid("60c3b9c8-9e92-4f5e-a3e7-604a912093ea"))
/* interface */ IMsRdpDevice;
struct __declspec(uuid("7ff17599-da2c-4677-ad35-f60c04fe1585"))
/* interface */ IMsRdpDriveCollection;
struct __declspec(uuid("d28b5458-f694-47a8-8e61-40356a767e46"))
/* interface */ IMsRdpDrive;
struct /* coclass */ MsRdpClient5;
struct /* coclass */ MsRdpClient6NotSafeForScripting;
struct __declspec(uuid("d43b7d80-8517-4b6d-9eac-96ad6800d7f2"))
/* dual interface */ IMsRdpClient6;
struct __declspec(uuid("222c4b5d-45d9-4df0-a7c6-60cf9089d285"))
/* dual interface */ IMsRdpClientAdvancedSettings6;
struct __declspec(uuid("67341688-d606-4c73-a5d2-2e0489009319"))
/* dual interface */ IMsRdpClientTransportSettings2;
struct __declspec(uuid("f50fa8aa-1c7d-4f59-b15c-a90cacae1fcb"))
/* interface */ IMsRdpClientNonScriptable4;
enum __MIDL_IMsRdpClientNonScriptable4_0001;
struct /* coclass */ MsRdpClient6;
struct /* coclass */ MsRdpClient7NotSafeForScripting;
struct __declspec(uuid("b2a5b5ce-3461-444a-91d4-add26d070638"))
/* dual interface */ IMsRdpClient7;
struct __declspec(uuid("26036036-4010-4578-8091-0db9a1edf9c3"))
/* dual interface */ IMsRdpClientAdvancedSettings7;
struct __declspec(uuid("3d5b21ac-748d-41de-8f30-e15169586bd4"))
/* dual interface */ IMsRdpClientTransportSettings3;
struct __declspec(uuid("25f2ce20-8b1d-4971-a7cd-549dae201fc0"))
/* dual interface */ IMsRdpClientSecuredSettings2;
struct __declspec(uuid("92c38a7d-241a-418c-9936-099872c9af20"))
/* dual interface */ ITSRemoteProgram2;
struct __declspec(uuid("4f6996d5-d7b1-412c-b0ff-063718566907"))
/* interface */ IMsRdpClientNonScriptable5;
struct __declspec(uuid("fdd029f9-9574-4def-8529-64b521cccaa4"))
/* interface */ IMsRdpPreferredRedirectionInfo;
struct __declspec(uuid("302d8188-0052-4807-806a-362b628f9ac5"))
/* interface */ IMsRdpExtendedSettings;
struct /* coclass */ MsRdpClient7;
struct /* coclass */ MsRdpClient8NotSafeForScripting;
struct __declspec(uuid("4247e044-9271-43a9-bc49-e2ad9e855d62"))
/* dual interface */ IMsRdpClient8;
enum __MIDL___MIDL_itf_mstsax_0000_0000_0004;
struct __declspec(uuid("89acb528-2557-4d16-8625-226a30e97e9a"))
/* dual interface */ IMsRdpClientAdvancedSettings8;
enum __MIDL___MIDL_itf_mstsax_0000_0000_0003;
enum __MIDL_IMsRdpClient8_0001;
struct /* coclass */ MsRdpClient8;
struct /* coclass */ MsRdpClient9NotSafeForScripting;
struct __declspec(uuid("28904001-04b6-436c-a55b-0af1a0883dc9"))
/* dual interface */ IMsRdpClient9;
struct __declspec(uuid("011c3236-4d81-4515-9143-067ab630d299"))
/* dual interface */ IMsRdpClientTransportSettings4;
struct /* coclass */ MsRdpClient9;
struct __declspec(uuid("079863b7-6d47-4105-8bfe-0cdcb360e67d"))
/* dispinterface */ IRemoteDesktopClientEvents;
struct /* coclass */ RemoteDesktopClient;
struct __declspec(uuid("57d25668-625a-4905-be4e-304caa13f89c"))
/* dual interface */ IRemoteDesktopClient;
struct __declspec(uuid("48a0f2a7-2713-431f-bbac-6f4558e7d64d"))
/* dual interface */ IRemoteDesktopClientSettings;
struct __declspec(uuid("7d54bc4e-1028-45d4-8b0a-b9b6bffba176"))
/* dual interface */ IRemoteDesktopClientActions;
enum __MIDL_IRemoteDesktopClientActions_0001;
enum __MIDL_IRemoteDesktopClientActions_0002;
enum __MIDL_IRemoteDesktopClientActions_0003;
struct __declspec(uuid("260ec22d-8cbc-44b5-9e88-2a37f6c93ae9"))
/* dual interface */ IRemoteDesktopClientTouchPointer;
typedef enum __MIDL___MIDL_itf_mstsax_0000_0037_0001 AutoReconnectContinueState;
typedef enum __MIDL___MIDL_itf_mstsax_0000_0049_0001 RemoteProgramResult;
typedef enum __MIDL___MIDL_itf_mstsax_0000_0049_0002 RemoteWindowDisplayedAttribute;
typedef enum __MIDL___MIDL_itf_mstsax_0000_0000_0001 ExtendedDisconnectReasonCode;
typedef enum __MIDL_IMsRdpClient_0001 ControlCloseStatus;
#if !defined(_WIN64)
typedef __w64 unsigned long UINT_PTR;
#else
typedef unsigned __int64 UINT_PTR;
#endif
#if !defined(_WIN64)
typedef __w64 long LONG_PTR;
#else
typedef __int64 LONG_PTR;
#endif
typedef enum __MIDL_IMsRdpClientNonScriptable4_0001 RedirectionWarningType;
typedef enum __MIDL___MIDL_itf_mstsax_0000_0000_0004 RemoteSessionActionType;
typedef enum __MIDL___MIDL_itf_mstsax_0000_0000_0003 ClientSpec;
typedef enum __MIDL_IMsRdpClient8_0001 ControlReconnectStatus;
typedef enum __MIDL_IRemoteDesktopClientActions_0001 RemoteActionType;
typedef enum __MIDL_IRemoteDesktopClientActions_0002 SnapshotEncodingType;
typedef enum __MIDL_IRemoteDesktopClientActions_0003 SnapshotFormatType;

//
// Smart pointer typedef declarations
//

_COM_SMARTPTR_TYPEDEF(IMsTscAxEvents, __uuidof(IMsTscAxEvents));
_COM_SMARTPTR_TYPEDEF(IMsTscAx_Redist, __uuidof(IMsTscAx_Redist));
_COM_SMARTPTR_TYPEDEF(IMsTscSecuredSettings, __uuidof(IMsTscSecuredSettings));
_COM_SMARTPTR_TYPEDEF(IMsTscAdvancedSettings, __uuidof(IMsTscAdvancedSettings));
_COM_SMARTPTR_TYPEDEF(IMsTscDebug, __uuidof(IMsTscDebug));
_COM_SMARTPTR_TYPEDEF(IMsTscAx, __uuidof(IMsTscAx));
_COM_SMARTPTR_TYPEDEF(IMsRdpClientAdvancedSettings, __uuidof(IMsRdpClientAdvancedSettings));
_COM_SMARTPTR_TYPEDEF(IMsRdpClientSecuredSettings, __uuidof(IMsRdpClientSecuredSettings));
_COM_SMARTPTR_TYPEDEF(IMsRdpClient, __uuidof(IMsRdpClient));
_COM_SMARTPTR_TYPEDEF(IMsTscNonScriptable, __uuidof(IMsTscNonScriptable));
_COM_SMARTPTR_TYPEDEF(IMsRdpClientNonScriptable, __uuidof(IMsRdpClientNonScriptable));
_COM_SMARTPTR_TYPEDEF(IMsRdpClientAdvancedSettings2, __uuidof(IMsRdpClientAdvancedSettings2));
_COM_SMARTPTR_TYPEDEF(IMsRdpClient2, __uuidof(IMsRdpClient2));
_COM_SMARTPTR_TYPEDEF(IMsRdpClientAdvancedSettings3, __uuidof(IMsRdpClientAdvancedSettings3));
_COM_SMARTPTR_TYPEDEF(IMsRdpClient3, __uuidof(IMsRdpClient3));
_COM_SMARTPTR_TYPEDEF(IMsRdpClientAdvancedSettings4, __uuidof(IMsRdpClientAdvancedSettings4));
_COM_SMARTPTR_TYPEDEF(IMsRdpClient4, __uuidof(IMsRdpClient4));
_COM_SMARTPTR_TYPEDEF(IMsRdpClientNonScriptable2, __uuidof(IMsRdpClientNonScriptable2));
_COM_SMARTPTR_TYPEDEF(IMsRdpClientTransportSettings, __uuidof(IMsRdpClientTransportSettings));
_COM_SMARTPTR_TYPEDEF(IMsRdpClientAdvancedSettings5, __uuidof(IMsRdpClientAdvancedSettings5));
_COM_SMARTPTR_TYPEDEF(ITSRemoteProgram, __uuidof(ITSRemoteProgram));
_COM_SMARTPTR_TYPEDEF(IMsRdpClientShell, __uuidof(IMsRdpClientShell));
_COM_SMARTPTR_TYPEDEF(IMsRdpClient5, __uuidof(IMsRdpClient5));
_COM_SMARTPTR_TYPEDEF(IMsRdpDevice, __uuidof(IMsRdpDevice));
_COM_SMARTPTR_TYPEDEF(IMsRdpDeviceCollection, __uuidof(IMsRdpDeviceCollection));
_COM_SMARTPTR_TYPEDEF(IMsRdpDrive, __uuidof(IMsRdpDrive));
_COM_SMARTPTR_TYPEDEF(IMsRdpDriveCollection, __uuidof(IMsRdpDriveCollection));
_COM_SMARTPTR_TYPEDEF(IMsRdpClientNonScriptable3, __uuidof(IMsRdpClientNonScriptable3));
_COM_SMARTPTR_TYPEDEF(IMsRdpClientAdvancedSettings6, __uuidof(IMsRdpClientAdvancedSettings6));
_COM_SMARTPTR_TYPEDEF(IMsRdpClientTransportSettings2, __uuidof(IMsRdpClientTransportSettings2));
_COM_SMARTPTR_TYPEDEF(IMsRdpClient6, __uuidof(IMsRdpClient6));
_COM_SMARTPTR_TYPEDEF(IMsRdpClientNonScriptable4, __uuidof(IMsRdpClientNonScriptable4));
_COM_SMARTPTR_TYPEDEF(IMsRdpClientAdvancedSettings7, __uuidof(IMsRdpClientAdvancedSettings7));
_COM_SMARTPTR_TYPEDEF(IMsRdpClientTransportSettings3, __uuidof(IMsRdpClientTransportSettings3));
_COM_SMARTPTR_TYPEDEF(IMsRdpClientSecuredSettings2, __uuidof(IMsRdpClientSecuredSettings2));
_COM_SMARTPTR_TYPEDEF(ITSRemoteProgram2, __uuidof(ITSRemoteProgram2));
_COM_SMARTPTR_TYPEDEF(IMsRdpClient7, __uuidof(IMsRdpClient7));
_COM_SMARTPTR_TYPEDEF(IMsRdpClientNonScriptable5, __uuidof(IMsRdpClientNonScriptable5));
_COM_SMARTPTR_TYPEDEF(IMsRdpPreferredRedirectionInfo, __uuidof(IMsRdpPreferredRedirectionInfo));
_COM_SMARTPTR_TYPEDEF(IMsRdpExtendedSettings, __uuidof(IMsRdpExtendedSettings));
_COM_SMARTPTR_TYPEDEF(IMsRdpClientAdvancedSettings8, __uuidof(IMsRdpClientAdvancedSettings8));
_COM_SMARTPTR_TYPEDEF(IMsRdpClient8, __uuidof(IMsRdpClient8));
_COM_SMARTPTR_TYPEDEF(IMsRdpClientTransportSettings4, __uuidof(IMsRdpClientTransportSettings4));
_COM_SMARTPTR_TYPEDEF(IMsRdpClient9, __uuidof(IMsRdpClient9));
_COM_SMARTPTR_TYPEDEF(IRemoteDesktopClientEvents, __uuidof(IRemoteDesktopClientEvents));
_COM_SMARTPTR_TYPEDEF(IRemoteDesktopClientSettings, __uuidof(IRemoteDesktopClientSettings));
_COM_SMARTPTR_TYPEDEF(IRemoteDesktopClientActions, __uuidof(IRemoteDesktopClientActions));
_COM_SMARTPTR_TYPEDEF(IRemoteDesktopClientTouchPointer, __uuidof(IRemoteDesktopClientTouchPointer));
_COM_SMARTPTR_TYPEDEF(IRemoteDesktopClient, __uuidof(IRemoteDesktopClient));

//
// Type library items
//

struct __declspec(uuid("336d5562-efa8-482e-8cb3-c5c0fc7a7db6"))
IMsTscAxEvents : IDispatch
{};

enum __MIDL___MIDL_itf_mstsax_0000_0037_0001
{
    autoReconnectContinueAutomatic = 0,
    autoReconnectContinueStop = 1,
    autoReconnectContinueManual = 2
};

enum __MIDL___MIDL_itf_mstsax_0000_0049_0001
{
    remoteAppResultOk = 0,
    remoteAppResultLocked = 1,
    remoteAppResultProtocolError = 2,
    remoteAppResultNotInWhitelist = 3,
    remoteAppResultNetworkPathDenied = 4,
    remoteAppResultFileNotFound = 5,
    remoteAppResultFailure = 6,
    remoteAppResultHookNotLoaded = 7
};

enum __MIDL___MIDL_itf_mstsax_0000_0049_0002
{
    remoteAppWindowNone = 0,
    remoteAppWindowDisplayed = 1,
    remoteAppShellIconDisplayed = 2
};

struct __declspec(uuid("a41a4187-5a86-4e26-b40a-856f9035d9cb"))
MsTscAxNotSafeForScripting;
    // interface IMsRdpClient
    // [ default ] interface IMsTscAx
    // interface IMsTscAx_Redist
    // [ default, source ] dispinterface IMsTscAxEvents
    // interface IMsTscNonScriptable
    // interface IMsRdpClientNonScriptable

struct __declspec(uuid("327bb5cd-834e-4400-aef2-b30e15e5d682"))
IMsTscAx_Redist : IDispatch
{};

struct __declspec(uuid("c9d65442-a0f9-45b2-8f73-d61d2db8cbb6"))
IMsTscSecuredSettings : IDispatch
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall put_StartProgram (
        /*[in]*/ BSTR pStartProgram ) = 0;
      virtual HRESULT __stdcall get_StartProgram (
        /*[out,retval]*/ BSTR * pStartProgram ) = 0;
      virtual HRESULT __stdcall put_WorkDir (
        /*[in]*/ BSTR pWorkDir ) = 0;
      virtual HRESULT __stdcall get_WorkDir (
        /*[out,retval]*/ BSTR * pWorkDir ) = 0;
      virtual HRESULT __stdcall put_FullScreen (
        /*[in]*/ long pfFullScreen ) = 0;
      virtual HRESULT __stdcall get_FullScreen (
        /*[out,retval]*/ long * pfFullScreen ) = 0;
};

struct __declspec(uuid("809945cc-4b3b-4a92-a6b0-dbf9b5f2ef2d"))
IMsTscAdvancedSettings : IDispatch
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall put_Compress (
        /*[in]*/ long pcompress ) = 0;
      virtual HRESULT __stdcall get_Compress (
        /*[out,retval]*/ long * pcompress ) = 0;
      virtual HRESULT __stdcall put_BitmapPeristence (
        /*[in]*/ long pbitmapPeristence ) = 0;
      virtual HRESULT __stdcall get_BitmapPeristence (
        /*[out,retval]*/ long * pbitmapPeristence ) = 0;
      virtual HRESULT __stdcall put_allowBackgroundInput (
        /*[in]*/ long pallowBackgroundInput ) = 0;
      virtual HRESULT __stdcall get_allowBackgroundInput (
        /*[out,retval]*/ long * pallowBackgroundInput ) = 0;
      virtual HRESULT __stdcall put_KeyBoardLayoutStr (
        /*[in]*/ BSTR _arg1 ) = 0;
      virtual HRESULT __stdcall put_PluginDlls (
        /*[in]*/ BSTR _arg1 ) = 0;
      virtual HRESULT __stdcall put_IconFile (
        /*[in]*/ BSTR _arg1 ) = 0;
      virtual HRESULT __stdcall put_IconIndex (
        /*[in]*/ long _arg1 ) = 0;
      virtual HRESULT __stdcall put_ContainerHandledFullScreen (
        /*[in]*/ long pContainerHandledFullScreen ) = 0;
      virtual HRESULT __stdcall get_ContainerHandledFullScreen (
        /*[out,retval]*/ long * pContainerHandledFullScreen ) = 0;
      virtual HRESULT __stdcall put_DisableRdpdr (
        /*[in]*/ long pDisableRdpdr ) = 0;
      virtual HRESULT __stdcall get_DisableRdpdr (
        /*[out,retval]*/ long * pDisableRdpdr ) = 0;
};

struct __declspec(uuid("209d0eb9-6254-47b1-9033-a98dae55bb27"))
IMsTscDebug : IDispatch
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall put_HatchBitmapPDU (
        /*[in]*/ long phatchBitmapPDU ) = 0;
      virtual HRESULT __stdcall get_HatchBitmapPDU (
        /*[out,retval]*/ long * phatchBitmapPDU ) = 0;
      virtual HRESULT __stdcall put_HatchSSBOrder (
        /*[in]*/ long phatchSSBOrder ) = 0;
      virtual HRESULT __stdcall get_HatchSSBOrder (
        /*[out,retval]*/ long * phatchSSBOrder ) = 0;
      virtual HRESULT __stdcall put_HatchMembltOrder (
        /*[in]*/ long phatchMembltOrder ) = 0;
      virtual HRESULT __stdcall get_HatchMembltOrder (
        /*[out,retval]*/ long * phatchMembltOrder ) = 0;
      virtual HRESULT __stdcall put_HatchIndexPDU (
        /*[in]*/ long phatchIndexPDU ) = 0;
      virtual HRESULT __stdcall get_HatchIndexPDU (
        /*[out,retval]*/ long * phatchIndexPDU ) = 0;
      virtual HRESULT __stdcall put_LabelMemblt (
        /*[in]*/ long plabelMemblt ) = 0;
      virtual HRESULT __stdcall get_LabelMemblt (
        /*[out,retval]*/ long * plabelMemblt ) = 0;
      virtual HRESULT __stdcall put_BitmapCacheMonitor (
        /*[in]*/ long pbitmapCacheMonitor ) = 0;
      virtual HRESULT __stdcall get_BitmapCacheMonitor (
        /*[out,retval]*/ long * pbitmapCacheMonitor ) = 0;
      virtual HRESULT __stdcall put_MallocFailuresPercent (
        /*[in]*/ long pmallocFailuresPercent ) = 0;
      virtual HRESULT __stdcall get_MallocFailuresPercent (
        /*[out,retval]*/ long * pmallocFailuresPercent ) = 0;
      virtual HRESULT __stdcall put_MallocHugeFailuresPercent (
        /*[in]*/ long pmallocHugeFailuresPercent ) = 0;
      virtual HRESULT __stdcall get_MallocHugeFailuresPercent (
        /*[out,retval]*/ long * pmallocHugeFailuresPercent ) = 0;
      virtual HRESULT __stdcall put_NetThroughput (
        /*[in]*/ long NetThroughput ) = 0;
      virtual HRESULT __stdcall get_NetThroughput (
        /*[out,retval]*/ long * NetThroughput ) = 0;
      virtual HRESULT __stdcall put_CLXCmdLine (
        /*[in]*/ BSTR pCLXCmdLine ) = 0;
      virtual HRESULT __stdcall get_CLXCmdLine (
        /*[out,retval]*/ BSTR * pCLXCmdLine ) = 0;
      virtual HRESULT __stdcall put_CLXDll (
        /*[in]*/ BSTR pCLXDll ) = 0;
      virtual HRESULT __stdcall get_CLXDll (
        /*[out,retval]*/ BSTR * pCLXDll ) = 0;
      virtual HRESULT __stdcall put_RemoteProgramsHatchVisibleRegion (
        /*[in]*/ long pcbHatch ) = 0;
      virtual HRESULT __stdcall get_RemoteProgramsHatchVisibleRegion (
        /*[out,retval]*/ long * pcbHatch ) = 0;
      virtual HRESULT __stdcall put_RemoteProgramsHatchVisibleNoDataRegion (
        /*[in]*/ long pcbHatch ) = 0;
      virtual HRESULT __stdcall get_RemoteProgramsHatchVisibleNoDataRegion (
        /*[out,retval]*/ long * pcbHatch ) = 0;
      virtual HRESULT __stdcall put_RemoteProgramsHatchNonVisibleRegion (
        /*[in]*/ long pcbHatch ) = 0;
      virtual HRESULT __stdcall get_RemoteProgramsHatchNonVisibleRegion (
        /*[out,retval]*/ long * pcbHatch ) = 0;
      virtual HRESULT __stdcall put_RemoteProgramsHatchWindow (
        /*[in]*/ long pcbHatch ) = 0;
      virtual HRESULT __stdcall get_RemoteProgramsHatchWindow (
        /*[out,retval]*/ long * pcbHatch ) = 0;
      virtual HRESULT __stdcall put_RemoteProgramsStayConnectOnBadCaps (
        /*[in]*/ long pcbStayConnected ) = 0;
      virtual HRESULT __stdcall get_RemoteProgramsStayConnectOnBadCaps (
        /*[out,retval]*/ long * pcbStayConnected ) = 0;
      virtual HRESULT __stdcall get_ControlType (
        /*[out,retval]*/ unsigned int * pControlType ) = 0;
      virtual HRESULT __stdcall put_DecodeGfx (
        /*[in]*/ VARIANT_BOOL _arg1 ) = 0;
};

struct __declspec(uuid("8c11efae-92c3-11d1-bc1e-00c04fa31489"))
IMsTscAx : IMsTscAx_Redist
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall put_Server (
        /*[in]*/ BSTR pServer ) = 0;
      virtual HRESULT __stdcall get_Server (
        /*[out,retval]*/ BSTR * pServer ) = 0;
      virtual HRESULT __stdcall put_Domain (
        /*[in]*/ BSTR pDomain ) = 0;
      virtual HRESULT __stdcall get_Domain (
        /*[out,retval]*/ BSTR * pDomain ) = 0;
      virtual HRESULT __stdcall put_UserName (
        /*[in]*/ BSTR pUserName ) = 0;
      virtual HRESULT __stdcall get_UserName (
        /*[out,retval]*/ BSTR * pUserName ) = 0;
      virtual HRESULT __stdcall put_DisconnectedText (
        /*[in]*/ BSTR pDisconnectedText ) = 0;
      virtual HRESULT __stdcall get_DisconnectedText (
        /*[out,retval]*/ BSTR * pDisconnectedText ) = 0;
      virtual HRESULT __stdcall put_ConnectingText (
        /*[in]*/ BSTR pConnectingText ) = 0;
      virtual HRESULT __stdcall get_ConnectingText (
        /*[out,retval]*/ BSTR * pConnectingText ) = 0;
      virtual HRESULT __stdcall get_Connected (
        /*[out,retval]*/ short * pIsConnected ) = 0;
      virtual HRESULT __stdcall put_DesktopWidth (
        /*[in]*/ long pVal ) = 0;
      virtual HRESULT __stdcall get_DesktopWidth (
        /*[out,retval]*/ long * pVal ) = 0;
      virtual HRESULT __stdcall put_DesktopHeight (
        /*[in]*/ long pVal ) = 0;
      virtual HRESULT __stdcall get_DesktopHeight (
        /*[out,retval]*/ long * pVal ) = 0;
      virtual HRESULT __stdcall put_StartConnected (
        /*[in]*/ long pfStartConnected ) = 0;
      virtual HRESULT __stdcall get_StartConnected (
        /*[out,retval]*/ long * pfStartConnected ) = 0;
      virtual HRESULT __stdcall get_HorizontalScrollBarVisible (
        /*[out,retval]*/ long * pfHScrollVisible ) = 0;
      virtual HRESULT __stdcall get_VerticalScrollBarVisible (
        /*[out,retval]*/ long * pfVScrollVisible ) = 0;
      virtual HRESULT __stdcall put_FullScreenTitle (
        /*[in]*/ BSTR _arg1 ) = 0;
      virtual HRESULT __stdcall get_CipherStrength (
        /*[out,retval]*/ long * pCipherStrength ) = 0;
      virtual HRESULT __stdcall get_Version (
        /*[out,retval]*/ BSTR * pVersion ) = 0;
      virtual HRESULT __stdcall get_SecuredSettingsEnabled (
        /*[out,retval]*/ long * pSecuredSettingsEnabled ) = 0;
      virtual HRESULT __stdcall get_SecuredSettings (
        /*[out,retval]*/ struct IMsTscSecuredSettings * * ppSecuredSettings ) = 0;
      virtual HRESULT __stdcall get_AdvancedSettings (
        /*[out,retval]*/ struct IMsTscAdvancedSettings * * ppAdvSettings ) = 0;
      virtual HRESULT __stdcall get_Debugger (
        /*[out,retval]*/ struct IMsTscDebug * * ppDebugger ) = 0;
      virtual HRESULT __stdcall Connect ( ) = 0;
      virtual HRESULT __stdcall Disconnect ( ) = 0;
      virtual HRESULT __stdcall CreateVirtualChannels (
        /*[in]*/ BSTR newVal ) = 0;
      virtual HRESULT __stdcall SendOnVirtualChannel (
        /*[in]*/ BSTR chanName,
        /*[in]*/ BSTR ChanData ) = 0;
};

struct __declspec(uuid("3c65b4ab-12b3-465b-acd4-b8dad3bff9e2"))
IMsRdpClientAdvancedSettings : IMsTscAdvancedSettings
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall put_SmoothScroll (
        /*[in]*/ long psmoothScroll ) = 0;
      virtual HRESULT __stdcall get_SmoothScroll (
        /*[out,retval]*/ long * psmoothScroll ) = 0;
      virtual HRESULT __stdcall put_AcceleratorPassthrough (
        /*[in]*/ long pacceleratorPassthrough ) = 0;
      virtual HRESULT __stdcall get_AcceleratorPassthrough (
        /*[out,retval]*/ long * pacceleratorPassthrough ) = 0;
      virtual HRESULT __stdcall put_ShadowBitmap (
        /*[in]*/ long pshadowBitmap ) = 0;
      virtual HRESULT __stdcall get_ShadowBitmap (
        /*[out,retval]*/ long * pshadowBitmap ) = 0;
      virtual HRESULT __stdcall put_TransportType (
        /*[in]*/ long ptransportType ) = 0;
      virtual HRESULT __stdcall get_TransportType (
        /*[out,retval]*/ long * ptransportType ) = 0;
      virtual HRESULT __stdcall put_SasSequence (
        /*[in]*/ long psasSequence ) = 0;
      virtual HRESULT __stdcall get_SasSequence (
        /*[out,retval]*/ long * psasSequence ) = 0;
      virtual HRESULT __stdcall put_EncryptionEnabled (
        /*[in]*/ long pencryptionEnabled ) = 0;
      virtual HRESULT __stdcall get_EncryptionEnabled (
        /*[out,retval]*/ long * pencryptionEnabled ) = 0;
      virtual HRESULT __stdcall put_DedicatedTerminal (
        /*[in]*/ long pdedicatedTerminal ) = 0;
      virtual HRESULT __stdcall get_DedicatedTerminal (
        /*[out,retval]*/ long * pdedicatedTerminal ) = 0;
      virtual HRESULT __stdcall put_RDPPort (
        /*[in]*/ long prdpPort ) = 0;
      virtual HRESULT __stdcall get_RDPPort (
        /*[out,retval]*/ long * prdpPort ) = 0;
      virtual HRESULT __stdcall put_EnableMouse (
        /*[in]*/ long penableMouse ) = 0;
      virtual HRESULT __stdcall get_EnableMouse (
        /*[out,retval]*/ long * penableMouse ) = 0;
      virtual HRESULT __stdcall put_DisableCtrlAltDel (
        /*[in]*/ long pdisableCtrlAltDel ) = 0;
      virtual HRESULT __stdcall get_DisableCtrlAltDel (
        /*[out,retval]*/ long * pdisableCtrlAltDel ) = 0;
      virtual HRESULT __stdcall put_EnableWindowsKey (
        /*[in]*/ long penableWindowsKey ) = 0;
      virtual HRESULT __stdcall get_EnableWindowsKey (
        /*[out,retval]*/ long * penableWindowsKey ) = 0;
      virtual HRESULT __stdcall put_DoubleClickDetect (
        /*[in]*/ long pdoubleClickDetect ) = 0;
      virtual HRESULT __stdcall get_DoubleClickDetect (
        /*[out,retval]*/ long * pdoubleClickDetect ) = 0;
      virtual HRESULT __stdcall put_MaximizeShell (
        /*[in]*/ long pmaximizeShell ) = 0;
      virtual HRESULT __stdcall get_MaximizeShell (
        /*[out,retval]*/ long * pmaximizeShell ) = 0;
      virtual HRESULT __stdcall put_HotKeyFullScreen (
        /*[in]*/ long photKeyFullScreen ) = 0;
      virtual HRESULT __stdcall get_HotKeyFullScreen (
        /*[out,retval]*/ long * photKeyFullScreen ) = 0;
      virtual HRESULT __stdcall put_HotKeyCtrlEsc (
        /*[in]*/ long photKeyCtrlEsc ) = 0;
      virtual HRESULT __stdcall get_HotKeyCtrlEsc (
        /*[out,retval]*/ long * photKeyCtrlEsc ) = 0;
      virtual HRESULT __stdcall put_HotKeyAltEsc (
        /*[in]*/ long photKeyAltEsc ) = 0;
      virtual HRESULT __stdcall get_HotKeyAltEsc (
        /*[out,retval]*/ long * photKeyAltEsc ) = 0;
      virtual HRESULT __stdcall put_HotKeyAltTab (
        /*[in]*/ long photKeyAltTab ) = 0;
      virtual HRESULT __stdcall get_HotKeyAltTab (
        /*[out,retval]*/ long * photKeyAltTab ) = 0;
      virtual HRESULT __stdcall put_HotKeyAltShiftTab (
        /*[in]*/ long photKeyAltShiftTab ) = 0;
      virtual HRESULT __stdcall get_HotKeyAltShiftTab (
        /*[out,retval]*/ long * photKeyAltShiftTab ) = 0;
      virtual HRESULT __stdcall put_HotKeyAltSpace (
        /*[in]*/ long photKeyAltSpace ) = 0;
      virtual HRESULT __stdcall get_HotKeyAltSpace (
        /*[out,retval]*/ long * photKeyAltSpace ) = 0;
      virtual HRESULT __stdcall put_HotKeyCtrlAltDel (
        /*[in]*/ long photKeyCtrlAltDel ) = 0;
      virtual HRESULT __stdcall get_HotKeyCtrlAltDel (
        /*[out,retval]*/ long * photKeyCtrlAltDel ) = 0;
      virtual HRESULT __stdcall put_orderDrawThreshold (
        /*[in]*/ long porderDrawThreshold ) = 0;
      virtual HRESULT __stdcall get_orderDrawThreshold (
        /*[out,retval]*/ long * porderDrawThreshold ) = 0;
      virtual HRESULT __stdcall put_BitmapCacheSize (
        /*[in]*/ long pbitmapCacheSize ) = 0;
      virtual HRESULT __stdcall get_BitmapCacheSize (
        /*[out,retval]*/ long * pbitmapCacheSize ) = 0;
      virtual HRESULT __stdcall put_BitmapVirtualCacheSize (
        /*[in]*/ long pbitmapVirtualCacheSize ) = 0;
      virtual HRESULT __stdcall get_BitmapVirtualCacheSize (
        /*[out,retval]*/ long * pbitmapVirtualCacheSize ) = 0;
      virtual HRESULT __stdcall put_ScaleBitmapCachesByBPP (
        /*[in]*/ long pbScale ) = 0;
      virtual HRESULT __stdcall get_ScaleBitmapCachesByBPP (
        /*[out,retval]*/ long * pbScale ) = 0;
      virtual HRESULT __stdcall put_NumBitmapCaches (
        /*[in]*/ long pnumBitmapCaches ) = 0;
      virtual HRESULT __stdcall get_NumBitmapCaches (
        /*[out,retval]*/ long * pnumBitmapCaches ) = 0;
      virtual HRESULT __stdcall put_CachePersistenceActive (
        /*[in]*/ long pcachePersistenceActive ) = 0;
      virtual HRESULT __stdcall get_CachePersistenceActive (
        /*[out,retval]*/ long * pcachePersistenceActive ) = 0;
      virtual HRESULT __stdcall put_PersistCacheDirectory (
        /*[in]*/ BSTR _arg1 ) = 0;
      virtual HRESULT __stdcall put_brushSupportLevel (
        /*[in]*/ long pbrushSupportLevel ) = 0;
      virtual HRESULT __stdcall get_brushSupportLevel (
        /*[out,retval]*/ long * pbrushSupportLevel ) = 0;
      virtual HRESULT __stdcall put_minInputSendInterval (
        /*[in]*/ long pminInputSendInterval ) = 0;
      virtual HRESULT __stdcall get_minInputSendInterval (
        /*[out,retval]*/ long * pminInputSendInterval ) = 0;
      virtual HRESULT __stdcall put_InputEventsAtOnce (
        /*[in]*/ long pinputEventsAtOnce ) = 0;
      virtual HRESULT __stdcall get_InputEventsAtOnce (
        /*[out,retval]*/ long * pinputEventsAtOnce ) = 0;
      virtual HRESULT __stdcall put_maxEventCount (
        /*[in]*/ long pmaxEventCount ) = 0;
      virtual HRESULT __stdcall get_maxEventCount (
        /*[out,retval]*/ long * pmaxEventCount ) = 0;
      virtual HRESULT __stdcall put_keepAliveInterval (
        /*[in]*/ long pkeepAliveInterval ) = 0;
      virtual HRESULT __stdcall get_keepAliveInterval (
        /*[out,retval]*/ long * pkeepAliveInterval ) = 0;
      virtual HRESULT __stdcall put_shutdownTimeout (
        /*[in]*/ long pshutdownTimeout ) = 0;
      virtual HRESULT __stdcall get_shutdownTimeout (
        /*[out,retval]*/ long * pshutdownTimeout ) = 0;
      virtual HRESULT __stdcall put_overallConnectionTimeout (
        /*[in]*/ long poverallConnectionTimeout ) = 0;
      virtual HRESULT __stdcall get_overallConnectionTimeout (
        /*[out,retval]*/ long * poverallConnectionTimeout ) = 0;
      virtual HRESULT __stdcall put_singleConnectionTimeout (
        /*[in]*/ long psingleConnectionTimeout ) = 0;
      virtual HRESULT __stdcall get_singleConnectionTimeout (
        /*[out,retval]*/ long * psingleConnectionTimeout ) = 0;
      virtual HRESULT __stdcall put_KeyboardType (
        /*[in]*/ long pkeyboardType ) = 0;
      virtual HRESULT __stdcall get_KeyboardType (
        /*[out,retval]*/ long * pkeyboardType ) = 0;
      virtual HRESULT __stdcall put_KeyboardSubType (
        /*[in]*/ long pkeyboardSubType ) = 0;
      virtual HRESULT __stdcall get_KeyboardSubType (
        /*[out,retval]*/ long * pkeyboardSubType ) = 0;
      virtual HRESULT __stdcall put_KeyboardFunctionKey (
        /*[in]*/ long pkeyboardFunctionKey ) = 0;
      virtual HRESULT __stdcall get_KeyboardFunctionKey (
        /*[out,retval]*/ long * pkeyboardFunctionKey ) = 0;
      virtual HRESULT __stdcall put_WinceFixedPalette (
        /*[in]*/ long pwinceFixedPalette ) = 0;
      virtual HRESULT __stdcall get_WinceFixedPalette (
        /*[out,retval]*/ long * pwinceFixedPalette ) = 0;
      virtual HRESULT __stdcall put_ConnectToServerConsole (
        /*[in]*/ VARIANT_BOOL pConnectToConsole ) = 0;
      virtual HRESULT __stdcall get_ConnectToServerConsole (
        /*[out,retval]*/ VARIANT_BOOL * pConnectToConsole ) = 0;
      virtual HRESULT __stdcall put_BitmapPersistence (
        /*[in]*/ long pbitmapPersistence ) = 0;
      virtual HRESULT __stdcall get_BitmapPersistence (
        /*[out,retval]*/ long * pbitmapPersistence ) = 0;
      virtual HRESULT __stdcall put_MinutesToIdleTimeout (
        /*[in]*/ long pminutesToIdleTimeout ) = 0;
      virtual HRESULT __stdcall get_MinutesToIdleTimeout (
        /*[out,retval]*/ long * pminutesToIdleTimeout ) = 0;
      virtual HRESULT __stdcall put_SmartSizing (
        /*[in]*/ VARIANT_BOOL pfSmartSizing ) = 0;
      virtual HRESULT __stdcall get_SmartSizing (
        /*[out,retval]*/ VARIANT_BOOL * pfSmartSizing ) = 0;
      virtual HRESULT __stdcall put_RdpdrLocalPrintingDocName (
        /*[in]*/ BSTR pLocalPrintingDocName ) = 0;
      virtual HRESULT __stdcall get_RdpdrLocalPrintingDocName (
        /*[out,retval]*/ BSTR * pLocalPrintingDocName ) = 0;
      virtual HRESULT __stdcall put_RdpdrClipCleanTempDirString (
        /*[in]*/ BSTR clipCleanTempDirString ) = 0;
      virtual HRESULT __stdcall get_RdpdrClipCleanTempDirString (
        /*[out,retval]*/ BSTR * clipCleanTempDirString ) = 0;
      virtual HRESULT __stdcall put_RdpdrClipPasteInfoString (
        /*[in]*/ BSTR clipPasteInfoString ) = 0;
      virtual HRESULT __stdcall get_RdpdrClipPasteInfoString (
        /*[out,retval]*/ BSTR * clipPasteInfoString ) = 0;
      virtual HRESULT __stdcall put_ClearTextPassword (
        /*[in]*/ BSTR _arg1 ) = 0;
      virtual HRESULT __stdcall put_DisplayConnectionBar (
        /*[in]*/ VARIANT_BOOL pDisplayConnectionBar ) = 0;
      virtual HRESULT __stdcall get_DisplayConnectionBar (
        /*[out,retval]*/ VARIANT_BOOL * pDisplayConnectionBar ) = 0;
      virtual HRESULT __stdcall put_PinConnectionBar (
        /*[in]*/ VARIANT_BOOL pPinConnectionBar ) = 0;
      virtual HRESULT __stdcall get_PinConnectionBar (
        /*[out,retval]*/ VARIANT_BOOL * pPinConnectionBar ) = 0;
      virtual HRESULT __stdcall put_GrabFocusOnConnect (
        /*[in]*/ VARIANT_BOOL pfGrabFocusOnConnect ) = 0;
      virtual HRESULT __stdcall get_GrabFocusOnConnect (
        /*[out,retval]*/ VARIANT_BOOL * pfGrabFocusOnConnect ) = 0;
      virtual HRESULT __stdcall put_LoadBalanceInfo (
        /*[in]*/ BSTR pLBInfo ) = 0;
      virtual HRESULT __stdcall get_LoadBalanceInfo (
        /*[out,retval]*/ BSTR * pLBInfo ) = 0;
      virtual HRESULT __stdcall put_RedirectDrives (
        /*[in]*/ VARIANT_BOOL pRedirectDrives ) = 0;
      virtual HRESULT __stdcall get_RedirectDrives (
        /*[out,retval]*/ VARIANT_BOOL * pRedirectDrives ) = 0;
      virtual HRESULT __stdcall put_RedirectPrinters (
        /*[in]*/ VARIANT_BOOL pRedirectPrinters ) = 0;
      virtual HRESULT __stdcall get_RedirectPrinters (
        /*[out,retval]*/ VARIANT_BOOL * pRedirectPrinters ) = 0;
      virtual HRESULT __stdcall put_RedirectPorts (
        /*[in]*/ VARIANT_BOOL pRedirectPorts ) = 0;
      virtual HRESULT __stdcall get_RedirectPorts (
        /*[out,retval]*/ VARIANT_BOOL * pRedirectPorts ) = 0;
      virtual HRESULT __stdcall put_RedirectSmartCards (
        /*[in]*/ VARIANT_BOOL pRedirectSmartCards ) = 0;
      virtual HRESULT __stdcall get_RedirectSmartCards (
        /*[out,retval]*/ VARIANT_BOOL * pRedirectSmartCards ) = 0;
      virtual HRESULT __stdcall put_BitmapVirtualCache16BppSize (
        /*[in]*/ long pBitmapVirtualCache16BppSize ) = 0;
      virtual HRESULT __stdcall get_BitmapVirtualCache16BppSize (
        /*[out,retval]*/ long * pBitmapVirtualCache16BppSize ) = 0;
      virtual HRESULT __stdcall put_BitmapVirtualCache24BppSize (
        /*[in]*/ long pBitmapVirtualCache24BppSize ) = 0;
      virtual HRESULT __stdcall get_BitmapVirtualCache24BppSize (
        /*[out,retval]*/ long * pBitmapVirtualCache24BppSize ) = 0;
      virtual HRESULT __stdcall put_PerformanceFlags (
        /*[in]*/ long pDisableList ) = 0;
      virtual HRESULT __stdcall get_PerformanceFlags (
        /*[out,retval]*/ long * pDisableList ) = 0;
      virtual HRESULT __stdcall put_ConnectWithEndpoint (
        /*[in]*/ VARIANT * _arg1 ) = 0;
      virtual HRESULT __stdcall put_NotifyTSPublicKey (
        /*[in]*/ VARIANT_BOOL pfNotify ) = 0;
      virtual HRESULT __stdcall get_NotifyTSPublicKey (
        /*[out,retval]*/ VARIANT_BOOL * pfNotify ) = 0;
};

struct __declspec(uuid("605befcf-39c1-45cc-a811-068fb7be346d"))
IMsRdpClientSecuredSettings : IMsTscSecuredSettings
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall put_KeyboardHookMode (
        /*[in]*/ long pkeyboardHookMode ) = 0;
      virtual HRESULT __stdcall get_KeyboardHookMode (
        /*[out,retval]*/ long * pkeyboardHookMode ) = 0;
      virtual HRESULT __stdcall put_AudioRedirectionMode (
        /*[in]*/ long pAudioRedirectionMode ) = 0;
      virtual HRESULT __stdcall get_AudioRedirectionMode (
        /*[out,retval]*/ long * pAudioRedirectionMode ) = 0;
};

enum __MIDL___MIDL_itf_mstsax_0000_0000_0001
{
    exDiscReasonNoInfo = 0,
    exDiscReasonAPIInitiatedDisconnect = 1,
    exDiscReasonAPIInitiatedLogoff = 2,
    exDiscReasonServerIdleTimeout = 3,
    exDiscReasonServerLogonTimeout = 4,
    exDiscReasonReplacedByOtherConnection = 5,
    exDiscReasonOutOfMemory = 6,
    exDiscReasonServerDeniedConnection = 7,
    exDiscReasonServerDeniedConnectionFips = 8,
    exDiscReasonServerInsufficientPrivileges = 9,
    exDiscReasonServerFreshCredsRequired = 10,
    exDiscReasonRpcInitiatedDisconnectByUser = 11,
    exDiscReasonLogoffByUser = 12,
    exDiscReasonLicenseInternal = 256,
    exDiscReasonLicenseNoLicenseServer = 257,
    exDiscReasonLicenseNoLicense = 258,
    exDiscReasonLicenseErrClientMsg = 259,
    exDiscReasonLicenseHwidDoesntMatchLicense = 260,
    exDiscReasonLicenseErrClientLicense = 261,
    exDiscReasonLicenseCantFinishProtocol = 262,
    exDiscReasonLicenseClientEndedProtocol = 263,
    exDiscReasonLicenseErrClientEncryption = 264,
    exDiscReasonLicenseCantUpgradeLicense = 265,
    exDiscReasonLicenseNoRemoteConnections = 266,
    exDiscReasonLicenseCreatingLicStoreAccDenied = 267,
    exDiscReasonRdpEncInvalidCredentials = 768,
    exDiscReasonProtocolRangeStart = 4096,
    exDiscReasonProtocolRangeEnd = 32767
};

struct __declspec(uuid("92b4a539-7115-4b7c-a5a9-e5d9efc2780a"))
IMsRdpClient : IMsTscAx
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall put_ColorDepth (
        /*[in]*/ long pcolorDepth ) = 0;
      virtual HRESULT __stdcall get_ColorDepth (
        /*[out,retval]*/ long * pcolorDepth ) = 0;
      virtual HRESULT __stdcall get_AdvancedSettings2 (
        /*[out,retval]*/ struct IMsRdpClientAdvancedSettings * * ppAdvSettings ) = 0;
      virtual HRESULT __stdcall get_SecuredSettings2 (
        /*[out,retval]*/ struct IMsRdpClientSecuredSettings * * ppSecuredSettings ) = 0;
      virtual HRESULT __stdcall get_ExtendedDisconnectReason (
        /*[out,retval]*/ ExtendedDisconnectReasonCode * pExtendedDisconnectReason ) = 0;
      virtual HRESULT __stdcall put_FullScreen (
        /*[in]*/ VARIANT_BOOL pfFullScreen ) = 0;
      virtual HRESULT __stdcall get_FullScreen (
        /*[out,retval]*/ VARIANT_BOOL * pfFullScreen ) = 0;
      virtual HRESULT __stdcall SetVirtualChannelOptions (
        /*[in]*/ BSTR chanName,
        /*[in]*/ long chanOptions ) = 0;
      virtual HRESULT __stdcall GetVirtualChannelOptions (
        /*[in]*/ BSTR chanName,
        /*[out,retval]*/ long * pChanOptions ) = 0;
      virtual HRESULT __stdcall RequestClose (
        /*[out,retval]*/ ControlCloseStatus * pCloseStatus ) = 0;
};

enum __MIDL_IMsRdpClient_0001
{
    controlCloseCanProceed = 0,
    controlCloseWaitForEvents = 1
};

struct __declspec(uuid("c1e6743a-41c1-4a74-832a-0dd06c1c7a0e"))
IMsTscNonScriptable : IUnknown
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall put_ClearTextPassword (
        /*[in]*/ BSTR _arg1 ) = 0;
      virtual HRESULT __stdcall put_PortablePassword (
        /*[in]*/ BSTR pPortablePass ) = 0;
      virtual HRESULT __stdcall get_PortablePassword (
        /*[out,retval]*/ BSTR * pPortablePass ) = 0;
      virtual HRESULT __stdcall put_PortableSalt (
        /*[in]*/ BSTR pPortableSalt ) = 0;
      virtual HRESULT __stdcall get_PortableSalt (
        /*[out,retval]*/ BSTR * pPortableSalt ) = 0;
      virtual HRESULT __stdcall put_BinaryPassword (
        /*[in]*/ BSTR pBinaryPassword ) = 0;
      virtual HRESULT __stdcall get_BinaryPassword (
        /*[out,retval]*/ BSTR * pBinaryPassword ) = 0;
      virtual HRESULT __stdcall put_BinarySalt (
        /*[in]*/ BSTR pSalt ) = 0;
      virtual HRESULT __stdcall get_BinarySalt (
        /*[out,retval]*/ BSTR * pSalt ) = 0;
      virtual HRESULT __stdcall ResetPassword ( ) = 0;
};

struct __declspec(uuid("2f079c4c-87b2-4afd-97ab-20cdb43038ae"))
IMsRdpClientNonScriptable : IMsTscNonScriptable
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall NotifyRedirectDeviceChange (
        /*[in]*/ UINT_PTR wParam,
        /*[in]*/ LONG_PTR lParam ) = 0;
      virtual HRESULT __stdcall SendKeys (
        /*[in]*/ long numKeys,
        /*[in]*/ VARIANT_BOOL * pbArrayKeyUp,
        /*[in]*/ long * plKeyData ) = 0;
};

struct __declspec(uuid("1fb464c8-09bb-4017-a2f5-eb742f04392f"))
MsTscAx;
    // interface IMsRdpClient
    // [ default ] interface IMsTscAx
    // interface IMsTscAx_Redist
    // [ default, source ] dispinterface IMsTscAxEvents
    // interface IMsTscNonScriptable
    // interface IMsRdpClientNonScriptable

struct __declspec(uuid("7cacbd7b-0d99-468f-ac33-22e495c0afe5"))
MsRdpClientNotSafeForScripting;
    // [ default ] interface IMsRdpClient
    // interface IMsTscAx
    // interface IMsTscAx_Redist
    // [ default, source ] dispinterface IMsTscAxEvents
    // interface IMsTscNonScriptable
    // interface IMsRdpClientNonScriptable

struct __declspec(uuid("791fa017-2de3-492e-acc5-53c67a2b94d0"))
MsRdpClient;
    // [ default ] interface IMsRdpClient
    // interface IMsTscAx
    // interface IMsTscAx_Redist
    // [ default, source ] dispinterface IMsTscAxEvents
    // interface IMsTscNonScriptable
    // interface IMsRdpClientNonScriptable

struct __declspec(uuid("3523c2fb-4031-44e4-9a3b-f1e94986ee7f"))
MsRdpClient2NotSafeForScripting;
    // [ default ] interface IMsRdpClient2
    // interface IMsRdpClient
    // interface IMsTscAx
    // interface IMsTscAx_Redist
    // [ default, source ] dispinterface IMsTscAxEvents
    // interface IMsTscNonScriptable
    // interface IMsRdpClientNonScriptable

struct __declspec(uuid("9ac42117-2b76-4320-aa44-0e616ab8437b"))
IMsRdpClientAdvancedSettings2 : IMsRdpClientAdvancedSettings
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall get_CanAutoReconnect (
        /*[out,retval]*/ VARIANT_BOOL * pfCanAutoReconnect ) = 0;
      virtual HRESULT __stdcall put_EnableAutoReconnect (
        /*[in]*/ VARIANT_BOOL pfEnableAutoReconnect ) = 0;
      virtual HRESULT __stdcall get_EnableAutoReconnect (
        /*[out,retval]*/ VARIANT_BOOL * pfEnableAutoReconnect ) = 0;
      virtual HRESULT __stdcall put_MaxReconnectAttempts (
        /*[in]*/ long pMaxReconnectAttempts ) = 0;
      virtual HRESULT __stdcall get_MaxReconnectAttempts (
        /*[out,retval]*/ long * pMaxReconnectAttempts ) = 0;
};

struct __declspec(uuid("e7e17dc4-3b71-4ba7-a8e6-281ffadca28f"))
IMsRdpClient2 : IMsRdpClient
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall get_AdvancedSettings3 (
        /*[out,retval]*/ struct IMsRdpClientAdvancedSettings2 * * ppAdvSettings ) = 0;
      virtual HRESULT __stdcall put_ConnectedStatusText (
        /*[in]*/ BSTR pConnectedStatusText ) = 0;
      virtual HRESULT __stdcall get_ConnectedStatusText (
        /*[out,retval]*/ BSTR * pConnectedStatusText ) = 0;
};

struct __declspec(uuid("9059f30f-4eb1-4bd2-9fdc-36f43a218f4a"))
MsRdpClient2;
    // [ default ] interface IMsRdpClient2
    // interface IMsRdpClient
    // interface IMsTscAx
    // interface IMsTscAx_Redist
    // [ default, source ] dispinterface IMsTscAxEvents
    // interface IMsTscNonScriptable
    // interface IMsRdpClientNonScriptable

struct __declspec(uuid("971127bb-259f-48c2-bd75-5f97a3331551"))
MsRdpClient2a;
    // [ default ] interface IMsRdpClient2
    // interface IMsRdpClient
    // interface IMsTscAx
    // interface IMsTscAx_Redist
    // [ default, source ] dispinterface IMsTscAxEvents
    // interface IMsTscNonScriptable
    // interface IMsRdpClientNonScriptable

struct __declspec(uuid("ace575fd-1fcf-4074-9401-ebab990fa9de"))
MsRdpClient3NotSafeForScripting;
    // [ default ] interface IMsRdpClient3
    // interface IMsRdpClient2
    // interface IMsRdpClient
    // interface IMsTscAx
    // interface IMsTscAx_Redist
    // [ default, source ] dispinterface IMsTscAxEvents
    // interface IMsTscNonScriptable
    // interface IMsRdpClientNonScriptable

struct __declspec(uuid("19cd856b-c542-4c53-acee-f127e3be1a59"))
IMsRdpClientAdvancedSettings3 : IMsRdpClientAdvancedSettings2
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall put_ConnectionBarShowMinimizeButton (
        /*[in]*/ VARIANT_BOOL pfShowMinimize ) = 0;
      virtual HRESULT __stdcall get_ConnectionBarShowMinimizeButton (
        /*[out,retval]*/ VARIANT_BOOL * pfShowMinimize ) = 0;
      virtual HRESULT __stdcall put_ConnectionBarShowRestoreButton (
        /*[in]*/ VARIANT_BOOL pfShowRestore ) = 0;
      virtual HRESULT __stdcall get_ConnectionBarShowRestoreButton (
        /*[out,retval]*/ VARIANT_BOOL * pfShowRestore ) = 0;
};

struct __declspec(uuid("91b7cbc5-a72e-4fa0-9300-d647d7e897ff"))
IMsRdpClient3 : IMsRdpClient2
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall get_AdvancedSettings4 (
        /*[out,retval]*/ struct IMsRdpClientAdvancedSettings3 * * ppAdvSettings ) = 0;
};

struct __declspec(uuid("7584c670-2274-4efb-b00b-d6aaba6d3850"))
MsRdpClient3;
    // [ default ] interface IMsRdpClient3
    // interface IMsRdpClient2
    // interface IMsRdpClient
    // interface IMsTscAx
    // interface IMsTscAx_Redist
    // [ default, source ] dispinterface IMsTscAxEvents
    // interface IMsTscNonScriptable
    // interface IMsRdpClientNonScriptable

struct __declspec(uuid("6a6f4b83-45c5-4ca9-bdd9-0d81c12295e4"))
MsRdpClient3a;
    // [ default ] interface IMsRdpClient3
    // interface IMsRdpClient2
    // interface IMsRdpClient
    // interface IMsTscAx
    // interface IMsTscAx_Redist
    // [ default, source ] dispinterface IMsTscAxEvents
    // interface IMsTscNonScriptable
    // interface IMsRdpClientNonScriptable

struct __declspec(uuid("6ae29350-321b-42be-bbe5-12fb5270c0de"))
MsRdpClient4NotSafeForScripting;
    // [ default ] interface IMsRdpClient4
    // interface IMsRdpClient3
    // interface IMsRdpClient2
    // interface IMsRdpClient
    // interface IMsTscAx
    // interface IMsTscAx_Redist
    // [ default, source ] dispinterface IMsTscAxEvents
    // interface IMsTscNonScriptable
    // interface IMsRdpClientNonScriptable
    // interface IMsRdpClientNonScriptable2

struct __declspec(uuid("fba7f64e-7345-4405-ae50-fa4a763dc0de"))
IMsRdpClientAdvancedSettings4 : IMsRdpClientAdvancedSettings3
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall put_AuthenticationLevel (
        /*[in]*/ unsigned int puiAuthLevel ) = 0;
      virtual HRESULT __stdcall get_AuthenticationLevel (
        /*[out,retval]*/ unsigned int * puiAuthLevel ) = 0;
};

struct __declspec(uuid("095e0738-d97d-488b-b9f6-dd0e8d66c0de"))
IMsRdpClient4 : IMsRdpClient3
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall get_AdvancedSettings5 (
        /*[out,retval]*/ struct IMsRdpClientAdvancedSettings4 * * ppAdvSettings ) = 0;
};

struct __declspec(uuid("17a5e535-4072-4fa4-af32-c8d0d47345e9"))
IMsRdpClientNonScriptable2 : IMsRdpClientNonScriptable
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall put_UIParentWindowHandle (
        /*[in]*/ wireHWND phwndUIParentWindowHandle ) = 0;
      virtual HRESULT __stdcall get_UIParentWindowHandle (
        /*[out,retval]*/ wireHWND * phwndUIParentWindowHandle ) = 0;
};

struct __declspec(uuid("4edcb26c-d24c-4e72-af07-b576699ac0de"))
MsRdpClient4;
    // [ default ] interface IMsRdpClient4
    // interface IMsRdpClient3
    // interface IMsRdpClient2
    // interface IMsRdpClient
    // interface IMsTscAx
    // interface IMsTscAx_Redist
    // [ default, source ] dispinterface IMsTscAxEvents
    // interface IMsTscNonScriptable
    // interface IMsRdpClientNonScriptable
    // interface IMsRdpClientNonScriptable2

struct __declspec(uuid("54ce37e0-9834-41ae-9896-4dab69dc022b"))
MsRdpClient4a;
    // [ default ] interface IMsRdpClient4
    // interface IMsRdpClient3
    // interface IMsRdpClient2
    // interface IMsRdpClient
    // interface IMsTscAx
    // interface IMsTscAx_Redist
    // [ default, source ] dispinterface IMsTscAxEvents
    // interface IMsTscNonScriptable
    // interface IMsRdpClientNonScriptable
    // interface IMsRdpClientNonScriptable2

struct __declspec(uuid("4eb2f086-c818-447e-b32c-c51ce2b30d31"))
MsRdpClient5NotSafeForScripting;
    // [ default ] interface IMsRdpClient5
    // interface IMsRdpClient4
    // interface IMsRdpClient3
    // interface IMsRdpClient2
    // interface IMsRdpClient
    // interface IMsTscAx
    // interface IMsTscAx_Redist
    // [ default, source ] dispinterface IMsTscAxEvents
    // interface IMsTscNonScriptable
    // interface IMsRdpClientNonScriptable
    // interface IMsRdpClientNonScriptable2
    // interface IMsRdpClientNonScriptable3

struct __declspec(uuid("720298c0-a099-46f5-9f82-96921bae4701"))
IMsRdpClientTransportSettings : IDispatch
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall put_GatewayHostname (
        /*[in]*/ BSTR pProxyHostname ) = 0;
      virtual HRESULT __stdcall get_GatewayHostname (
        /*[out,retval]*/ BSTR * pProxyHostname ) = 0;
      virtual HRESULT __stdcall put_GatewayUsageMethod (
        /*[in]*/ unsigned long pulProxyUsageMethod ) = 0;
      virtual HRESULT __stdcall get_GatewayUsageMethod (
        /*[out,retval]*/ unsigned long * pulProxyUsageMethod ) = 0;
      virtual HRESULT __stdcall put_GatewayProfileUsageMethod (
        /*[in]*/ unsigned long pulProxyProfileUsageMethod ) = 0;
      virtual HRESULT __stdcall get_GatewayProfileUsageMethod (
        /*[out,retval]*/ unsigned long * pulProxyProfileUsageMethod ) = 0;
      virtual HRESULT __stdcall put_GatewayCredsSource (
        /*[in]*/ unsigned long pulProxyCredsSource ) = 0;
      virtual HRESULT __stdcall get_GatewayCredsSource (
        /*[out,retval]*/ unsigned long * pulProxyCredsSource ) = 0;
      virtual HRESULT __stdcall put_GatewayUserSelectedCredsSource (
        /*[in]*/ unsigned long pulProxyCredsSource ) = 0;
      virtual HRESULT __stdcall get_GatewayUserSelectedCredsSource (
        /*[out,retval]*/ unsigned long * pulProxyCredsSource ) = 0;
      virtual HRESULT __stdcall get_GatewayIsSupported (
        /*[out,retval]*/ long * pfProxyIsSupported ) = 0;
      virtual HRESULT __stdcall get_GatewayDefaultUsageMethod (
        /*[out,retval]*/ unsigned long * pulProxyDefaultUsageMethod ) = 0;
};

struct __declspec(uuid("fba7f64e-6783-4405-da45-fa4a763dabd0"))
IMsRdpClientAdvancedSettings5 : IMsRdpClientAdvancedSettings4
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall put_RedirectClipboard (
        /*[in]*/ VARIANT_BOOL pfRedirectClipboard ) = 0;
      virtual HRESULT __stdcall get_RedirectClipboard (
        /*[out,retval]*/ VARIANT_BOOL * pfRedirectClipboard ) = 0;
      virtual HRESULT __stdcall put_AudioRedirectionMode (
        /*[in]*/ unsigned int puiAudioRedirectionMode ) = 0;
      virtual HRESULT __stdcall get_AudioRedirectionMode (
        /*[out,retval]*/ unsigned int * puiAudioRedirectionMode ) = 0;
      virtual HRESULT __stdcall put_ConnectionBarShowPinButton (
        /*[in]*/ VARIANT_BOOL pfShowPin ) = 0;
      virtual HRESULT __stdcall get_ConnectionBarShowPinButton (
        /*[out,retval]*/ VARIANT_BOOL * pfShowPin ) = 0;
      virtual HRESULT __stdcall put_PublicMode (
        /*[in]*/ VARIANT_BOOL pfPublicMode ) = 0;
      virtual HRESULT __stdcall get_PublicMode (
        /*[out,retval]*/ VARIANT_BOOL * pfPublicMode ) = 0;
      virtual HRESULT __stdcall put_RedirectDevices (
        /*[in]*/ VARIANT_BOOL pfRedirectPnPDevices ) = 0;
      virtual HRESULT __stdcall get_RedirectDevices (
        /*[out,retval]*/ VARIANT_BOOL * pfRedirectPnPDevices ) = 0;
      virtual HRESULT __stdcall put_RedirectPOSDevices (
        /*[in]*/ VARIANT_BOOL pfRedirectPOSDevices ) = 0;
      virtual HRESULT __stdcall get_RedirectPOSDevices (
        /*[out,retval]*/ VARIANT_BOOL * pfRedirectPOSDevices ) = 0;
      virtual HRESULT __stdcall put_BitmapVirtualCache32BppSize (
        /*[in]*/ long pBitmapVirtualCache32BppSize ) = 0;
      virtual HRESULT __stdcall get_BitmapVirtualCache32BppSize (
        /*[out,retval]*/ long * pBitmapVirtualCache32BppSize ) = 0;
};

struct __declspec(uuid("fdd029f9-467a-4c49-8529-64b521dbd1b4"))
ITSRemoteProgram : IDispatch
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall put_RemoteProgramMode (
        /*[in]*/ VARIANT_BOOL pvboolRemoteProgramMode ) = 0;
      virtual HRESULT __stdcall get_RemoteProgramMode (
        /*[out,retval]*/ VARIANT_BOOL * pvboolRemoteProgramMode ) = 0;
      virtual HRESULT __stdcall ServerStartProgram (
        /*[in]*/ BSTR bstrExecutablePath,
        /*[in]*/ BSTR bstrFilePath,
        /*[in]*/ BSTR bstrWorkingDirectory,
        /*[in]*/ VARIANT_BOOL vbExpandEnvVarInWorkingDirectoryOnServer,
        /*[in]*/ BSTR bstrArguments,
        /*[in]*/ VARIANT_BOOL vbExpandEnvVarInArgumentsOnServer ) = 0;
};

struct __declspec(uuid("d012ae6d-c19a-4bfe-b367-201f8911f134"))
IMsRdpClientShell : IDispatch
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall Launch ( ) = 0;
      virtual HRESULT __stdcall put_RdpFileContents (
        /*[in]*/ BSTR pszRdpFile ) = 0;
      virtual HRESULT __stdcall get_RdpFileContents (
        /*[out,retval]*/ BSTR * pszRdpFile ) = 0;
      virtual HRESULT __stdcall SetRdpProperty (
        /*[in]*/ BSTR szProperty,
        /*[in]*/ VARIANT Value ) = 0;
      virtual HRESULT __stdcall GetRdpProperty (
        /*[in]*/ BSTR szProperty,
        /*[out,retval]*/ VARIANT * pValue ) = 0;
      virtual HRESULT __stdcall get_IsRemoteProgramClientInstalled (
        /*[out,retval]*/ VARIANT_BOOL * pbClientInstalled ) = 0;
      virtual HRESULT __stdcall put_PublicMode (
        /*[in]*/ VARIANT_BOOL pfPublicMode ) = 0;
      virtual HRESULT __stdcall get_PublicMode (
        /*[out,retval]*/ VARIANT_BOOL * pfPublicMode ) = 0;
      virtual HRESULT __stdcall ShowTrustedSitesManagementDialog ( ) = 0;
};

struct __declspec(uuid("4eb5335b-6429-477d-b922-e06a28ecd8bf"))
IMsRdpClient5 : IMsRdpClient4
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall get_TransportSettings (
        /*[out,retval]*/ struct IMsRdpClientTransportSettings * * ppXportSet ) = 0;
      virtual HRESULT __stdcall get_AdvancedSettings6 (
        /*[out,retval]*/ struct IMsRdpClientAdvancedSettings5 * * ppAdvSettings ) = 0;
      virtual HRESULT __stdcall GetErrorDescription (
        /*[in]*/ unsigned int disconnectReason,
        /*[in]*/ unsigned int ExtendedDisconnectReason,
        /*[out,retval]*/ BSTR * pBstrErrorMsg ) = 0;
      virtual HRESULT __stdcall get_RemoteProgram (
        /*[out,retval]*/ struct ITSRemoteProgram * * ppRemoteProgram ) = 0;
      virtual HRESULT __stdcall get_MsRdpClientShell (
        /*[out,retval]*/ struct IMsRdpClientShell * * ppLauncher ) = 0;
};

struct __declspec(uuid("60c3b9c8-9e92-4f5e-a3e7-604a912093ea"))
IMsRdpDevice : IUnknown
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall get_DeviceInstanceId (
        /*[out,retval]*/ BSTR * pDevInstanceId ) = 0;
      virtual HRESULT __stdcall get_FriendlyName (
        /*[out,retval]*/ BSTR * pFriendlyName ) = 0;
      virtual HRESULT __stdcall get_DeviceDescription (
        /*[out,retval]*/ BSTR * pDeviceDescription ) = 0;
      virtual HRESULT __stdcall put_RedirectionState (
        /*[in]*/ VARIANT_BOOL pvboolRedirState ) = 0;
      virtual HRESULT __stdcall get_RedirectionState (
        /*[out,retval]*/ VARIANT_BOOL * pvboolRedirState ) = 0;
};

struct __declspec(uuid("56540617-d281-488c-8738-6a8fdf64a118"))
IMsRdpDeviceCollection : IUnknown
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall RescanDevices (
        /*[in]*/ VARIANT_BOOL vboolDynRedir ) = 0;
      virtual HRESULT __stdcall get_DeviceByIndex (
        /*[in]*/ unsigned long index,
        /*[out,retval]*/ struct IMsRdpDevice * * ppDevice ) = 0;
      virtual HRESULT __stdcall get_DeviceById (
        /*[in]*/ BSTR devInstanceId,
        /*[out,retval]*/ struct IMsRdpDevice * * ppDevice ) = 0;
      virtual HRESULT __stdcall get_DeviceCount (
        /*[out,retval]*/ unsigned long * pDeviceCount ) = 0;
};

struct __declspec(uuid("d28b5458-f694-47a8-8e61-40356a767e46"))
IMsRdpDrive : IUnknown
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall get_Name (
        /*[out,retval]*/ BSTR * pName ) = 0;
      virtual HRESULT __stdcall put_RedirectionState (
        /*[in]*/ VARIANT_BOOL pvboolRedirState ) = 0;
      virtual HRESULT __stdcall get_RedirectionState (
        /*[out,retval]*/ VARIANT_BOOL * pvboolRedirState ) = 0;
};

struct __declspec(uuid("7ff17599-da2c-4677-ad35-f60c04fe1585"))
IMsRdpDriveCollection : IUnknown
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall RescanDrives (
        VARIANT_BOOL vboolDynRedir ) = 0;
      virtual HRESULT __stdcall get_DriveByIndex (
        /*[in]*/ unsigned long index,
        /*[out,retval]*/ struct IMsRdpDrive * * ppDevice ) = 0;
      virtual HRESULT __stdcall get_DriveCount (
        /*[out,retval]*/ unsigned long * pDriveCount ) = 0;
};

struct __declspec(uuid("b3378d90-0728-45c7-8ed7-b6159fb92219"))
IMsRdpClientNonScriptable3 : IMsRdpClientNonScriptable2
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall put_ShowRedirectionWarningDialog (
        /*[in]*/ VARIANT_BOOL pfShowRdrDlg ) = 0;
      virtual HRESULT __stdcall get_ShowRedirectionWarningDialog (
        /*[out,retval]*/ VARIANT_BOOL * pfShowRdrDlg ) = 0;
      virtual HRESULT __stdcall put_PromptForCredentials (
        /*[in]*/ VARIANT_BOOL pfPrompt ) = 0;
      virtual HRESULT __stdcall get_PromptForCredentials (
        /*[out,retval]*/ VARIANT_BOOL * pfPrompt ) = 0;
      virtual HRESULT __stdcall put_NegotiateSecurityLayer (
        /*[in]*/ VARIANT_BOOL pfNegotiate ) = 0;
      virtual HRESULT __stdcall get_NegotiateSecurityLayer (
        /*[out,retval]*/ VARIANT_BOOL * pfNegotiate ) = 0;
      virtual HRESULT __stdcall put_EnableCredSspSupport (
        /*[in]*/ VARIANT_BOOL pfEnableSupport ) = 0;
      virtual HRESULT __stdcall get_EnableCredSspSupport (
        /*[out,retval]*/ VARIANT_BOOL * pfEnableSupport ) = 0;
      virtual HRESULT __stdcall put_RedirectDynamicDrives (
        /*[in]*/ VARIANT_BOOL pfRedirectDynamicDrives ) = 0;
      virtual HRESULT __stdcall get_RedirectDynamicDrives (
        /*[out,retval]*/ VARIANT_BOOL * pfRedirectDynamicDrives ) = 0;
      virtual HRESULT __stdcall put_RedirectDynamicDevices (
        /*[in]*/ VARIANT_BOOL pfRedirectDynamicDevices ) = 0;
      virtual HRESULT __stdcall get_RedirectDynamicDevices (
        /*[out,retval]*/ VARIANT_BOOL * pfRedirectDynamicDevices ) = 0;
      virtual HRESULT __stdcall get_DeviceCollection (
        /*[out,retval]*/ struct IMsRdpDeviceCollection * * ppDeviceCollection ) = 0;
      virtual HRESULT __stdcall get_DriveCollection (
        /*[out,retval]*/ struct IMsRdpDriveCollection * * ppDeviceCollection ) = 0;
      virtual HRESULT __stdcall put_WarnAboutSendingCredentials (
        /*[in]*/ VARIANT_BOOL pfWarn ) = 0;
      virtual HRESULT __stdcall get_WarnAboutSendingCredentials (
        /*[out,retval]*/ VARIANT_BOOL * pfWarn ) = 0;
      virtual HRESULT __stdcall put_WarnAboutClipboardRedirection (
        /*[in]*/ VARIANT_BOOL pfWarn ) = 0;
      virtual HRESULT __stdcall get_WarnAboutClipboardRedirection (
        /*[out,retval]*/ VARIANT_BOOL * pfWarn ) = 0;
      virtual HRESULT __stdcall put_ConnectionBarText (
        /*[in]*/ BSTR pConnectionBarText ) = 0;
      virtual HRESULT __stdcall get_ConnectionBarText (
        /*[out,retval]*/ BSTR * pConnectionBarText ) = 0;
};

struct __declspec(uuid("4eb89ff4-7f78-4a0f-8b8d-2bf02e94e4b2"))
MsRdpClient5;
    // [ default ] interface IMsRdpClient5
    // interface IMsRdpClient4
    // interface IMsRdpClient3
    // interface IMsRdpClient2
    // interface IMsRdpClient
    // interface IMsTscAx
    // interface IMsTscAx_Redist
    // [ default, source ] dispinterface IMsTscAxEvents
    // interface IMsTscNonScriptable
    // interface IMsRdpClientNonScriptable
    // interface IMsRdpClientNonScriptable2
    // interface IMsRdpClientNonScriptable3

struct __declspec(uuid("d2ea46a7-c2bf-426b-af24-e19c44456399"))
MsRdpClient6NotSafeForScripting;
    // [ default ] interface IMsRdpClient6
    // interface IMsRdpClient5
    // interface IMsRdpClient4
    // interface IMsRdpClient3
    // interface IMsRdpClient2
    // interface IMsRdpClient
    // interface IMsTscAx
    // interface IMsTscAx_Redist
    // [ default, source ] dispinterface IMsTscAxEvents
    // interface IMsTscNonScriptable
    // interface IMsRdpClientNonScriptable
    // interface IMsRdpClientNonScriptable2
    // interface IMsRdpClientNonScriptable3
    // interface IMsRdpClientNonScriptable4

struct __declspec(uuid("222c4b5d-45d9-4df0-a7c6-60cf9089d285"))
IMsRdpClientAdvancedSettings6 : IMsRdpClientAdvancedSettings5
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall put_RelativeMouseMode (
        /*[in]*/ VARIANT_BOOL pfRelativeMouseMode ) = 0;
      virtual HRESULT __stdcall get_RelativeMouseMode (
        /*[out,retval]*/ VARIANT_BOOL * pfRelativeMouseMode ) = 0;
      virtual HRESULT __stdcall get_AuthenticationServiceClass (
        /*[out,retval]*/ BSTR * pbstrAuthServiceClass ) = 0;
      virtual HRESULT __stdcall put_AuthenticationServiceClass (
        /*[in]*/ BSTR pbstrAuthServiceClass ) = 0;
      virtual HRESULT __stdcall get_PCB (
        /*[out,retval]*/ BSTR * bstrPCB ) = 0;
      virtual HRESULT __stdcall put_PCB (
        /*[in]*/ BSTR bstrPCB ) = 0;
      virtual HRESULT __stdcall put_HotKeyFocusReleaseLeft (
        /*[in]*/ long HotKeyFocusReleaseLeft ) = 0;
      virtual HRESULT __stdcall get_HotKeyFocusReleaseLeft (
        /*[out,retval]*/ long * HotKeyFocusReleaseLeft ) = 0;
      virtual HRESULT __stdcall put_HotKeyFocusReleaseRight (
        /*[in]*/ long HotKeyFocusReleaseRight ) = 0;
      virtual HRESULT __stdcall get_HotKeyFocusReleaseRight (
        /*[out,retval]*/ long * HotKeyFocusReleaseRight ) = 0;
      virtual HRESULT __stdcall put_EnableCredSspSupport (
        /*[in]*/ VARIANT_BOOL pfEnableSupport ) = 0;
      virtual HRESULT __stdcall get_EnableCredSspSupport (
        /*[out,retval]*/ VARIANT_BOOL * pfEnableSupport ) = 0;
      virtual HRESULT __stdcall get_AuthenticationType (
        /*[out,retval]*/ unsigned int * puiAuthType ) = 0;
      virtual HRESULT __stdcall put_ConnectToAdministerServer (
        /*[in]*/ VARIANT_BOOL pConnectToAdministerServer ) = 0;
      virtual HRESULT __stdcall get_ConnectToAdministerServer (
        /*[out,retval]*/ VARIANT_BOOL * pConnectToAdministerServer ) = 0;
};

struct __declspec(uuid("67341688-d606-4c73-a5d2-2e0489009319"))
IMsRdpClientTransportSettings2 : IMsRdpClientTransportSettings
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall put_GatewayCredSharing (
        /*[in]*/ unsigned long pulProxyCredSharing ) = 0;
      virtual HRESULT __stdcall get_GatewayCredSharing (
        /*[out,retval]*/ unsigned long * pulProxyCredSharing ) = 0;
      virtual HRESULT __stdcall put_GatewayPreAuthRequirement (
        /*[in]*/ unsigned long pulProxyPreAuthRequirement ) = 0;
      virtual HRESULT __stdcall get_GatewayPreAuthRequirement (
        /*[out,retval]*/ unsigned long * pulProxyPreAuthRequirement ) = 0;
      virtual HRESULT __stdcall put_GatewayPreAuthServerAddr (
        /*[in]*/ BSTR pbstrProxyPreAuthServerAddr ) = 0;
      virtual HRESULT __stdcall get_GatewayPreAuthServerAddr (
        /*[out,retval]*/ BSTR * pbstrProxyPreAuthServerAddr ) = 0;
      virtual HRESULT __stdcall put_GatewaySupportUrl (
        /*[in]*/ BSTR pbstrProxySupportUrl ) = 0;
      virtual HRESULT __stdcall get_GatewaySupportUrl (
        /*[out,retval]*/ BSTR * pbstrProxySupportUrl ) = 0;
      virtual HRESULT __stdcall put_GatewayEncryptedOtpCookie (
        /*[in]*/ BSTR pbstrEncryptedOtpCookie ) = 0;
      virtual HRESULT __stdcall get_GatewayEncryptedOtpCookie (
        /*[out,retval]*/ BSTR * pbstrEncryptedOtpCookie ) = 0;
      virtual HRESULT __stdcall put_GatewayEncryptedOtpCookieSize (
        /*[in]*/ unsigned long pulEncryptedOtpCookieSize ) = 0;
      virtual HRESULT __stdcall get_GatewayEncryptedOtpCookieSize (
        /*[out,retval]*/ unsigned long * pulEncryptedOtpCookieSize ) = 0;
      virtual HRESULT __stdcall put_GatewayUsername (
        /*[in]*/ BSTR pProxyUsername ) = 0;
      virtual HRESULT __stdcall get_GatewayUsername (
        /*[out,retval]*/ BSTR * pProxyUsername ) = 0;
      virtual HRESULT __stdcall put_GatewayDomain (
        /*[in]*/ BSTR pProxyDomain ) = 0;
      virtual HRESULT __stdcall get_GatewayDomain (
        /*[out,retval]*/ BSTR * pProxyDomain ) = 0;
      virtual HRESULT __stdcall put_GatewayPassword (
        /*[in]*/ BSTR _arg1 ) = 0;
};

struct __declspec(uuid("d43b7d80-8517-4b6d-9eac-96ad6800d7f2"))
IMsRdpClient6 : IMsRdpClient5
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall get_AdvancedSettings7 (
        /*[out,retval]*/ struct IMsRdpClientAdvancedSettings6 * * ppAdvSettings ) = 0;
      virtual HRESULT __stdcall get_TransportSettings2 (
        /*[out,retval]*/ struct IMsRdpClientTransportSettings2 * * ppXportSet2 ) = 0;
};

struct __declspec(uuid("f50fa8aa-1c7d-4f59-b15c-a90cacae1fcb"))
IMsRdpClientNonScriptable4 : IMsRdpClientNonScriptable3
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall put_RedirectionWarningType (
        /*[in]*/ RedirectionWarningType pWrnType ) = 0;
      virtual HRESULT __stdcall get_RedirectionWarningType (
        /*[out,retval]*/ RedirectionWarningType * pWrnType ) = 0;
      virtual HRESULT __stdcall put_MarkRdpSettingsSecure (
        /*[in]*/ VARIANT_BOOL pfRdpSecure ) = 0;
      virtual HRESULT __stdcall get_MarkRdpSettingsSecure (
        /*[out,retval]*/ VARIANT_BOOL * pfRdpSecure ) = 0;
      virtual HRESULT __stdcall put_PublisherCertificateChain (
        /*[in]*/ VARIANT * pVarCert ) = 0;
      virtual HRESULT __stdcall get_PublisherCertificateChain (
        /*[out,retval]*/ VARIANT * pVarCert ) = 0;
      virtual HRESULT __stdcall put_WarnAboutPrinterRedirection (
        /*[in]*/ VARIANT_BOOL pfWarn ) = 0;
      virtual HRESULT __stdcall get_WarnAboutPrinterRedirection (
        /*[out,retval]*/ VARIANT_BOOL * pfWarn ) = 0;
      virtual HRESULT __stdcall put_AllowCredentialSaving (
        /*[in]*/ VARIANT_BOOL pfAllowSave ) = 0;
      virtual HRESULT __stdcall get_AllowCredentialSaving (
        /*[out,retval]*/ VARIANT_BOOL * pfAllowSave ) = 0;
      virtual HRESULT __stdcall put_PromptForCredsOnClient (
        /*[in]*/ VARIANT_BOOL pfPromptForCredsOnClient ) = 0;
      virtual HRESULT __stdcall get_PromptForCredsOnClient (
        /*[out,retval]*/ VARIANT_BOOL * pfPromptForCredsOnClient ) = 0;
      virtual HRESULT __stdcall put_LaunchedViaClientShellInterface (
        /*[in]*/ VARIANT_BOOL pfLaunchedViaClientShellInterface ) = 0;
      virtual HRESULT __stdcall get_LaunchedViaClientShellInterface (
        /*[out,retval]*/ VARIANT_BOOL * pfLaunchedViaClientShellInterface ) = 0;
      virtual HRESULT __stdcall put_TrustedZoneSite (
        /*[in]*/ VARIANT_BOOL pfIsTrustedZone ) = 0;
      virtual HRESULT __stdcall get_TrustedZoneSite (
        /*[out,retval]*/ VARIANT_BOOL * pfIsTrustedZone ) = 0;
};

enum __MIDL_IMsRdpClientNonScriptable4_0001
{
    RedirectionWarningTypeDefault = 0,
    RedirectionWarningTypeUnsigned = 1,
    RedirectionWarningTypeUnknown = 2,
    RedirectionWarningTypeUser = 3,
    RedirectionWarningTypeThirdPartySigned = 4,
    RedirectionWarningTypeTrusted = 5,
    RedirectionWarningTypeMax = 5
};

struct __declspec(uuid("7390f3d8-0439-4c05-91e3-cf5cb290c3d0"))
MsRdpClient6;
    // [ default ] interface IMsRdpClient6
    // interface IMsRdpClient5
    // interface IMsRdpClient4
    // interface IMsRdpClient3
    // interface IMsRdpClient2
    // interface IMsRdpClient
    // interface IMsTscAx
    // interface IMsTscAx_Redist
    // [ default, source ] dispinterface IMsTscAxEvents
    // interface IMsTscNonScriptable
    // interface IMsRdpClientNonScriptable
    // interface IMsRdpClientNonScriptable2
    // interface IMsRdpClientNonScriptable3
    // interface IMsRdpClientNonScriptable4

struct __declspec(uuid("54d38bf7-b1ef-4479-9674-1bd6ea465258"))
MsRdpClient7NotSafeForScripting;
    // [ default ] interface IMsRdpClient7
    // interface IMsRdpClient6
    // interface IMsRdpClient5
    // interface IMsRdpClient4
    // interface IMsRdpClient3
    // interface IMsRdpClient2
    // interface IMsRdpClient
    // interface IMsTscAx
    // interface IMsTscAx_Redist
    // [ default, source ] dispinterface IMsTscAxEvents
    // interface IMsTscNonScriptable
    // interface IMsRdpClientNonScriptable
    // interface IMsRdpClientNonScriptable2
    // interface IMsRdpClientNonScriptable3
    // interface IMsRdpClientNonScriptable4
    // interface IMsRdpClientNonScriptable5
    // interface IMsRdpPreferredRedirectionInfo
    // interface IMsRdpExtendedSettings

struct __declspec(uuid("26036036-4010-4578-8091-0db9a1edf9c3"))
IMsRdpClientAdvancedSettings7 : IMsRdpClientAdvancedSettings6
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall put_AudioCaptureRedirectionMode (
        /*[in]*/ VARIANT_BOOL pfRedir ) = 0;
      virtual HRESULT __stdcall get_AudioCaptureRedirectionMode (
        /*[out,retval]*/ VARIANT_BOOL * pfRedir ) = 0;
      virtual HRESULT __stdcall put_VideoPlaybackMode (
        /*[in]*/ unsigned int pVideoPlaybackMode ) = 0;
      virtual HRESULT __stdcall get_VideoPlaybackMode (
        /*[out,retval]*/ unsigned int * pVideoPlaybackMode ) = 0;
      virtual HRESULT __stdcall put_EnableSuperPan (
        /*[in]*/ VARIANT_BOOL pfEnableSuperPan ) = 0;
      virtual HRESULT __stdcall get_EnableSuperPan (
        /*[out,retval]*/ VARIANT_BOOL * pfEnableSuperPan ) = 0;
      virtual HRESULT __stdcall put_SuperPanAccelerationFactor (
        /*[in]*/ unsigned long puAccelFactor ) = 0;
      virtual HRESULT __stdcall get_SuperPanAccelerationFactor (
        /*[out,retval]*/ unsigned long * puAccelFactor ) = 0;
      virtual HRESULT __stdcall put_NegotiateSecurityLayer (
        /*[in]*/ VARIANT_BOOL pfNegotiate ) = 0;
      virtual HRESULT __stdcall get_NegotiateSecurityLayer (
        /*[out,retval]*/ VARIANT_BOOL * pfNegotiate ) = 0;
      virtual HRESULT __stdcall put_AudioQualityMode (
        /*[in]*/ unsigned int pAudioQualityMode ) = 0;
      virtual HRESULT __stdcall get_AudioQualityMode (
        /*[out,retval]*/ unsigned int * pAudioQualityMode ) = 0;
      virtual HRESULT __stdcall put_RedirectDirectX (
        /*[in]*/ VARIANT_BOOL pfRedirectDirectX ) = 0;
      virtual HRESULT __stdcall get_RedirectDirectX (
        /*[out,retval]*/ VARIANT_BOOL * pfRedirectDirectX ) = 0;
      virtual HRESULT __stdcall put_NetworkConnectionType (
        /*[in]*/ unsigned int pConnectionType ) = 0;
      virtual HRESULT __stdcall get_NetworkConnectionType (
        /*[out,retval]*/ unsigned int * pConnectionType ) = 0;
};

struct __declspec(uuid("3d5b21ac-748d-41de-8f30-e15169586bd4"))
IMsRdpClientTransportSettings3 : IMsRdpClientTransportSettings2
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall put_GatewayCredSourceCookie (
        /*[in]*/ unsigned long pulProxyCredSourceCookie ) = 0;
      virtual HRESULT __stdcall get_GatewayCredSourceCookie (
        /*[out,retval]*/ unsigned long * pulProxyCredSourceCookie ) = 0;
      virtual HRESULT __stdcall put_GatewayAuthCookieServerAddr (
        /*[in]*/ BSTR pbstrProxyAuthCookieServerAddr ) = 0;
      virtual HRESULT __stdcall get_GatewayAuthCookieServerAddr (
        /*[out,retval]*/ BSTR * pbstrProxyAuthCookieServerAddr ) = 0;
      virtual HRESULT __stdcall put_GatewayEncryptedAuthCookie (
        /*[in]*/ BSTR pbstrEncryptedAuthCookie ) = 0;
      virtual HRESULT __stdcall get_GatewayEncryptedAuthCookie (
        /*[out,retval]*/ BSTR * pbstrEncryptedAuthCookie ) = 0;
      virtual HRESULT __stdcall put_GatewayEncryptedAuthCookieSize (
        /*[in]*/ unsigned long pulEncryptedAuthCookieSize ) = 0;
      virtual HRESULT __stdcall get_GatewayEncryptedAuthCookieSize (
        /*[out,retval]*/ unsigned long * pulEncryptedAuthCookieSize ) = 0;
      virtual HRESULT __stdcall put_GatewayAuthLoginPage (
        /*[in]*/ BSTR pbstrProxyAuthLoginPage ) = 0;
      virtual HRESULT __stdcall get_GatewayAuthLoginPage (
        /*[out,retval]*/ BSTR * pbstrProxyAuthLoginPage ) = 0;
};

struct __declspec(uuid("25f2ce20-8b1d-4971-a7cd-549dae201fc0"))
IMsRdpClientSecuredSettings2 : IMsRdpClientSecuredSettings
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall get_PCB (
        /*[out,retval]*/ BSTR * bstrPCB ) = 0;
      virtual HRESULT __stdcall put_PCB (
        /*[in]*/ BSTR bstrPCB ) = 0;
};

struct __declspec(uuid("92c38a7d-241a-418c-9936-099872c9af20"))
ITSRemoteProgram2 : ITSRemoteProgram
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall put_RemoteApplicationName (
        /*[in]*/ BSTR _arg1 ) = 0;
      virtual HRESULT __stdcall put_RemoteApplicationProgram (
        /*[in]*/ BSTR _arg1 ) = 0;
      virtual HRESULT __stdcall put_RemoteApplicationArgs (
        /*[in]*/ BSTR _arg1 ) = 0;
};

struct __declspec(uuid("b2a5b5ce-3461-444a-91d4-add26d070638"))
IMsRdpClient7 : IMsRdpClient6
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall get_AdvancedSettings8 (
        /*[out,retval]*/ struct IMsRdpClientAdvancedSettings7 * * ppAdvSettings ) = 0;
      virtual HRESULT __stdcall get_TransportSettings3 (
        /*[out,retval]*/ struct IMsRdpClientTransportSettings3 * * ppXportSet3 ) = 0;
      virtual HRESULT __stdcall GetStatusText (
        /*[in]*/ unsigned int statusCode,
        /*[out,retval]*/ BSTR * pBstrStatusText ) = 0;
      virtual HRESULT __stdcall get_SecuredSettings3 (
        /*[out,retval]*/ struct IMsRdpClientSecuredSettings2 * * ppSecuredSettings ) = 0;
      virtual HRESULT __stdcall get_RemoteProgram2 (
        /*[out,retval]*/ struct ITSRemoteProgram2 * * ppRemoteProgram ) = 0;
};

struct __declspec(uuid("4f6996d5-d7b1-412c-b0ff-063718566907"))
IMsRdpClientNonScriptable5 : IMsRdpClientNonScriptable4
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall put_UseMultimon (
        /*[in]*/ VARIANT_BOOL pfUseMultimon ) = 0;
      virtual HRESULT __stdcall get_UseMultimon (
        /*[out,retval]*/ VARIANT_BOOL * pfUseMultimon ) = 0;
      virtual HRESULT __stdcall get_RemoteMonitorCount (
        /*[out,retval]*/ unsigned long * pcRemoteMonitors ) = 0;
      virtual HRESULT __stdcall GetRemoteMonitorsBoundingBox (
        /*[out]*/ long * pLeft,
        /*[out]*/ long * pTop,
        /*[out]*/ long * pRight,
        /*[out]*/ long * pBottom ) = 0;
      virtual HRESULT __stdcall get_RemoteMonitorLayoutMatchesLocal (
        /*[out,retval]*/ VARIANT_BOOL * pfRemoteMatchesLocal ) = 0;
      virtual HRESULT __stdcall put_DisableConnectionBar (
        /*[in]*/ VARIANT_BOOL _arg1 ) = 0;
      virtual HRESULT __stdcall put_DisableRemoteAppCapsCheck (
        /*[in]*/ VARIANT_BOOL pfDisableRemoteAppCapsCheck ) = 0;
      virtual HRESULT __stdcall get_DisableRemoteAppCapsCheck (
        /*[out,retval]*/ VARIANT_BOOL * pfDisableRemoteAppCapsCheck ) = 0;
      virtual HRESULT __stdcall put_WarnAboutDirectXRedirection (
        /*[in]*/ VARIANT_BOOL pfWarn ) = 0;
      virtual HRESULT __stdcall get_WarnAboutDirectXRedirection (
        /*[out,retval]*/ VARIANT_BOOL * pfWarn ) = 0;
      virtual HRESULT __stdcall put_AllowPromptingForCredentials (
        /*[in]*/ VARIANT_BOOL pfAllow ) = 0;
      virtual HRESULT __stdcall get_AllowPromptingForCredentials (
        /*[out,retval]*/ VARIANT_BOOL * pfAllow ) = 0;
};

struct __declspec(uuid("fdd029f9-9574-4def-8529-64b521cccaa4"))
IMsRdpPreferredRedirectionInfo : IUnknown
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall put_UseRedirectionServerName (
        /*[in]*/ VARIANT_BOOL pVal ) = 0;
      virtual HRESULT __stdcall get_UseRedirectionServerName (
        /*[out,retval]*/ VARIANT_BOOL * pVal ) = 0;
};

struct __declspec(uuid("302d8188-0052-4807-806a-362b628f9ac5"))
IMsRdpExtendedSettings : IUnknown
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall put_Property (
        /*[in]*/ BSTR bstrPropertyName,
        /*[in]*/ VARIANT * pValue ) = 0;
      virtual HRESULT __stdcall get_Property (
        /*[in]*/ BSTR bstrPropertyName,
        /*[out,retval]*/ VARIANT * pValue ) = 0;
};

struct __declspec(uuid("a9d7038d-b5ed-472e-9c47-94bea90a5910"))
MsRdpClient7;
    // [ default ] interface IMsRdpClient7
    // interface IMsRdpClient6
    // interface IMsRdpClient5
    // interface IMsRdpClient4
    // interface IMsRdpClient3
    // interface IMsRdpClient2
    // interface IMsRdpClient
    // interface IMsTscAx
    // interface IMsTscAx_Redist
    // [ default, source ] dispinterface IMsTscAxEvents
    // interface IMsTscNonScriptable
    // interface IMsRdpClientNonScriptable
    // interface IMsRdpClientNonScriptable2
    // interface IMsRdpClientNonScriptable3
    // interface IMsRdpClientNonScriptable4
    // interface IMsRdpClientNonScriptable5
    // interface IMsRdpPreferredRedirectionInfo

struct __declspec(uuid("a3bc03a0-041d-42e3-ad22-882b7865c9c5"))
MsRdpClient8NotSafeForScripting;
    // [ default ] interface IMsRdpClient8
    // interface IMsRdpClient7
    // interface IMsRdpClient6
    // interface IMsRdpClient5
    // interface IMsRdpClient4
    // interface IMsRdpClient3
    // interface IMsRdpClient2
    // interface IMsRdpClient
    // interface IMsTscAx
    // interface IMsTscAx_Redist
    // [ default, source ] dispinterface IMsTscAxEvents
    // interface IMsTscNonScriptable
    // interface IMsRdpClientNonScriptable
    // interface IMsRdpClientNonScriptable2
    // interface IMsRdpClientNonScriptable3
    // interface IMsRdpClientNonScriptable4
    // interface IMsRdpClientNonScriptable5
    // interface IMsRdpPreferredRedirectionInfo
    // interface IMsRdpExtendedSettings

enum __MIDL___MIDL_itf_mstsax_0000_0000_0004
{
    RemoteSessionActionCharms = 0,
    RemoteSessionActionAppbar = 1,
    RemoteSessionActionSnap = 2,
    RemoteSessionActionStartScreen = 3,
    RemoteSessionActionAppSwitch = 4,
    RemoteSessionActionActionCenter = 5
};

struct __declspec(uuid("89acb528-2557-4d16-8625-226a30e97e9a"))
IMsRdpClientAdvancedSettings8 : IMsRdpClientAdvancedSettings7
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall put_BandwidthDetection (
        /*[in]*/ VARIANT_BOOL pfAutodetect ) = 0;
      virtual HRESULT __stdcall get_BandwidthDetection (
        /*[out,retval]*/ VARIANT_BOOL * pfAutodetect ) = 0;
      virtual HRESULT __stdcall put_ClientProtocolSpec (
        /*[in]*/ ClientSpec pClientMode ) = 0;
      virtual HRESULT __stdcall get_ClientProtocolSpec (
        /*[out,retval]*/ ClientSpec * pClientMode ) = 0;
};

enum __MIDL___MIDL_itf_mstsax_0000_0000_0003
{
    FullMode = 0,
    ThinClientMode = 1,
    SmallCacheMode = 2
};

struct __declspec(uuid("4247e044-9271-43a9-bc49-e2ad9e855d62"))
IMsRdpClient8 : IMsRdpClient7
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall SendRemoteAction (
        /*[in]*/ RemoteSessionActionType actionType ) = 0;
      virtual HRESULT __stdcall get_AdvancedSettings9 (
        /*[out,retval]*/ struct IMsRdpClientAdvancedSettings8 * * ppAdvSettings ) = 0;
      virtual HRESULT __stdcall Reconnect (
        /*[in]*/ unsigned long ulWidth,
        /*[in]*/ unsigned long ulHeight,
        /*[out,retval]*/ ControlReconnectStatus * pReconnectStatus ) = 0;
};

enum __MIDL_IMsRdpClient8_0001
{
    controlReconnectStarted = 0,
    controlReconnectBlocked = 1
};

struct __declspec(uuid("5f681803-2900-4c43-a1cc-cf405404a676"))
MsRdpClient8;
    // [ default ] interface IMsRdpClient8
    // interface IMsRdpClient7
    // interface IMsRdpClient6
    // interface IMsRdpClient5
    // interface IMsRdpClient4
    // interface IMsRdpClient3
    // interface IMsRdpClient2
    // interface IMsRdpClient
    // interface IMsTscAx
    // interface IMsTscAx_Redist
    // [ default, source ] dispinterface IMsTscAxEvents
    // interface IMsTscNonScriptable
    // interface IMsRdpClientNonScriptable
    // interface IMsRdpClientNonScriptable2
    // interface IMsRdpClientNonScriptable3
    // interface IMsRdpClientNonScriptable4
    // interface IMsRdpClientNonScriptable5
    // interface IMsRdpPreferredRedirectionInfo

struct __declspec(uuid("8b918b82-7985-4c24-89df-c33ad2bbfbcd"))
MsRdpClient9NotSafeForScripting;
    // [ default ] interface IMsRdpClient9
    // interface IMsRdpClient8
    // interface IMsRdpClient7
    // interface IMsRdpClient6
    // interface IMsRdpClient5
    // interface IMsRdpClient4
    // interface IMsRdpClient3
    // interface IMsRdpClient2
    // interface IMsRdpClient
    // interface IMsTscAx
    // interface IMsTscAx_Redist
    // [ default, source ] dispinterface IMsTscAxEvents
    // interface IMsTscNonScriptable
    // interface IMsRdpClientNonScriptable
    // interface IMsRdpClientNonScriptable2
    // interface IMsRdpClientNonScriptable3
    // interface IMsRdpClientNonScriptable4
    // interface IMsRdpClientNonScriptable5
    // interface IMsRdpPreferredRedirectionInfo
    // interface IMsRdpExtendedSettings

struct __declspec(uuid("011c3236-4d81-4515-9143-067ab630d299"))
IMsRdpClientTransportSettings4 : IMsRdpClientTransportSettings3
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall put_GatewayBrokeringType (
        /*[in]*/ unsigned long _arg1 ) = 0;
};

struct __declspec(uuid("28904001-04b6-436c-a55b-0af1a0883dc9"))
IMsRdpClient9 : IMsRdpClient8
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall get_TransportSettings4 (
        /*[out,retval]*/ struct IMsRdpClientTransportSettings4 * * ppXportSet4 ) = 0;
      virtual HRESULT __stdcall SyncSessionDisplaySettings ( ) = 0;
      virtual HRESULT __stdcall UpdateSessionDisplaySettings (
        /*[in]*/ unsigned long ulDesktopWidth,
        /*[in]*/ unsigned long ulDesktopHeight,
        /*[in]*/ unsigned long ulPhysicalWidth,
        /*[in]*/ unsigned long ulPhysicalHeight,
        /*[in]*/ unsigned long ulOrientation,
        /*[in]*/ unsigned long ulDesktopScaleFactor,
        /*[in]*/ unsigned long ulDeviceScaleFactor ) = 0;
      virtual HRESULT __stdcall attachEvent (
        /*[in]*/ BSTR eventName,
        /*[in]*/ IDispatch * callback ) = 0;
      virtual HRESULT __stdcall detachEvent (
        /*[in]*/ BSTR eventName,
        /*[in]*/ IDispatch * callback ) = 0;
};

struct __declspec(uuid("301b94ba-5d25-4a12-bffe-3b6e7a616585"))
MsRdpClient9;
    // [ default ] interface IMsRdpClient9
    // interface IMsRdpClient8
    // interface IMsRdpClient7
    // interface IMsRdpClient6
    // interface IMsRdpClient5
    // interface IMsRdpClient4
    // interface IMsRdpClient3
    // interface IMsRdpClient2
    // interface IMsRdpClient
    // interface IMsTscAx
    // interface IMsTscAx_Redist
    // [ default, source ] dispinterface IMsTscAxEvents
    // interface IMsTscNonScriptable
    // interface IMsRdpClientNonScriptable
    // interface IMsRdpClientNonScriptable2
    // interface IMsRdpClientNonScriptable3
    // interface IMsRdpClientNonScriptable4
    // interface IMsRdpClientNonScriptable5
    // interface IMsRdpPreferredRedirectionInfo

struct __declspec(uuid("079863b7-6d47-4105-8bfe-0cdcb360e67d"))
IRemoteDesktopClientEvents : IDispatch
{};

struct __declspec(uuid("eab16c5d-eed1-4e95-868b-0fba1b42c092"))
RemoteDesktopClient;
    // [ default ] interface IRemoteDesktopClient
    // [ default, source ] dispinterface IRemoteDesktopClientEvents

struct __declspec(uuid("48a0f2a7-2713-431f-bbac-6f4558e7d64d"))
IRemoteDesktopClientSettings : IDispatch
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall ApplySettings (
        /*[in]*/ BSTR RdpFileContents ) = 0;
      virtual HRESULT __stdcall RetrieveSettings (
        /*[out,retval]*/ BSTR * RdpFileContents ) = 0;
      virtual HRESULT __stdcall GetRdpProperty (
        /*[in]*/ BSTR propertyName,
        /*[out,retval]*/ VARIANT * Value ) = 0;
      virtual HRESULT __stdcall SetRdpProperty (
        /*[in]*/ BSTR propertyName,
        /*[in]*/ VARIANT Value ) = 0;
};

enum __MIDL_IRemoteDesktopClientActions_0001
{
    RemoteActionCharms = 0,
    RemoteActionAppbar = 1,
    RemoteActionSnap = 2,
    RemoteActionStartScreen = 3,
    RemoteActionAppSwitch = 4
};

enum __MIDL_IRemoteDesktopClientActions_0002
{
    SnapshotEncodingDataUri = 0
};

struct __declspec(uuid("7d54bc4e-1028-45d4-8b0a-b9b6bffba176"))
IRemoteDesktopClientActions : IDispatch
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall SuspendScreenUpdates ( ) = 0;
      virtual HRESULT __stdcall ResumeScreenUpdates ( ) = 0;
      virtual HRESULT __stdcall ExecuteRemoteAction (
        /*[in]*/ RemoteActionType remoteAction ) = 0;
      virtual HRESULT __stdcall GetSnapshot (
        /*[in]*/ SnapshotEncodingType snapshotEncoding,
        /*[in]*/ SnapshotFormatType snapshotFormat,
        /*[in]*/ unsigned long snapshotWidth,
        /*[in]*/ unsigned long snapshotHeight,
        /*[out,retval]*/ BSTR * snapshotData ) = 0;
};

enum __MIDL_IRemoteDesktopClientActions_0003
{
    SnapshotFormatPng = 0,
    SnapshotFormatJpeg = 1,
    SnapshotFormatBmp = 2
};

struct __declspec(uuid("260ec22d-8cbc-44b5-9e88-2a37f6c93ae9"))
IRemoteDesktopClientTouchPointer : IDispatch
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall put_Enabled (
        /*[in]*/ VARIANT_BOOL Enabled ) = 0;
      virtual HRESULT __stdcall get_Enabled (
        /*[out,retval]*/ VARIANT_BOOL * Enabled ) = 0;
      virtual HRESULT __stdcall put_EventsEnabled (
        /*[in]*/ VARIANT_BOOL EventsEnabled ) = 0;
      virtual HRESULT __stdcall get_EventsEnabled (
        /*[out,retval]*/ VARIANT_BOOL * EventsEnabled ) = 0;
      virtual HRESULT __stdcall put_PointerSpeed (
        /*[in]*/ unsigned long PointerSpeed ) = 0;
      virtual HRESULT __stdcall get_PointerSpeed (
        /*[out,retval]*/ unsigned long * PointerSpeed ) = 0;
};

struct __declspec(uuid("57d25668-625a-4905-be4e-304caa13f89c"))
IRemoteDesktopClient : IDispatch
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall Connect ( ) = 0;
      virtual HRESULT __stdcall Disconnect ( ) = 0;
      virtual HRESULT __stdcall Reconnect (
        /*[in]*/ unsigned long width,
        /*[in]*/ unsigned long height ) = 0;
      virtual HRESULT __stdcall get_Settings (
        /*[out,retval]*/ struct IRemoteDesktopClientSettings * * Settings ) = 0;
      virtual HRESULT __stdcall get_Actions (
        /*[out,retval]*/ struct IRemoteDesktopClientActions * * Actions ) = 0;
      virtual HRESULT __stdcall get_TouchPointer (
        /*[out,retval]*/ struct IRemoteDesktopClientTouchPointer * * TouchPointer ) = 0;
      virtual HRESULT __stdcall DeleteSavedCredentials (
        /*[in]*/ BSTR serverName ) = 0;
      virtual HRESULT __stdcall UpdateSessionDisplaySettings (
        /*[in]*/ unsigned long width,
        /*[in]*/ unsigned long height ) = 0;
      virtual HRESULT __stdcall attachEvent (
        /*[in]*/ BSTR eventName,
        /*[in]*/ IDispatch * callback ) = 0;
      virtual HRESULT __stdcall detachEvent (
        /*[in]*/ BSTR eventName,
        /*[in]*/ IDispatch * callback ) = 0;
};

} // namespace mstsc

#pragma pack(pop)
