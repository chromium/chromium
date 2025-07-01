// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_INSTANCE_API_H_
#define PPAPI_THUNK_PPB_INSTANCE_API_H_

#include <stdint.h>

#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "ppapi/c/dev/ppb_url_util_dev.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/c/ppb_console.h"
#include "ppapi/c/ppb_gamepad.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_mouse_cursor.h"
#include "ppapi/c/ppb_text_input_controller.h"
#include "ppapi/c/private/ppb_instance_private.h"
#include "ppapi/shared_impl/api_id.h"
#include "ppapi/shared_impl/singleton_resource_id.h"

// Windows headers interfere with this file.
#ifdef PostMessage
#undef PostMessage
#endif

struct PPP_MessageHandler_0_2;

namespace ppapi {

class Resource;
class TrackedCallback;
struct ViewData;

namespace thunk {

class PPB_Instance_API {
 public:
  virtual ~PPB_Instance_API() {}

  virtual PP_Bool BindGraphics(PP_Instance instance, PP_Resource device) = 0;
  virtual PP_Bool IsFullFrame(PP_Instance instance) = 0;

  // Unexposed PPAPI functions for proxying.
  // Returns the internal view data struct.
  virtual const ViewData* GetViewData(PP_Instance instance) = 0;

  // InstancePrivate.
  virtual PP_Var GetWindowObject(PP_Instance instance) = 0;
  virtual PP_Var GetOwnerElementObject(PP_Instance instance) = 0;
  virtual PP_Var ExecuteScript(PP_Instance instance,
                               PP_Var script,
                               PP_Var* exception) = 0;

  // Audio.
  virtual uint32_t GetAudioHardwareOutputSampleRate(PP_Instance instance) = 0;
  virtual uint32_t GetAudioHardwareOutputBufferSize(PP_Instance instance) = 0;

  // CharSet.
  virtual PP_Var GetDefaultCharSet(PP_Instance instance) = 0;

  // Console.
  virtual void Log(PP_Instance instance,
                   PP_LogLevel log_level,
                   PP_Var value) = 0;
  virtual void LogWithSource(PP_Instance instance,
                             PP_LogLevel log_level,
                             PP_Var source,
                             PP_Var value) = 0;

  // Fullscreen.
  virtual PP_Bool IsFullscreen(PP_Instance instance) = 0;
  virtual PP_Bool SetFullscreen(PP_Instance instance,
                                PP_Bool fullscreen) = 0;
  virtual PP_Bool GetScreenSize(PP_Instance instance, PP_Size* size) = 0;

  // This is an implementation-only function which grabs an instance of a
  // "singleton resource". These are used to implement instance interfaces
  // (functions which are associated with the instance itself as opposed to a
  // resource).
  virtual Resource* GetSingletonResource(
      PP_Instance instance, SingletonResourceID id) = 0;

  // InputEvent.
  virtual int32_t RequestInputEvents(PP_Instance instance,
                                     uint32_t event_classes) = 0;
  virtual int32_t RequestFilteringInputEvents(PP_Instance instance,
                                              uint32_t event_classes) = 0;
  virtual void ClearInputEventRequest(PP_Instance instance,
                                      uint32_t event_classes) = 0;

  // Messaging.
  virtual void PostMessage(PP_Instance instance, PP_Var message) = 0;
  virtual int32_t RegisterMessageHandler(PP_Instance instance,
                                         void* user_data,
                                         const PPP_MessageHandler_0_2* handler,
                                         PP_Resource message_loop) = 0;
  virtual void UnregisterMessageHandler(PP_Instance instance) = 0;

  // Mouse cursor.
  virtual PP_Bool SetCursor(PP_Instance instance,
                            PP_MouseCursor_Type type,
                            PP_Resource image,
                            const PP_Point* hot_spot) = 0;

  // MouseLock.
  virtual int32_t LockMouse(PP_Instance instance,
                            scoped_refptr<TrackedCallback> callback) = 0;
  virtual void UnlockMouse(PP_Instance instance) = 0;

  // TextInput.
  virtual void SetTextInputType(PP_Instance instance,
                                PP_TextInput_Type type) = 0;
  virtual void UpdateCaretPosition(PP_Instance instance,
                                   const PP_Rect& caret,
                                   const PP_Rect& bounding_box) = 0;
  virtual void CancelCompositionText(PP_Instance instance) = 0;
  virtual void SelectionChanged(PP_Instance instance) = 0;
  virtual void UpdateSurroundingText(PP_Instance instance,
                                     const char* text,
                                     uint32_t caret,
                                     uint32_t anchor) = 0;

  // Testing and URLUtil.
  virtual PP_Var GetDocumentURL(PP_Instance instance,
                                PP_URLComponents_Dev* components) = 0;
#if !BUILDFLAG(IS_NACL)
  // URLUtil.
  virtual PP_Var ResolveRelativeToDocument(
      PP_Instance instance,
      PP_Var relative,
      PP_URLComponents_Dev* components) = 0;
  virtual PP_Bool DocumentCanRequest(PP_Instance instance, PP_Var url) = 0;
  virtual PP_Bool DocumentCanAccessDocument(PP_Instance instance,
                                            PP_Instance target) = 0;
  virtual PP_Var GetPluginInstanceURL(PP_Instance instance,
                                      PP_URLComponents_Dev* components) = 0;
  virtual PP_Var GetPluginReferrerURL(PP_Instance instance,
                                      PP_URLComponents_Dev* components) = 0;
#endif  // !BUILDFLAG(IS_NACL)

  static const ApiID kApiID = API_ID_PPB_INSTANCE;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_INSTANCE_API_H_
