/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef LIBRARIES_PPAPI_SIMPLE_PS_INTERFACE_H_
#define LIBRARIES_PPAPI_SIMPLE_PS_INTERFACE_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"


#include "ppapi/c/ppb_audio.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/c/ppb_console.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/c/ppb_file_ref.h"
#include "ppapi/c/ppb_file_system.h"
#include "ppapi/c/ppb_fullscreen.h"
#include "ppapi/c/ppb_gamepad.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/c/ppb_graphics_3d.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/ppb_input_event.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_message_loop.h"
#include "ppapi/c/ppb_messaging.h"
#include "ppapi/c/ppb_mouse_cursor.h"
#include "ppapi/c/ppb_mouse_lock.h"
#include "ppapi/c/ppb_url_loader.h"
#include "ppapi/c/ppb_url_request_info.h"
#include "ppapi/c/ppb_url_response_info.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/ppb_var_array.h"
#include "ppapi/c/ppb_var_array_buffer.h"
#include "ppapi/c/ppb_var_dictionary.h"
#include "ppapi/c/ppb_view.h"
#include "ppapi/c/ppb_websocket.h"

#include "sdk_util/macros.h"

EXTERN_C_BEGIN

const PPB_Audio* PSInterfaceAudio();
const PPB_AudioConfig* PSInterfaceAudioConfig();
const PPB_Console* PSInterfaceConsole();
const PPB_Core* PSInterfaceCore();
const PPB_FileIO* PSInterfaceFileIO();
const PPB_FileRef* PSInterfaceFileRef();
const PPB_FileSystem* PSInterfaceFileSystem();
const PPB_Fullscreen* PSInterfaceFullscreen();
const PPB_Gamepad* PSInterfaceGamepad();
const PPB_Graphics2D* PSInterfaceGraphics2D();
const PPB_Graphics3D* PSInterfaceGraphics3D();
const PPB_ImageData* PSInterfaceImageData();
const PPB_InputEvent* PSInterfaceInputEvent();
const PPB_Instance* PSInterfaceInstance();
const PPB_Messaging* PSInterfaceMessaging();
const PPB_MessageLoop* PSInterfaceMessageLoop();
const PPB_MouseCursor* PSInterfaceMouseCursor();
const PPB_URLLoader* PSInterfaceURLLoader();
const PPB_URLRequestInfo* PSInterfaceURLRequestInfo();
const PPB_URLResponseInfo* PSInterfaceURLResponseInfo();
const PPB_Var* PSInterfaceVar();
const PPB_VarArray* PSInterfaceVarArray();
const PPB_VarArrayBuffer* PSInterfaceVarArrayBuffer();
const PPB_VarDictionary* PSInterfaceVarDictionary();
const PPB_View* PSInterfaceView();
const PPB_WebSocket* PSInterfaceWebSocket();


/* Initializes the Interface module which fetches the above interfaces. */
void PSInterfaceInit();

EXTERN_C_END

#endif  // LIBRARIES_PPAPI_SIMPLE_PS_INTERFACE_H_
