// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// LogExts API Logging Manifest describing the interfaces of the Remote Desktop
// ActiveX control. All definitions were extracted from the type library
// embedded into mstscax.dll.
//
// To use this file copy it to winext\manifest directory of the WinDbg
// installation and add #include "rdp.h" to winext\manifest\main.h. Then load
// !logexts extension as usual and enable the "rdp" category.

module MSTSCAX.DLL:
category rdp:

#ifndef REMOTING_TOOLS_WINEXT_MANIFEST_RDP_H_
#define REMOTING_TOOLS_WINEXT_MANIFEST_RDP_H_

//
// GUIDs
//

struct __declspec(uuid("336d5562-efa8-482e-8cb3-c5c0fc7a7db6")) IMsTscAxEvents;
struct __declspec(uuid("92b4a539-7115-4b7c-a5a9-e5d9efc2780a")) IMsRdpClient;
struct __declspec(uuid("8c11efae-92c3-11d1-bc1e-00c04fa31489")) IMsTscAx;
struct __declspec(uuid("327bb5cd-834e-4400-aef2-b30e15e5d682")) IMsTscAx_Redist;
struct __declspec(uuid("c9d65442-a0f9-45b2-8f73-d61d2db8cbb6")) IMsTscSecuredSettings;
struct __declspec(uuid("809945cc-4b3b-4a92-a6b0-dbf9b5f2ef2d")) IMsTscAdvancedSettings;
struct __declspec(uuid("209d0eb9-6254-47b1-9033-a98dae55bb27")) IMsTscDebug;
struct __declspec(uuid("3c65b4ab-12b3-465b-acd4-b8dad3bff9e2")) IMsRdpClientAdvancedSettings;
struct __declspec(uuid("605befcf-39c1-45cc-a811-068fb7be346d")) IMsRdpClientSecuredSettings;
struct __declspec(uuid("c1e6743a-41c1-4a74-832a-0dd06c1c7a0e")) IMsTscNonScriptable;
struct __declspec(uuid("2f079c4c-87b2-4afd-97ab-20cdb43038ae")) IMsRdpClientNonScriptable;
struct __declspec(uuid("e7e17dc4-3b71-4ba7-a8e6-281ffadca28f")) IMsRdpClient2;
struct __declspec(uuid("9ac42117-2b76-4320-aa44-0e616ab8437b")) IMsRdpClientAdvancedSettings2;
struct __declspec(uuid("91b7cbc5-a72e-4fa0-9300-d647d7e897ff")) IMsRdpClient3;
struct __declspec(uuid("19cd856b-c542-4c53-acee-f127e3be1a59")) IMsRdpClientAdvancedSettings3;
struct __declspec(uuid("095e0738-d97d-488b-b9f6-dd0e8d66c0de")) IMsRdpClient4;
struct __declspec(uuid("fba7f64e-7345-4405-ae50-fa4a763dc0de")) IMsRdpClientAdvancedSettings4;
struct __declspec(uuid("17a5e535-4072-4fa4-af32-c8d0d47345e9")) IMsRdpClientNonScriptable2;
struct __declspec(uuid("4eb5335b-6429-477d-b922-e06a28ecd8bf")) IMsRdpClient5;
struct __declspec(uuid("720298c0-a099-46f5-9f82-96921bae4701")) IMsRdpClientTransportSettings;
struct __declspec(uuid("fba7f64e-6783-4405-da45-fa4a763dabd0")) IMsRdpClientAdvancedSettings5;
struct __declspec(uuid("fdd029f9-467a-4c49-8529-64b521dbd1b4")) ITSRemoteProgram;
struct __declspec(uuid("d012ae6d-c19a-4bfe-b367-201f8911f134")) IMsRdpClientShell;
struct __declspec(uuid("b3378d90-0728-45c7-8ed7-b6159fb92219")) IMsRdpClientNonScriptable3;
struct __declspec(uuid("56540617-d281-488c-8738-6a8fdf64a118")) IMsRdpDeviceCollection;
struct __declspec(uuid("60c3b9c8-9e92-4f5e-a3e7-604a912093ea")) IMsRdpDevice;
struct __declspec(uuid("7ff17599-da2c-4677-ad35-f60c04fe1585")) IMsRdpDriveCollection;
struct __declspec(uuid("d28b5458-f694-47a8-8e61-40356a767e46")) IMsRdpDrive;
struct __declspec(uuid("d43b7d80-8517-4b6d-9eac-96ad6800d7f2")) IMsRdpClient6;
struct __declspec(uuid("222c4b5d-45d9-4df0-a7c6-60cf9089d285")) IMsRdpClientAdvancedSettings6;
struct __declspec(uuid("67341688-d606-4c73-a5d2-2e0489009319")) IMsRdpClientTransportSettings2;
struct __declspec(uuid("f50fa8aa-1c7d-4f59-b15c-a90cacae1fcb")) IMsRdpClientNonScriptable4;
struct __declspec(uuid("b2a5b5ce-3461-444a-91d4-add26d070638")) IMsRdpClient7;
struct __declspec(uuid("26036036-4010-4578-8091-0db9a1edf9c3")) IMsRdpClientAdvancedSettings7;
struct __declspec(uuid("3d5b21ac-748d-41de-8f30-e15169586bd4")) IMsRdpClientTransportSettings3;
struct __declspec(uuid("25f2ce20-8b1d-4971-a7cd-549dae201fc0")) IMsRdpClientSecuredSettings2;
struct __declspec(uuid("92c38a7d-241a-418c-9936-099872c9af20")) ITSRemoteProgram2;
struct __declspec(uuid("4f6996d5-d7b1-412c-b0ff-063718566907")) IMsRdpClientNonScriptable5;
struct __declspec(uuid("fdd029f9-9574-4def-8529-64b521cccaa4")) IMsRdpPreferredRedirectionInfo;
struct __declspec(uuid("302d8188-0052-4807-806a-362b628f9ac5")) IMsRdpExtendedSettings;
struct __declspec(uuid("4247e044-9271-43a9-bc49-e2ad9e855d62")) IMsRdpClient8;
struct __declspec(uuid("89acb528-2557-4d16-8625-226a30e97e9a")) IMsRdpClientAdvancedSettings8;
struct __declspec(uuid("079863b7-6d47-4105-8bfe-0cdcb360e67d")) IRemoteDesktopClientEvents;
struct __declspec(uuid("57d25668-625a-4905-be4e-304caa13f89c")) IRemoteDesktopClient;
struct __declspec(uuid("48a0f2a7-2713-431f-bbac-6f4558e7d64d")) IRemoteDesktopClientSettings;
struct __declspec(uuid("7d54bc4e-1028-45d4-8b0a-b9b6bffba176")) IRemoteDesktopClientActions;
struct __declspec(uuid("260ec22d-8cbc-44b5-9e88-2a37f6c93ae9")) IRemoteDesktopClientTouchPointer;
struct __declspec(uuid("A0B2DD9A-7F53-4E65-8547-851952EC8C96")) IMsRdpSessionManager;

//
// Typedefs
//

typedef LPVOID LONG_PTR;

typedef struct tagSAFEARRAYBOUND {
  ULONG cElements;
  LONG lLbound;
} SAFEARRAYBOUND;

typedef struct tagSAFEARRAY {
  USHORT         cDims;
  USHORT         fFeatures;
  ULONG          cbElements;
  ULONG          cLocks;
  PVOID          pvData;
  SAFEARRAYBOUND rgsabound[1];
} SAFEARRAY;

//
// Values
//

value short  VARIANT_BOOL {
#define VARIANT_TRUE 0xffff
#define VARIANT_FALSE 0
};

value long AutoReconnectContinueState {
#define autoReconnectContinueAutomatic 0
#define autoReconnectContinueStop 1
#define autoReconnectContinueManual 2
};

value long RemoteWindowDisplayedAttribute {
#define remoteAppWindowNone 0
#define remoteAppWindowDisplayed 1
#define remoteAppShellIconDisplayed 2
};

value long RemoteProgramResult {
#define remoteAppResultOk 0
#define remoteAppResultLocked 1
#define remoteAppResultProtocolError 2
#define remoteAppResultNotInWhitelist 3
#define remoteAppResultNetworkPathDenied 4
#define remoteAppResultFileNotFound 5
#define remoteAppResultFailure 6
#define remoteAppResultHookNotLoaded 7
};

value long ExtendedDisconnectReasonCode {
#define exDiscReasonNoInfo 0
#define exDiscReasonAPIInitiatedDisconnect 1
#define exDiscReasonAPIInitiatedLogoff 2
#define exDiscReasonServerIdleTimeout 3
#define exDiscReasonServerLogonTimeout 4
#define exDiscReasonReplacedByOtherConnection 5
#define exDiscReasonOutOfMemory 6
#define exDiscReasonServerDeniedConnection 7
#define exDiscReasonServerDeniedConnectionFips 8
#define exDiscReasonServerInsufficientPrivileges 9
#define exDiscReasonServerFreshCredsRequired 10
#define exDiscReasonLicenseInternal 256
#define exDiscReasonLicenseNoLicenseServer 257
#define exDiscReasonLicenseNoLicense 258
#define exDiscReasonLicenseErrClientMsg 259
#define exDiscReasonLicenseHwidDoesntMatchLicense 260
#define exDiscReasonLicenseErrClientLicense 261
#define exDiscReasonLicenseCantFinishProtocol 262
#define exDiscReasonLicenseClientEndedProtocol 263
#define exDiscReasonLicenseErrClientEncryption 264
#define exDiscReasonLicenseCantUpgradeLicense 265
#define exDiscReasonLicenseNoRemoteConnections 266
#define exDiscReasonLicenseCreatingLicStoreAccDenied 267
#define exDiscReasonRdpEncInvalidCredentials 768
#define exDiscReasonProtocolRangeStart 4096
#define exDiscReasonProtocolRangeEnd 32767
};

value long ControlCloseStatus {
#define controlCloseCanProceed 0
#define controlCloseWaitForEvents 1
};

value long RedirectionWarningType {
#define RedirectionWarningTypeDefault 0
#define RedirectionWarningTypeUnsigned 1
#define RedirectionWarningTypeUnknown 2
#define RedirectionWarningTypeUser 3
#define RedirectionWarningTypeThirdPartySigned 4
#define RedirectionWarningTypeTrusted 5
};

value long RemoteSessionActionType {
#define RemoteSessionActionCharms 0
#define RemoteSessionActionAppbar 1
#define RemoteSessionActionSnap 2
#define RemoteSessionActionStartScreen 3
#define RemoteSessionActionAppSwitch 4
};

value long ClientSpec {
#define FullMode 0
#define ThinClientMode 1
#define SmallCacheMode 2
};

value long ControlReconnectStatus {
#define controlReconnectStarted 0
#define controlReconnectBlocked 1
};

value long RemoteActionType {
#define RemoteActionCharms 0
#define RemoteActionAppbar 1
#define RemoteActionSnap 2
#define RemoteActionStartScreen 3
#define RemoteActionAppSwitch 4
};

value long SnapshotEncodingType {
#define SnapshotEncodingDataUri 0
};

value long SnapshotFormatType {
#define SnapshotFormatPng 0
#define SnapshotFormatJpeg 1
#define SnapshotFormatBmp 2
};

//
// Interfaces
//

interface IMsTscAxEvents : IDispatch {
  HRESULT OnConnecting();
  HRESULT OnConnected();
  HRESULT OnLoginComplete();
  HRESULT OnDisconnected(long discReason);
  HRESULT OnEnterFullScreenMode();
  HRESULT OnLeaveFullScreenMode();
  HRESULT OnChannelReceivedData(BSTR chanName, BSTR data);
  HRESULT OnRequestGoFullScreen();
  HRESULT OnRequestLeaveFullScreen();
  HRESULT OnFatalError(long errorCode);
  HRESULT OnWarning(long warningCode);
  HRESULT OnRemoteDesktopSizeChange(long width, long height);
  HRESULT OnIdleTimeoutNotification();
  HRESULT OnRequestContainerMinimize();
  HRESULT OnConfirmClose([out] VARIANT_BOOL* pfAllowClose);
  HRESULT OnReceivedTSPublicKey(BSTR publicKey, [out] VARIANT_BOOL* pfContinueLogon);
  HRESULT OnAutoReconnecting(long disconnectReason, long attemptCount, [out] AutoReconnectContinueState* pArcContinueStatus);
  HRESULT OnAuthenticationWarningDisplayed();
  HRESULT OnAuthenticationWarningDismissed();
  HRESULT OnRemoteProgramResult(BSTR bstrRemoteProgram, RemoteProgramResult lError, VARIANT_BOOL vbIsExecutable);
  HRESULT OnRemoteProgramDisplayed(VARIANT_BOOL vbDisplayed, ULONG uDisplayInformation);
  HRESULT OnRemoteWindowDisplayed(VARIANT_BOOL vbDisplayed, LPVOID hwnd, RemoteWindowDisplayedAttribute windowAttribute);
  HRESULT OnLogonError(long lError);
  HRESULT OnFocusReleased(int iDirection);
  HRESULT OnUserNameAcquired(BSTR bstrUserName);
  HRESULT OnMouseInputModeChanged(VARIANT_BOOL fMouseModeRelative);
  HRESULT OnServiceMessageReceived(BSTR serviceMessage);
  HRESULT OnConnectionBarPullDown();
  HRESULT OnNetworkStatusChanged(ULONG qualityLevel, long bandwidth, long rtt);
  HRESULT OnDevicesButtonPressed();
  HRESULT OnAutoReconnected();
  HRESULT OnAutoReconnecting2(long disconnectReason, VARIANT_BOOL networkAvailable, long attemptCount, long maxAttemptCount);
};

interface IMsTscSecuredSettings : IDispatch {
    HRESULT put_StartProgram(BSTR pStartProgram);
    HRESULT get_StartProgram([out] BSTR* pStartProgram);
    HRESULT put_WorkDir(BSTR pWorkDir);
    HRESULT get_WorkDir([out] BSTR* pWorkDir);
    HRESULT put_FullScreen(long pfFullScreen);
    HRESULT get_FullScreen([out] long* pfFullScreen);
};

interface IMsTscAdvancedSettings : IDispatch {
  HRESULT put_Compress(long pcompress);
  HRESULT get_Compress([out] long* pcompress);
  HRESULT put_BitmapPeristence(long pbitmapPeristence);
  HRESULT get_BitmapPeristence([out] long* pbitmapPeristence);
  HRESULT put_allowBackgroundInput(long pallowBackgroundInput);
  HRESULT get_allowBackgroundInput([out] long* pallowBackgroundInput);
  HRESULT put_KeyBoardLayoutStr(BSTR _arg1);
  HRESULT put_PluginDlls(BSTR _arg1);
  HRESULT put_IconFile(BSTR _arg1);
  HRESULT put_IconIndex(long _arg1);
  HRESULT put_ContainerHandledFullScreen(long pContainerHandledFullScreen);
  HRESULT get_ContainerHandledFullScreen([out] long* pContainerHandledFullScreen);
  HRESULT put_DisableRdpdr(long pDisableRdpdr);
  HRESULT get_DisableRdpdr([out] long* pDisableRdpdr);
};

interface IMsTscDebug : IDispatch {
  HRESULT put_HatchBitmapPDU(long phatchBitmapPDU);
  HRESULT get_HatchBitmapPDU([out] long* phatchBitmapPDU);
  HRESULT put_HatchSSBOrder(long phatchSSBOrder);
  HRESULT get_HatchSSBOrder([out] long* phatchSSBOrder);
  HRESULT put_HatchMembltOrder(long phatchMembltOrder);
  HRESULT get_HatchMembltOrder([out] long* phatchMembltOrder);
  HRESULT put_HatchIndexPDU(long phatchIndexPDU);
  HRESULT get_HatchIndexPDU([out] long* phatchIndexPDU);
  HRESULT put_LabelMemblt(long plabelMemblt);
  HRESULT get_LabelMemblt([out] long* plabelMemblt);
  HRESULT put_BitmapCacheMonitor(long pbitmapCacheMonitor);
  HRESULT get_BitmapCacheMonitor([out] long* pbitmapCacheMonitor);
  HRESULT put_MallocFailuresPercent(long pmallocFailuresPercent);
  HRESULT get_MallocFailuresPercent([out] long* pmallocFailuresPercent);
  HRESULT put_MallocHugeFailuresPercent(long pmallocHugeFailuresPercent);
  HRESULT get_MallocHugeFailuresPercent([out] long* pmallocHugeFailuresPercent);
  HRESULT put_NetThroughput(long NetThroughput);
  HRESULT get_NetThroughput([out] long* NetThroughput);
  HRESULT put_CLXCmdLine(BSTR pCLXCmdLine);
  HRESULT get_CLXCmdLine([out] BSTR* pCLXCmdLine);
  HRESULT put_CLXDll(BSTR pCLXDll);
  HRESULT get_CLXDll([out] BSTR* pCLXDll);
  HRESULT put_RemoteProgramsHatchVisibleRegion(long pcbHatch);
  HRESULT get_RemoteProgramsHatchVisibleRegion([out] long* pcbHatch);
  HRESULT put_RemoteProgramsHatchVisibleNoDataRegion(long pcbHatch);
  HRESULT get_RemoteProgramsHatchVisibleNoDataRegion([out] long* pcbHatch);
  HRESULT put_RemoteProgramsHatchNonVisibleRegion(long pcbHatch);
  HRESULT get_RemoteProgramsHatchNonVisibleRegion([out] long* pcbHatch);
  HRESULT put_RemoteProgramsHatchWindow(long pcbHatch);
  HRESULT get_RemoteProgramsHatchWindow([out] long* pcbHatch);
  HRESULT put_RemoteProgramsStayConnectOnBadCaps(long pcbStayConnected);
  HRESULT get_RemoteProgramsStayConnectOnBadCaps([out] long* pcbStayConnected);
  HRESULT get_ControlType([out] UINT* pControlType);
  HRESULT put_DecodeGfx(VARIANT_BOOL _arg1);
};

interface IMsTscAx : IDispatch
{
  HRESULT put_Server(BSTR pServer);
  HRESULT get_Server([out] BSTR* pServer);
  HRESULT put_Domain(BSTR pDomain);
  HRESULT get_Domain([out] BSTR* pDomain);
  HRESULT put_UserName(BSTR pUserName);
  HRESULT get_UserName([out] BSTR* pUserName);
  HRESULT put_DisconnectedText(BSTR pDisconnectedText);
  HRESULT get_DisconnectedText([out] BSTR* pDisconnectedText);
  HRESULT put_ConnectingText(BSTR pConnectingText);
  HRESULT get_ConnectingText([out] BSTR* pConnectingText);
  HRESULT get_Connected([out] short* pIsConnected);
  HRESULT put_DesktopWidth(long pVal);
  HRESULT get_DesktopWidth([out] long* pVal);
  HRESULT put_DesktopHeight(long pVal);
  HRESULT get_DesktopHeight([out] long* pVal);
  HRESULT put_StartConnected(long pfStartConnected);
  HRESULT get_StartConnected([out] long* pfStartConnected);
  HRESULT get_HorizontalScrollBarVisible([out] long* pfHScrollVisible);
  HRESULT get_VerticalScrollBarVisible([out] long* pfVScrollVisible);
  HRESULT put_FullScreenTitle(BSTR _arg1);
  HRESULT get_CipherStrength([out] long* pCipherStrength);
  HRESULT get_Version([out] BSTR* pVersion);
  HRESULT get_SecuredSettingsEnabled([out] long* pSecuredSettingsEnabled);
  HRESULT get_SecuredSettings([out] IMsTscSecuredSettings** ppSecuredSettings);
  HRESULT get_AdvancedSettings([out] IMsTscAdvancedSettings** ppAdvSettings);
  HRESULT get_Debugger([out] IMsTscDebug** ppDebugger);
  HRESULT Connect();
  HRESULT Disconnect();
  HRESULT CreateVirtualChannels(BSTR newVal);
  HRESULT SendOnVirtualChannel(BSTR chanName, BSTR ChanData);
};

interface IMsRdpClientAdvancedSettings : IMsTscAdvancedSettings {
  HRESULT put_SmoothScroll(long psmoothScroll);
  HRESULT get_SmoothScroll([out] long* psmoothScroll);
  HRESULT put_AcceleratorPassthrough(long pacceleratorPassthrough);
  HRESULT get_AcceleratorPassthrough([out] long* pacceleratorPassthrough);
  HRESULT put_ShadowBitmap(long pshadowBitmap);
  HRESULT get_ShadowBitmap([out] long* pshadowBitmap);
  HRESULT put_TransportType(long ptransportType);
  HRESULT get_TransportType([out] long* ptransportType);
  HRESULT put_SasSequence(long psasSequence);
  HRESULT get_SasSequence([out] long* psasSequence);
  HRESULT put_EncryptionEnabled(long pencryptionEnabled);
  HRESULT get_EncryptionEnabled([out] long* pencryptionEnabled);
  HRESULT put_DedicatedTerminal(long pdedicatedTerminal);
  HRESULT get_DedicatedTerminal([out] long* pdedicatedTerminal);
  HRESULT put_RDPPort(long prdpPort);
  HRESULT get_RDPPort([out] long* prdpPort);
  HRESULT put_EnableMouse(long penableMouse);
  HRESULT get_EnableMouse([out] long* penableMouse);
  HRESULT put_DisableCtrlAltDel(long pdisableCtrlAltDel);
  HRESULT get_DisableCtrlAltDel([out] long* pdisableCtrlAltDel);
  HRESULT put_EnableWindowsKey(long penableWindowsKey);
  HRESULT get_EnableWindowsKey([out] long* penableWindowsKey);
  HRESULT put_DoubleClickDetect(long pdoubleClickDetect);
  HRESULT get_DoubleClickDetect([out] long* pdoubleClickDetect);
  HRESULT put_MaximizeShell(long pmaximizeShell);
  HRESULT get_MaximizeShell([out] long* pmaximizeShell);
  HRESULT put_HotKeyFullScreen(long photKeyFullScreen);
  HRESULT get_HotKeyFullScreen([out] long* photKeyFullScreen);
  HRESULT put_HotKeyCtrlEsc(long photKeyCtrlEsc);
  HRESULT get_HotKeyCtrlEsc([out] long* photKeyCtrlEsc);
  HRESULT put_HotKeyAltEsc(long photKeyAltEsc);
  HRESULT get_HotKeyAltEsc([out] long* photKeyAltEsc);
  HRESULT put_HotKeyAltTab(long photKeyAltTab);
  HRESULT get_HotKeyAltTab([out] long* photKeyAltTab);
  HRESULT put_HotKeyAltShiftTab(long photKeyAltShiftTab);
  HRESULT get_HotKeyAltShiftTab([out] long* photKeyAltShiftTab);
  HRESULT put_HotKeyAltSpace(long photKeyAltSpace);
  HRESULT get_HotKeyAltSpace([out] long* photKeyAltSpace);
  HRESULT put_HotKeyCtrlAltDel(long photKeyCtrlAltDel);
  HRESULT get_HotKeyCtrlAltDel([out] long* photKeyCtrlAltDel);
  HRESULT put_orderDrawThreshold(long porderDrawThreshold);
  HRESULT get_orderDrawThreshold([out] long* porderDrawThreshold);
  HRESULT put_BitmapCacheSize(long pbitmapCacheSize);
  HRESULT get_BitmapCacheSize([out] long* pbitmapCacheSize);
  HRESULT put_BitmapVirtualCacheSize(long pbitmapVirtualCacheSize);
  HRESULT get_BitmapVirtualCacheSize([out] long* pbitmapVirtualCacheSize);
  HRESULT put_ScaleBitmapCachesByBPP(long pbScale);
  HRESULT get_ScaleBitmapCachesByBPP([out] long* pbScale);
  HRESULT put_NumBitmapCaches(long pnumBitmapCaches);
  HRESULT get_NumBitmapCaches([out] long* pnumBitmapCaches);
  HRESULT put_CachePersistenceActive(long pcachePersistenceActive);
  HRESULT get_CachePersistenceActive([out] long* pcachePersistenceActive);
  HRESULT put_PersistCacheDirectory(BSTR _arg1);
  HRESULT put_brushSupportLevel(long pbrushSupportLevel);
  HRESULT get_brushSupportLevel([out] long* pbrushSupportLevel);
  HRESULT put_minInputSendInterval(long pminInputSendInterval);
  HRESULT get_minInputSendInterval([out] long* pminInputSendInterval);
  HRESULT put_InputEventsAtOnce(long pinputEventsAtOnce);
  HRESULT get_InputEventsAtOnce([out] long* pinputEventsAtOnce);
  HRESULT put_maxEventCount(long pmaxEventCount);
  HRESULT get_maxEventCount([out] long* pmaxEventCount);
  HRESULT put_keepAliveInterval(long pkeepAliveInterval);
  HRESULT get_keepAliveInterval([out] long* pkeepAliveInterval);
  HRESULT put_shutdownTimeout(long pshutdownTimeout);
  HRESULT get_shutdownTimeout([out] long* pshutdownTimeout);
  HRESULT put_overallConnectionTimeout(long poverallConnectionTimeout);
  HRESULT get_overallConnectionTimeout([out] long* poverallConnectionTimeout);
  HRESULT put_singleConnectionTimeout(long psingleConnectionTimeout);
  HRESULT get_singleConnectionTimeout([out] long* psingleConnectionTimeout);
  HRESULT put_KeyboardType(long pkeyboardType);
  HRESULT get_KeyboardType([out] long* pkeyboardType);
  HRESULT put_KeyboardSubType(long pkeyboardSubType);
  HRESULT get_KeyboardSubType([out] long* pkeyboardSubType);
  HRESULT put_KeyboardFunctionKey(long pkeyboardFunctionKey);
  HRESULT get_KeyboardFunctionKey([out] long* pkeyboardFunctionKey);
  HRESULT put_WinceFixedPalette(long pwinceFixedPalette);
  HRESULT get_WinceFixedPalette([out] long* pwinceFixedPalette);
  HRESULT put_ConnectToServerConsole(VARIANT_BOOL pConnectToConsole);
  HRESULT get_ConnectToServerConsole([out] VARIANT_BOOL* pConnectToConsole);
  HRESULT put_BitmapPersistence(long pbitmapPersistence);
  HRESULT get_BitmapPersistence([out] long* pbitmapPersistence);
  HRESULT put_MinutesToIdleTimeout(long pminutesToIdleTimeout);
  HRESULT get_MinutesToIdleTimeout([out] long* pminutesToIdleTimeout);
  HRESULT put_SmartSizing(VARIANT_BOOL pfSmartSizing);
  HRESULT get_SmartSizing([out] VARIANT_BOOL* pfSmartSizing);
  HRESULT put_RdpdrLocalPrintingDocName(BSTR pLocalPrintingDocName);
  HRESULT get_RdpdrLocalPrintingDocName([out] BSTR* pLocalPrintingDocName);
  HRESULT put_RdpdrClipCleanTempDirString(BSTR clipCleanTempDirString);
  HRESULT get_RdpdrClipCleanTempDirString([out] BSTR* clipCleanTempDirString);
  HRESULT put_RdpdrClipPasteInfoString(BSTR clipPasteInfoString);
  HRESULT get_RdpdrClipPasteInfoString([out] BSTR* clipPasteInfoString);
  HRESULT put_ClearTextPassword(BSTR _arg1);
  HRESULT put_DisplayConnectionBar(VARIANT_BOOL pDisplayConnectionBar);
  HRESULT get_DisplayConnectionBar([out] VARIANT_BOOL* pDisplayConnectionBar);
  HRESULT put_PinConnectionBar(VARIANT_BOOL pPinConnectionBar);
  HRESULT get_PinConnectionBar([out] VARIANT_BOOL* pPinConnectionBar);
  HRESULT put_GrabFocusOnConnect(VARIANT_BOOL pfGrabFocusOnConnect);
  HRESULT get_GrabFocusOnConnect([out] VARIANT_BOOL* pfGrabFocusOnConnect);
  HRESULT put_LoadBalanceInfo(BSTR pLBInfo);
  HRESULT get_LoadBalanceInfo([out] BSTR* pLBInfo);
  HRESULT put_RedirectDrives(VARIANT_BOOL pRedirectDrives);
  HRESULT get_RedirectDrives([out] VARIANT_BOOL* pRedirectDrives);
  HRESULT put_RedirectPrinters(VARIANT_BOOL pRedirectPrinters);
  HRESULT get_RedirectPrinters([out] VARIANT_BOOL* pRedirectPrinters);
  HRESULT put_RedirectPorts(VARIANT_BOOL pRedirectPorts);
  HRESULT get_RedirectPorts([out] VARIANT_BOOL* pRedirectPorts);
  HRESULT put_RedirectSmartCards(VARIANT_BOOL pRedirectSmartCards);
  HRESULT get_RedirectSmartCards([out] VARIANT_BOOL* pRedirectSmartCards);
  HRESULT put_BitmapVirtualCache16BppSize(long pBitmapVirtualCache16BppSize);
  HRESULT get_BitmapVirtualCache16BppSize([out] long* pBitmapVirtualCache16BppSize);
  HRESULT put_BitmapVirtualCache24BppSize(long pBitmapVirtualCache24BppSize);
  HRESULT get_BitmapVirtualCache24BppSize([out] long* pBitmapVirtualCache24BppSize);
  HRESULT put_PerformanceFlags(long pDisableList);
  HRESULT get_PerformanceFlags([out] long* pDisableList);
  HRESULT put_ConnectWithEndpoint(VARIANT* _arg1);
  HRESULT put_NotifyTSPublicKey(VARIANT_BOOL pfNotify);
  HRESULT get_NotifyTSPublicKey([out] VARIANT_BOOL* pfNotify);
};

interface IMsRdpClientSecuredSettings : IMsTscSecuredSettings {
  HRESULT put_KeyboardHookMode(long pkeyboardHookMode);
  HRESULT get_KeyboardHookMode([out] long* pkeyboardHookMode);
  HRESULT put_AudioRedirectionMode(long pAudioRedirectionMode);
  HRESULT get_AudioRedirectionMode([out] long* pAudioRedirectionMode);
};

interface IMsRdpClient : IMsTscAx {
  HRESULT put_ColorDepth(long pcolorDepth);
  HRESULT get_ColorDepth([out] long* pcolorDepth);
  HRESULT get_AdvancedSettings2([out] IMsRdpClientAdvancedSettings** ppAdvSettings);
  HRESULT get_SecuredSettings2([out] IMsRdpClientSecuredSettings** ppSecuredSettings);
  HRESULT get_ExtendedDisconnectReason([out] ExtendedDisconnectReasonCode* pExtendedDisconnectReason);
  HRESULT put_FullScreen(VARIANT_BOOL pfFullScreen);
  HRESULT get_FullScreen([out] VARIANT_BOOL* pfFullScreen);
  HRESULT SetVirtualChannelOptions(BSTR chanName, long chanOptions);
  HRESULT GetVirtualChannelOptions(BSTR chanName, [out] long* pChanOptions);
  HRESULT RequestClose([out] ControlCloseStatus* pCloseStatus);
};

interface IMsTscNonScriptable : IUnknown {
  HRESULT put_ClearTextPassword(BSTR _arg1);
  HRESULT put_PortablePassword(BSTR pPortablePass);
  HRESULT get_PortablePassword([out] BSTR* pPortablePass);
  HRESULT put_PortableSalt(BSTR pPortableSalt);
  HRESULT get_PortableSalt([out] BSTR* pPortableSalt);
  HRESULT put_BinaryPassword(BSTR pBinaryPassword);
  HRESULT get_BinaryPassword([out] BSTR* pBinaryPassword);
  HRESULT put_BinarySalt(BSTR pSalt);
  HRESULT get_BinarySalt([out] BSTR* pSalt);
  HRESULT ResetPassword();
};

interface IMsRdpClientNonScriptable : IMsTscNonScriptable {
  HRESULT NotifyRedirectDeviceChange(UINT_PTR wParam, LONG_PTR lParam);
  HRESULT SendKeys(long numKeys, VARIANT_BOOL* pbArrayKeyUp, long* plKeyData);
};

interface IMsRdpClientAdvancedSettings2 : IMsRdpClientAdvancedSettings {
  HRESULT get_CanAutoReconnect([out] VARIANT_BOOL* pfCanAutoReconnect);
  HRESULT put_EnableAutoReconnect(VARIANT_BOOL pfEnableAutoReconnect);
  HRESULT get_EnableAutoReconnect([out] VARIANT_BOOL* pfEnableAutoReconnect);
  HRESULT put_MaxReconnectAttempts(long pMaxReconnectAttempts);
  HRESULT get_MaxReconnectAttempts([out] long* pMaxReconnectAttempts);
};

interface IMsRdpClient2 : IMsRdpClient {
  HRESULT get_AdvancedSettings3([out] IMsRdpClientAdvancedSettings2** ppAdvSettings);
  HRESULT put_ConnectedStatusText(BSTR pConnectedStatusText);
  HRESULT get_ConnectedStatusText([out] BSTR* pConnectedStatusText);
};

interface IMsRdpClientAdvancedSettings3 : IMsRdpClientAdvancedSettings2 {
  HRESULT put_ConnectionBarShowMinimizeButton(VARIANT_BOOL pfShowMinimize);
  HRESULT get_ConnectionBarShowMinimizeButton([out] VARIANT_BOOL* pfShowMinimize);
  HRESULT put_ConnectionBarShowRestoreButton(VARIANT_BOOL pfShowRestore);
  HRESULT get_ConnectionBarShowRestoreButton([out] VARIANT_BOOL* pfShowRestore);
};

interface IMsRdpClient3 : IMsRdpClient2 {
  HRESULT get_AdvancedSettings4([out] IMsRdpClientAdvancedSettings3** ppAdvSettings);
};

interface IMsRdpClientAdvancedSettings4 : IMsRdpClientAdvancedSettings3 {
  HRESULT put_AuthenticationLevel(UINT puiAuthLevel);
  HRESULT get_AuthenticationLevel([out] UINT* puiAuthLevel);
};

interface IMsRdpClient4 : IMsRdpClient3 {
  HRESULT get_AdvancedSettings5([out] IMsRdpClientAdvancedSettings4** ppAdvSettings);
};

interface IMsRdpClientNonScriptable2 : IMsRdpClientNonScriptable {
  HRESULT put_UIParentWindowHandle(LPVOID phwndUIParentWindowHandle);
  HRESULT get_UIParentWindowHandle([out] LPVOID* phwndUIParentWindowHandle);
};

interface IMsRdpClientTransportSettings : IDispatch {
  HRESULT put_GatewayHostname(BSTR pProxyHostname);
  HRESULT get_GatewayHostname([out] BSTR* pProxyHostname);
  HRESULT put_GatewayUsageMethod(ULONG pulProxyUsageMethod);
  HRESULT get_GatewayUsageMethod([out] ULONG* pulProxyUsageMethod);
  HRESULT put_GatewayProfileUsageMethod(ULONG pulProxyProfileUsageMethod);
  HRESULT get_GatewayProfileUsageMethod([out] ULONG* pulProxyProfileUsageMethod);
  HRESULT put_GatewayCredsSource(ULONG pulProxyCredsSource);
  HRESULT get_GatewayCredsSource([out] ULONG* pulProxyCredsSource);
  HRESULT put_GatewayUserSelectedCredsSource(ULONG pulProxyCredsSource);
  HRESULT get_GatewayUserSelectedCredsSource([out] ULONG* pulProxyCredsSource);
  HRESULT get_GatewayIsSupported([out] long* pfProxyIsSupported);
  HRESULT get_GatewayDefaultUsageMethod([out] ULONG* pulProxyDefaultUsageMethod);
};

interface IMsRdpClientAdvancedSettings5 : IMsRdpClientAdvancedSettings4 {
  HRESULT put_RedirectClipboard(VARIANT_BOOL pfRedirectClipboard);
  HRESULT get_RedirectClipboard([out] VARIANT_BOOL* pfRedirectClipboard);
  HRESULT put_AudioRedirectionMode(UINT puiAudioRedirectionMode);
  HRESULT get_AudioRedirectionMode([out] UINT* puiAudioRedirectionMode);
  HRESULT put_ConnectionBarShowPinButton(VARIANT_BOOL pfShowPin);
  HRESULT get_ConnectionBarShowPinButton([out] VARIANT_BOOL* pfShowPin);
  HRESULT put_PublicMode(VARIANT_BOOL pfPublicMode);
  HRESULT get_PublicMode([out] VARIANT_BOOL* pfPublicMode);
  HRESULT put_RedirectDevices(VARIANT_BOOL pfRedirectPnPDevices);
  HRESULT get_RedirectDevices([out] VARIANT_BOOL* pfRedirectPnPDevices);
  HRESULT put_RedirectPOSDevices(VARIANT_BOOL pfRedirectPOSDevices);
  HRESULT get_RedirectPOSDevices([out] VARIANT_BOOL* pfRedirectPOSDevices);
  HRESULT put_BitmapVirtualCache32BppSize(long pBitmapVirtualCache32BppSize);
  HRESULT get_BitmapVirtualCache32BppSize([out] long* pBitmapVirtualCache32BppSize);
};

interface ITSRemoteProgram : IDispatch {
  HRESULT put_RemoteProgramMode(VARIANT_BOOL pvboolRemoteProgramMode);
  HRESULT get_RemoteProgramMode([out] VARIANT_BOOL* pvboolRemoteProgramMode);
  HRESULT ServerStartProgram(BSTR bstrExecutablePath, BSTR bstrFilePath, BSTR bstrWorkingDirectory, VARIANT_BOOL vbExpandEnvVarInWorkingDirectoryOnServer, BSTR bstrArguments, VARIANT_BOOL vbExpandEnvVarInArgumentsOnServer);
};

interface IMsRdpClientShell : IDispatch {
  HRESULT Launch();
  HRESULT put_RdpFileContents(BSTR pszRdpFile);
  HRESULT get_RdpFileContents([out] BSTR* pszRdpFile);
  HRESULT SetRdpProperty(BSTR szProperty, VARIANT Value);
  HRESULT GetRdpProperty(BSTR szProperty, [out] VARIANT* pValue);
  HRESULT get_IsRemoteProgramClientInstalled([out] VARIANT_BOOL* pbClientInstalled);
  HRESULT put_PublicMode(VARIANT_BOOL pfPublicMode);
  HRESULT get_PublicMode([out] VARIANT_BOOL* pfPublicMode);
  HRESULT ShowTrustedSitesManagementDialog();
};

interface IMsRdpClient5 : IMsRdpClient4 {
  HRESULT get_TransportSettings([out] IMsRdpClientTransportSettings** ppXportSet);
  HRESULT get_AdvancedSettings6([out] IMsRdpClientAdvancedSettings5** ppAdvSettings);
  HRESULT GetErrorDescription(UINT disconnectReason, UINT ExtendedDisconnectReason, [out] BSTR* pBstrErrorMsg);
  HRESULT get_RemoteProgram([out] ITSRemoteProgram** ppRemoteProgram);
  HRESULT get_MsRdpClientShell([out] IMsRdpClientShell** ppLauncher);
};

interface IMsRdpDevice : IUnknown {
  HRESULT get_DeviceInstanceId([out] BSTR* pDevInstanceId);
  HRESULT get_FriendlyName([out] BSTR* pFriendlyName);
  HRESULT get_DeviceDescription([out] BSTR* pDeviceDescription);
  HRESULT put_RedirectionState(VARIANT_BOOL pvboolRedirState);
  HRESULT get_RedirectionState([out] VARIANT_BOOL* pvboolRedirState);
};

interface IMsRdpDeviceCollection : IUnknown {
  HRESULT RescanDevices(VARIANT_BOOL vboolDynRedir);
  HRESULT get_DeviceByIndex(ULONG index, [out] IMsRdpDevice** ppDevice);
  HRESULT get_DeviceById(BSTR devInstanceId, [out] IMsRdpDevice** ppDevice);
  HRESULT get_DeviceCount([out] ULONG* pDeviceCount);
};

interface IMsRdpDrive : IUnknown {
  HRESULT get_Name([out] BSTR* pName);
  HRESULT put_RedirectionState(VARIANT_BOOL pvboolRedirState);
  HRESULT get_RedirectionState([out] VARIANT_BOOL* pvboolRedirState);
};

interface IMsRdpDriveCollection : IUnknown {
  HRESULT RescanDrives(VARIANT_BOOL vboolDynRedir);
  HRESULT get_DriveByIndex(ULONG index, [out] IMsRdpDrive** ppDevice);
  HRESULT get_DriveCount([out] ULONG* pDriveCount);
};

interface IMsRdpClientNonScriptable3 : IMsRdpClientNonScriptable2 {
  HRESULT put_ShowRedirectionWarningDialog(VARIANT_BOOL pfShowRdrDlg);
  HRESULT get_ShowRedirectionWarningDialog([out] VARIANT_BOOL* pfShowRdrDlg);
  HRESULT put_PromptForCredentials(VARIANT_BOOL pfPrompt);
  HRESULT get_PromptForCredentials([out] VARIANT_BOOL* pfPrompt);
  HRESULT put_NegotiateSecurityLayer(VARIANT_BOOL pfNegotiate);
  HRESULT get_NegotiateSecurityLayer([out] VARIANT_BOOL* pfNegotiate);
  HRESULT put_EnableCredSspSupport(VARIANT_BOOL pfEnableSupport);
  HRESULT get_EnableCredSspSupport([out] VARIANT_BOOL* pfEnableSupport);
  HRESULT put_RedirectDynamicDrives(VARIANT_BOOL pfRedirectDynamicDrives);
  HRESULT get_RedirectDynamicDrives([out] VARIANT_BOOL* pfRedirectDynamicDrives);
  HRESULT put_RedirectDynamicDevices(VARIANT_BOOL pfRedirectDynamicDevices);
  HRESULT get_RedirectDynamicDevices([out] VARIANT_BOOL* pfRedirectDynamicDevices);
  HRESULT get_DeviceCollection([out] IMsRdpDeviceCollection** ppDeviceCollection);
  HRESULT get_DriveCollection([out] IMsRdpDriveCollection** ppDeviceCollection);
  HRESULT put_WarnAboutSendingCredentials(VARIANT_BOOL pfWarn);
  HRESULT get_WarnAboutSendingCredentials([out] VARIANT_BOOL* pfWarn);
  HRESULT put_WarnAboutClipboardRedirection(VARIANT_BOOL pfWarn);
  HRESULT get_WarnAboutClipboardRedirection([out] VARIANT_BOOL* pfWarn);
  HRESULT put_ConnectionBarText(BSTR pConnectionBarText);
  HRESULT get_ConnectionBarText([out] BSTR* pConnectionBarText);
};

interface IMsRdpClientAdvancedSettings6 : IMsRdpClientAdvancedSettings5 {
  HRESULT put_RelativeMouseMode(VARIANT_BOOL pfRelativeMouseMode);
  HRESULT get_RelativeMouseMode([out] VARIANT_BOOL* pfRelativeMouseMode);
  HRESULT get_AuthenticationServiceClass([out] BSTR* pbstrAuthServiceClass);
  HRESULT put_AuthenticationServiceClass(BSTR pbstrAuthServiceClass);
  HRESULT get_PCB([out] BSTR* bstrPCB);
  HRESULT put_PCB(BSTR bstrPCB);
  HRESULT put_HotKeyFocusReleaseLeft(long HotKeyFocusReleaseLeft);
  HRESULT get_HotKeyFocusReleaseLeft([out] long* HotKeyFocusReleaseLeft);
  HRESULT put_HotKeyFocusReleaseRight(long HotKeyFocusReleaseRight);
  HRESULT get_HotKeyFocusReleaseRight([out] long* HotKeyFocusReleaseRight);
  HRESULT put_EnableCredSspSupport(VARIANT_BOOL pfEnableSupport);
  HRESULT get_EnableCredSspSupport([out] VARIANT_BOOL* pfEnableSupport);
  HRESULT get_AuthenticationType([out] UINT* puiAuthType);
  HRESULT put_ConnectToAdministerServer(VARIANT_BOOL pConnectToAdministerServer);
  HRESULT get_ConnectToAdministerServer([out] VARIANT_BOOL* pConnectToAdministerServer);
};

interface IMsRdpClientTransportSettings2 : IMsRdpClientTransportSettings {
  HRESULT put_GatewayCredSharing(ULONG pulProxyCredSharing);
  HRESULT get_GatewayCredSharing([out] ULONG* pulProxyCredSharing);
  HRESULT put_GatewayPreAuthRequirement(ULONG pulProxyPreAuthRequirement);
  HRESULT get_GatewayPreAuthRequirement([out] ULONG* pulProxyPreAuthRequirement);
  HRESULT put_GatewayPreAuthServerAddr(BSTR pbstrProxyPreAuthServerAddr);
  HRESULT get_GatewayPreAuthServerAddr([out] BSTR* pbstrProxyPreAuthServerAddr);
  HRESULT put_GatewaySupportUrl(BSTR pbstrProxySupportUrl);
  HRESULT get_GatewaySupportUrl([out] BSTR* pbstrProxySupportUrl);
  HRESULT put_GatewayEncryptedOtpCookie(BSTR pbstrEncryptedOtpCookie);
  HRESULT get_GatewayEncryptedOtpCookie([out] BSTR* pbstrEncryptedOtpCookie);
  HRESULT put_GatewayEncryptedOtpCookieSize(ULONG pulEncryptedOtpCookieSize);
  HRESULT get_GatewayEncryptedOtpCookieSize([out] ULONG* pulEncryptedOtpCookieSize);
  HRESULT put_GatewayUsername(BSTR pProxyUsername);
  HRESULT get_GatewayUsername([out] BSTR* pProxyUsername);
  HRESULT put_GatewayDomain(BSTR pProxyDomain);
  HRESULT get_GatewayDomain([out] BSTR* pProxyDomain);
  HRESULT put_GatewayPassword(BSTR _arg1);
};

interface IMsRdpClient6 : IMsRdpClient5 {
  HRESULT get_AdvancedSettings7([out] IMsRdpClientAdvancedSettings6** ppAdvSettings);
  HRESULT get_TransportSettings2([out] IMsRdpClientTransportSettings2** ppXportSet2);
};

interface IMsRdpClientNonScriptable4 : IMsRdpClientNonScriptable3 {
  HRESULT put_RedirectionWarningType(RedirectionWarningType pWrnType);
  HRESULT get_RedirectionWarningType([out] RedirectionWarningType* pWrnType);
  HRESULT put_MarkRdpSettingsSecure(VARIANT_BOOL pfRdpSecure);
  HRESULT get_MarkRdpSettingsSecure([out] VARIANT_BOOL* pfRdpSecure);
  HRESULT put_PublisherCertificateChain(VARIANT* pVarCert);
  HRESULT get_PublisherCertificateChain([out] VARIANT* pVarCert);
  HRESULT put_WarnAboutPrinterRedirection(VARIANT_BOOL pfWarn);
  HRESULT get_WarnAboutPrinterRedirection([out] VARIANT_BOOL* pfWarn);
  HRESULT put_AllowCredentialSaving(VARIANT_BOOL pfAllowSave);
  HRESULT get_AllowCredentialSaving([out] VARIANT_BOOL* pfAllowSave);
  HRESULT put_PromptForCredsOnClient(VARIANT_BOOL pfPromptForCredsOnClient);
  HRESULT get_PromptForCredsOnClient([out] VARIANT_BOOL* pfPromptForCredsOnClient);
  HRESULT put_LaunchedViaClientShellInterface(VARIANT_BOOL pfLaunchedViaClientShellInterface);
  HRESULT get_LaunchedViaClientShellInterface([out] VARIANT_BOOL* pfLaunchedViaClientShellInterface);
  HRESULT put_TrustedZoneSite(VARIANT_BOOL pfIsTrustedZone);
  HRESULT get_TrustedZoneSite([out] VARIANT_BOOL* pfIsTrustedZone);
};

interface IMsRdpClientAdvancedSettings7 : IMsRdpClientAdvancedSettings6 {
  HRESULT put_AudioCaptureRedirectionMode(VARIANT_BOOL pfRedir);
  HRESULT get_AudioCaptureRedirectionMode([out] VARIANT_BOOL* pfRedir);
  HRESULT put_VideoPlaybackMode(UINT pVideoPlaybackMode);
  HRESULT get_VideoPlaybackMode([out] UINT* pVideoPlaybackMode);
  HRESULT put_EnableSuperPan(VARIANT_BOOL pfEnableSuperPan);
  HRESULT get_EnableSuperPan([out] VARIANT_BOOL* pfEnableSuperPan);
  HRESULT put_SuperPanAccelerationFactor(ULONG puAccelFactor);
  HRESULT get_SuperPanAccelerationFactor([out] ULONG* puAccelFactor);
  HRESULT put_NegotiateSecurityLayer(VARIANT_BOOL pfNegotiate);
  HRESULT get_NegotiateSecurityLayer([out] VARIANT_BOOL* pfNegotiate);
  HRESULT put_AudioQualityMode(UINT pAudioQualityMode);
  HRESULT get_AudioQualityMode([out] UINT* pAudioQualityMode);
  HRESULT put_RedirectDirectX(VARIANT_BOOL pfRedirectDirectX);
  HRESULT get_RedirectDirectX([out] VARIANT_BOOL* pfRedirectDirectX);
  HRESULT put_NetworkConnectionType(UINT pConnectionType);
  HRESULT get_NetworkConnectionType([out] UINT* pConnectionType);
};

interface IMsRdpClientTransportSettings3 : IMsRdpClientTransportSettings2 {
  HRESULT put_GatewayCredSourceCookie(ULONG pulProxyCredSourceCookie);
  HRESULT get_GatewayCredSourceCookie([out] ULONG* pulProxyCredSourceCookie);
  HRESULT put_GatewayAuthCookieServerAddr(BSTR pbstrProxyAuthCookieServerAddr);
  HRESULT get_GatewayAuthCookieServerAddr([out] BSTR* pbstrProxyAuthCookieServerAddr);
  HRESULT put_GatewayEncryptedAuthCookie(BSTR pbstrEncryptedAuthCookie);
  HRESULT get_GatewayEncryptedAuthCookie([out] BSTR* pbstrEncryptedAuthCookie);
  HRESULT put_GatewayEncryptedAuthCookieSize(ULONG pulEncryptedAuthCookieSize);
  HRESULT get_GatewayEncryptedAuthCookieSize([out] ULONG* pulEncryptedAuthCookieSize);
  HRESULT put_GatewayAuthLoginPage(BSTR pbstrProxyAuthLoginPage);
  HRESULT get_GatewayAuthLoginPage([out] BSTR* pbstrProxyAuthLoginPage);
};

interface IMsRdpClientSecuredSettings2 : IMsRdpClientSecuredSettings {
  HRESULT get_PCB([out] BSTR* bstrPCB);
  HRESULT put_PCB(BSTR bstrPCB);
};

interface ITSRemoteProgram2 : ITSRemoteProgram {
  HRESULT put_RemoteApplicationName(BSTR _arg1);
  HRESULT put_RemoteApplicationProgram(BSTR _arg1);
  HRESULT put_RemoteApplicationArgs(BSTR _arg1);
};

interface IMsRdpClient7 : IMsRdpClient6 {
  HRESULT get_AdvancedSettings8([out] IMsRdpClientAdvancedSettings7** ppAdvSettings);
  HRESULT get_TransportSettings3([out] IMsRdpClientTransportSettings3** ppXportSet3);
  HRESULT GetStatusText(UINT statusCode, [out] BSTR* pBstrStatusText);
  HRESULT get_SecuredSettings3([out] IMsRdpClientSecuredSettings2** ppSecuredSettings);
  HRESULT get_RemoteProgram2([out] ITSRemoteProgram2** ppRemoteProgram);
};

interface IMsRdpClientNonScriptable5 : IMsRdpClientNonScriptable4 {
  HRESULT put_UseMultimon(VARIANT_BOOL pfUseMultimon);
  HRESULT get_UseMultimon([out] VARIANT_BOOL* pfUseMultimon);
  HRESULT get_RemoteMonitorCount([out] ULONG* pcRemoteMonitors);
  HRESULT GetRemoteMonitorsBoundingBox(/*[out]*/ long* pLeft, /*[out]*/ long* pTop, /*[out]*/ long* pRight, /*[out]*/ long* pBottom);
  HRESULT get_RemoteMonitorLayoutMatchesLocal([out] VARIANT_BOOL* pfRemoteMatchesLocal);
  HRESULT put_DisableConnectionBar(VARIANT_BOOL _arg1);
  HRESULT put_DisableRemoteAppCapsCheck(VARIANT_BOOL pfDisableRemoteAppCapsCheck);
  HRESULT get_DisableRemoteAppCapsCheck([out] VARIANT_BOOL* pfDisableRemoteAppCapsCheck);
  HRESULT put_WarnAboutDirectXRedirection(VARIANT_BOOL pfWarn);
  HRESULT get_WarnAboutDirectXRedirection([out] VARIANT_BOOL* pfWarn);
  HRESULT put_AllowPromptingForCredentials(VARIANT_BOOL pfAllow);
  HRESULT get_AllowPromptingForCredentials([out] VARIANT_BOOL* pfAllow);
};

interface IMsRdpPreferredRedirectionInfo : IUnknown {
  HRESULT put_UseRedirectionServerName(VARIANT_BOOL pVal);
  HRESULT get_UseRedirectionServerName([out] VARIANT_BOOL* pVal);
};

interface IMsRdpExtendedSettings : IUnknown {
  HRESULT put_Property(BSTR bstrPropertyName, VARIANT* pValue);
  HRESULT get_Property(BSTR bstrPropertyName, [out] VARIANT* pValue);
};

interface IMsRdpClientAdvancedSettings8 : IMsRdpClientAdvancedSettings7 {
  HRESULT put_BandwidthDetection(VARIANT_BOOL pfAutodetect);
  HRESULT get_BandwidthDetection([out] VARIANT_BOOL* pfAutodetect);
  HRESULT put_ClientProtocolSpec(ClientSpec pClientMode);
  HRESULT get_ClientProtocolSpec([out] ClientSpec* pClientMode);
};

interface IMsRdpClient8 : IMsRdpClient7 {
  HRESULT SendRemoteAction(RemoteSessionActionType actionType);
  HRESULT get_AdvancedSettings9([out] IMsRdpClientAdvancedSettings8** ppAdvSettings);
  HRESULT Reconnect(ULONG ulWidth, ULONG ulHeight, [out] ControlReconnectStatus* pReconnectStatus);
};

interface IRemoteDesktopClientEvents : IDispatch {
  HRESULT OnConnecting();
  HRESULT OnConnected();
  HRESULT OnLoginCompleted();
  HRESULT OnDisconnected(long disconnectReason, long ExtendedDisconnectReason, BSTR disconnectErrorMessage);
  HRESULT OnStatusChanged(long statusCode, BSTR statusMessage);
  HRESULT OnAutoReconnecting(long disconnectReason, long ExtendedDisconnectReason, BSTR disconnectErrorMessage, VARIANT_BOOL networkAvailable, long attemptCount, long maxAttemptCount);
  HRESULT OnAutoReconnected();
  HRESULT OnDialogDisplaying();
  HRESULT OnDialogDismissed();
  HRESULT OnNetworkStatusChanged(ULONG qualityLevel, long bandwidth, long rtt);
  HRESULT OnAdminMessageReceived(BSTR adminMessage);
  HRESULT OnKeyCombinationPressed(long keyCombination);
  HRESULT OnRemoteDesktopSizeChanged(long width, long height);
  HRESULT OnTouchPointerCursorMoved(long x, long y);
};

interface IRemoteDesktopClientSettings : IDispatch {
  HRESULT ApplySettings(BSTR RdpFileContents);
  HRESULT RetrieveSettings([out] BSTR* RdpFileContents);
  HRESULT GetRdpProperty(BSTR propertyName, [out] VARIANT* Value);
  HRESULT SetRdpProperty(BSTR propertyName, VARIANT Value);
};

interface IRemoteDesktopClientActions : IDispatch {
  HRESULT SuspendScreenUpdates();
  HRESULT ResumeScreenUpdates();
  HRESULT ExecuteRemoteAction(RemoteActionType remoteAction);
  HRESULT GetSnapshot(SnapshotEncodingType snapshotEncoding, SnapshotFormatType snapshotFormat, ULONG snapshotWidth, ULONG snapshotHeight, [out] BSTR* snapshotData);
};

interface IRemoteDesktopClientTouchPointer : IDispatch {
  HRESULT put_Enabled(VARIANT_BOOL Enabled);
  HRESULT get_Enabled([out] VARIANT_BOOL* Enabled);
  HRESULT put_EventsEnabled(VARIANT_BOOL EventsEnabled);
  HRESULT get_EventsEnabled([out] VARIANT_BOOL* EventsEnabled);
  HRESULT put_PointerSpeed(ULONG PointerSpeed);
  HRESULT get_PointerSpeed([out] ULONG* PointerSpeed);
};

interface IRemoteDesktopClient : IDispatch {
  HRESULT Connect();
  HRESULT Disconnect();
  HRESULT Reconnect(ULONG width, ULONG height);
  HRESULT get_Settings([out] IRemoteDesktopClientSettings** Settings);
  HRESULT get_Actions([out] IRemoteDesktopClientActions** Actions);
  HRESULT get_TouchPointer([out] IRemoteDesktopClientTouchPointer** TouchPointer);
  HRESULT DeleteSavedCredentials(BSTR serverName);
};

interface IMsRdpSessionManager : IUnknown {
  HRESULT StartRemoteApplication(SAFEARRAY* psaCreds, SAFEARRAY* psaParams, long lFlags);
  HRESULT GetProcessId([out] ULONG* pulProcessId);
};

HRESULT DllGetClassObject(REFCLSID rclsid, [iid] REFIID riid, [out] COM_INTERFACE_PTR* ppv);

#endif  // REMOTING_TOOLS_WINEXT_MANIFEST_RDP_H_