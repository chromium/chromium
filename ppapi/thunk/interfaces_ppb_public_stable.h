// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// no-include-guard-because-multiply-included
// NOLINT(build/header_guard)

#include "ppapi/thunk/interfaces_preamble.h"

// This file contains lists of interfaces. It's intended to be included by
// another file which defines implementations of the macros. This allows files
// to do specific registration tasks for each supported interface.
//
// When adding an interface, you must also add the hash value for the
// interface's name to tools/metrics/histograms/histograms.xml. This is so we
// get human-readable UMA tracking for interface usage.
//
// Use the 'pepper_hash_for_uma' tool in ppapi/tools to determine the hash for
// a given interface string.

// Api categories
// --------------
// Enumerates the categories of APIs. These correspnd to the *_api.h files in
// this directory. One API may implement one or more actual interfaces.
//
// For PROXIED_APIs, these also correspond to *_Proxy objects. The proxied ones
// define factory functions for each of these classes.
PROXIED_API(PPB_Audio)
PROXIED_API(PPB_Core)
PROXIED_API(PPB_Graphics3D)
PROXIED_API(PPB_ImageData)
PROXIED_API(PPB_Instance)

// Interfaces
// ----------
// Enumerates interfaces as (interface_name, interface_struct).
//
// The api_name corresponds to the class in the list above for the object
// that implements the API. Some things may be special and aren't implemented
// by any specific API object, and we use "NoAPIName" for those. Implementors
// of these macros should handle this case. There can be more than one line
// referring to the same api_name (typically different versions of the
// same interface).
//
// The interface_name is the string that corresponds to the interface.
//
// The interface_struct is the typename of the struct corresponding to the
// interface string.
// Note: Core is special and is registered manually.
PROXIED_IFACE(PPB_AUDIO_INTERFACE_1_0, PPB_Audio_1_0)
PROXIED_IFACE(PPB_AUDIO_INTERFACE_1_1, PPB_Audio_1_1)
PROXIED_IFACE(PPB_AUDIOBUFFER_INTERFACE_0_1, PPB_AudioBuffer_0_1)
PROXIED_IFACE(PPB_FILEREF_INTERFACE_1_0, PPB_FileRef_1_0)
PROXIED_IFACE(PPB_FILEREF_INTERFACE_1_1, PPB_FileRef_1_1)
PROXIED_IFACE(PPB_FILEREF_INTERFACE_1_2, PPB_FileRef_1_2)
PROXIED_IFACE(PPB_FILESYSTEM_INTERFACE_1_0, PPB_FileSystem_1_0)
PROXIED_IFACE(PPB_GRAPHICS_3D_INTERFACE_1_0, PPB_Graphics3D_1_0)
PROXIED_IFACE(PPB_IMAGEDATA_INTERFACE_1_0, PPB_ImageData_1_0)
PROXIED_IFACE(PPB_CONSOLE_INTERFACE_1_0, PPB_Console_1_0)
PROXIED_IFACE(PPB_GAMEPAD_INTERFACE_1_0, PPB_Gamepad_1_0)
PROXIED_IFACE(PPB_INSTANCE_INTERFACE_1_0, PPB_Instance_1_0)
PROXIED_IFACE(PPB_FILEIO_INTERFACE_1_0, PPB_FileIO_1_0)
PROXIED_IFACE(PPB_FILEIO_INTERFACE_1_1, PPB_FileIO_1_1)
PROXIED_IFACE(PPB_GRAPHICS_2D_INTERFACE_1_0, PPB_Graphics2D_1_0)
PROXIED_IFACE(PPB_GRAPHICS_2D_INTERFACE_1_1, PPB_Graphics2D_1_1)
PROXIED_IFACE(PPB_GRAPHICS_2D_INTERFACE_1_2, PPB_Graphics2D_1_2)
PROXIED_IFACE(PPB_HOSTRESOLVER_INTERFACE_1_0, PPB_HostResolver_1_0)
PROXIED_IFACE(PPB_IME_INPUT_EVENT_INTERFACE_1_0, PPB_IMEInputEvent_1_0)
PROXIED_IFACE(PPB_INPUT_EVENT_INTERFACE_1_0, PPB_InputEvent_1_0)
PROXIED_IFACE(PPB_KEYBOARD_INPUT_EVENT_INTERFACE_1_0,
              PPB_KeyboardInputEvent_1_0)
PROXIED_IFACE(PPB_KEYBOARD_INPUT_EVENT_INTERFACE_1_2,
              PPB_KeyboardInputEvent_1_2)
PROXIED_IFACE(PPB_MOUSE_INPUT_EVENT_INTERFACE_1_0, PPB_MouseInputEvent_1_0)
PROXIED_IFACE(PPB_MOUSE_INPUT_EVENT_INTERFACE_1_1, PPB_MouseInputEvent_1_1)
PROXIED_IFACE(PPB_WHEEL_INPUT_EVENT_INTERFACE_1_0, PPB_WheelInputEvent_1_0)
PROXIED_IFACE(PPB_TOUCH_INPUT_EVENT_INTERFACE_1_0, PPB_TouchInputEvent_1_0)
PROXIED_IFACE(PPB_TOUCH_INPUT_EVENT_INTERFACE_1_4, PPB_TouchInputEvent_1_4)
PROXIED_IFACE(PPB_FULLSCREEN_INTERFACE_1_0, PPB_Fullscreen_1_0)
PROXIED_IFACE(PPB_MEDIASTREAMAUDIOTRACK_INTERFACE_0_1,
              PPB_MediaStreamAudioTrack_0_1)
PROXIED_IFACE(PPB_MEDIASTREAMVIDEOTRACK_INTERFACE_0_1,
              PPB_MediaStreamVideoTrack_0_1)
PROXIED_IFACE(PPB_MESSAGING_INTERFACE_1_0, PPB_Messaging_1_0)
PROXIED_IFACE(PPB_MESSAGING_INTERFACE_1_2, PPB_Messaging_1_2)
PROXIED_IFACE(PPB_MOUSECURSOR_INTERFACE_1_0, PPB_MouseCursor_1_0)
PROXIED_IFACE(PPB_MOUSELOCK_INTERFACE_1_0, PPB_MouseLock_1_0)
PROXIED_IFACE(PPB_NETADDRESS_INTERFACE_1_0, PPB_NetAddress_1_0)
PROXIED_IFACE(PPB_NETWORKLIST_INTERFACE_1_0, PPB_NetworkList_1_0)
PROXIED_IFACE(PPB_NETWORKMONITOR_INTERFACE_1_0, PPB_NetworkMonitor_1_0)
PROXIED_IFACE(PPB_NETWORKPROXY_INTERFACE_1_0, PPB_NetworkProxy_1_0)
PROXIED_IFACE(PPB_TEXTINPUTCONTROLLER_INTERFACE_1_0,
              PPB_TextInputController_1_0)
PROXIED_IFACE(PPB_URLLOADER_INTERFACE_1_0, PPB_URLLoader_1_0)
PROXIED_IFACE(PPB_URLREQUESTINFO_INTERFACE_1_0, PPB_URLRequestInfo_1_0)
PROXIED_IFACE(PPB_URLRESPONSEINFO_INTERFACE_1_0, PPB_URLResponseInfo_1_0)
PROXIED_IFACE(PPB_VAR_ARRAY_INTERFACE_1_0, PPB_VarArray_1_0)
PROXIED_IFACE(PPB_VAR_DICTIONARY_INTERFACE_1_0, PPB_VarDictionary_1_0)
PROXIED_IFACE(PPB_VIDEODECODER_INTERFACE_0_2, PPB_VideoDecoder_0_2)
PROXIED_IFACE(PPB_VIDEODECODER_INTERFACE_1_0, PPB_VideoDecoder_1_0)
PROXIED_IFACE(PPB_VIDEODECODER_INTERFACE_1_1, PPB_VideoDecoder_1_1)
PROXIED_IFACE(PPB_VIDEOENCODER_INTERFACE_0_2, PPB_VideoEncoder_0_2)
PROXIED_IFACE(PPB_VIDEOFRAME_INTERFACE_0_1, PPB_VideoFrame_0_1)
PROXIED_IFACE(PPB_WEBSOCKET_INTERFACE_1_0, PPB_WebSocket_1_0)

// Note: PPB_Var and PPB_VarArrayBuffer are special and registered manually.
PROXIED_IFACE(PPB_VIEW_INTERFACE_1_0, PPB_View_1_0)
PROXIED_IFACE(PPB_VIEW_INTERFACE_1_1, PPB_View_1_1)
PROXIED_IFACE(PPB_VIEW_INTERFACE_1_2, PPB_View_1_2)

// This has no corresponding _Proxy object since it does no IPC.
PROXIED_IFACE(PPB_AUDIO_CONFIG_INTERFACE_1_0, PPB_AudioConfig_1_0)
PROXIED_IFACE(PPB_AUDIO_CONFIG_INTERFACE_1_1, PPB_AudioConfig_1_1)

#include "ppapi/thunk/interfaces_postamble.h"
