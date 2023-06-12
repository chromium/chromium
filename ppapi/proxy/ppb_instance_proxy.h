// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_INSTANCE_PROXY_H_
#define PPAPI_PROXY_PPB_INSTANCE_PROXY_H_

#include <stdint.h>

#include <string>

#include "build/build_config.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/proxy_completion_callback_factory.h"
#include "ppapi/shared_impl/host_resource.h"
#include "ppapi/shared_impl/ppb_instance_shared.h"
#include "ppapi/shared_impl/singleton_resource_id.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/utility/completion_callback_factory.h"

// Windows headers interfere with this file.
#ifdef PostMessage
#undef PostMessage
#endif

namespace ppapi {
namespace proxy {

class SerializedVarReceiveInput;
class SerializedVarOutParam;
class SerializedVarReturnValue;

class PPB_Instance_Proxy : public InterfaceProxy,
                           public PPB_Instance_Shared {
 public:
  PPB_Instance_Proxy(Dispatcher* dispatcher);
  ~PPB_Instance_Proxy() override;

  // InterfaceProxy implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;

  // PPB_Instance_API implementation.
  PP_Bool BindGraphics(PP_Instance instance, PP_Resource device) override;
  PP_Bool IsFullFrame(PP_Instance instance) override;
  const ViewData* GetViewData(PP_Instance instance) override;
  PP_Var GetWindowObject(PP_Instance instance) override;
  PP_Var GetOwnerElementObject(PP_Instance instance) override;
  PP_Var ExecuteScript(PP_Instance instance,
                       PP_Var script,
                       PP_Var* exception) override;
  uint32_t GetAudioHardwareOutputSampleRate(PP_Instance instance) override;
  uint32_t GetAudioHardwareOutputBufferSize(PP_Instance instance) override;
  PP_Var GetDefaultCharSet(PP_Instance instance) override;
  PP_Bool IsFullscreen(PP_Instance instance) override;
  PP_Bool SetFullscreen(PP_Instance instance, PP_Bool fullscreen) override;
  PP_Bool GetScreenSize(PP_Instance instance, PP_Size* size) override;
  Resource* GetSingletonResource(PP_Instance instance,
                                 SingletonResourceID id) override;
  int32_t RequestInputEvents(PP_Instance instance,
                             uint32_t event_classes) override;
  int32_t RequestFilteringInputEvents(PP_Instance instance,
                                      uint32_t event_classes) override;
  void ClearInputEventRequest(PP_Instance instance,
                              uint32_t event_classes) override;
  void PostMessage(PP_Instance instance, PP_Var message) override;
  int32_t RegisterMessageHandler(PP_Instance instance,
                                 void* user_data,
                                 const PPP_MessageHandler_0_2* handler,
                                 PP_Resource message_loop) override;
  void UnregisterMessageHandler(PP_Instance instance) override;
  PP_Bool SetCursor(PP_Instance instance,
                    PP_MouseCursor_Type type,
                    PP_Resource image,
                    const PP_Point* hot_spot) override;
  int32_t LockMouse(PP_Instance instance,
                    scoped_refptr<TrackedCallback> callback) override;
  void UnlockMouse(PP_Instance instance) override;
  void SetTextInputType(PP_Instance instance, PP_TextInput_Type type) override;
  void UpdateCaretPosition(PP_Instance instance,
                           const PP_Rect& caret,
                           const PP_Rect& bounding_box) override;
  void CancelCompositionText(PP_Instance instance) override;
  void SelectionChanged(PP_Instance instance) override;
  void UpdateSurroundingText(PP_Instance instance,
                             const char* text,
                             uint32_t caret,
                             uint32_t anchor) override;
  PP_Var GetDocumentURL(PP_Instance instance,
                        PP_URLComponents_Dev* components) override;
#if !BUILDFLAG(IS_NACL)
  PP_Var ResolveRelativeToDocument(PP_Instance instance,
                                   PP_Var relative,
                                   PP_URLComponents_Dev* components) override;
  PP_Bool DocumentCanRequest(PP_Instance instance, PP_Var url) override;
  PP_Bool DocumentCanAccessDocument(PP_Instance instance,
                                    PP_Instance target) override;
  PP_Var GetPluginInstanceURL(PP_Instance instance,
                              PP_URLComponents_Dev* components) override;
  PP_Var GetPluginReferrerURL(PP_Instance instance,
                              PP_URLComponents_Dev* components) override;
#endif  // !BUILDFLAG(IS_NACL)

  static const ApiID kApiID = API_ID_PPB_INSTANCE;

 private:
  // Plugin -> Host message handlers.
  void OnHostMsgGetWindowObject(PP_Instance instance,
                                SerializedVarReturnValue result);
  void OnHostMsgGetOwnerElementObject(PP_Instance instance,
                                      SerializedVarReturnValue result);
  void OnHostMsgBindGraphics(PP_Instance instance,
                             PP_Resource device);
  void OnHostMsgIsFullFrame(PP_Instance instance, PP_Bool* result);
  void OnHostMsgExecuteScript(PP_Instance instance,
                              SerializedVarReceiveInput script,
                              SerializedVarOutParam out_exception,
                              SerializedVarReturnValue result);
  void OnHostMsgGetAudioHardwareOutputSampleRate(PP_Instance instance,
                                                 uint32_t *result);
  void OnHostMsgGetAudioHardwareOutputBufferSize(PP_Instance instance,
                                                 uint32_t *result);
  void OnHostMsgGetDefaultCharSet(PP_Instance instance,
                                  SerializedVarReturnValue result);
  void OnHostMsgSetFullscreen(PP_Instance instance,
                              PP_Bool fullscreen,
                              PP_Bool* result);
  void OnHostMsgGetScreenSize(PP_Instance instance,
                              PP_Bool* result,
                              PP_Size* size);
  void OnHostMsgRequestInputEvents(PP_Instance instance,
                                   bool is_filtering,
                                   uint32_t event_classes);
  void OnHostMsgClearInputEvents(PP_Instance instance,
                                 uint32_t event_classes);
  void OnHostMsgPostMessage(PP_Instance instance,
                            SerializedVarReceiveInput message);
  void OnHostMsgLockMouse(PP_Instance instance);
  void OnHostMsgUnlockMouse(PP_Instance instance);
  void OnHostMsgSetCursor(PP_Instance instance,
                          int32_t type,
                          const ppapi::HostResource& custom_image,
                          const PP_Point& hot_spot);
  void OnHostMsgSetTextInputType(PP_Instance instance, PP_TextInput_Type type);
  void OnHostMsgUpdateCaretPosition(PP_Instance instance,
                                    const PP_Rect& caret,
                                    const PP_Rect& bounding_box);
  void OnHostMsgCancelCompositionText(PP_Instance instance);
  void OnHostMsgUpdateSurroundingText(
      PP_Instance instance,
      const std::string& text,
      uint32_t caret,
      uint32_t anchor);
  void OnHostMsgGetDocumentURL(PP_Instance instance,
                               PP_URLComponents_Dev* components,
                               SerializedVarReturnValue result);

#if !BUILDFLAG(IS_NACL)
  void OnHostMsgResolveRelativeToDocument(PP_Instance instance,
                                          SerializedVarReceiveInput relative,
                                          SerializedVarReturnValue result);
  void OnHostMsgDocumentCanRequest(PP_Instance instance,
                                   SerializedVarReceiveInput url,
                                   PP_Bool* result);
  void OnHostMsgDocumentCanAccessDocument(PP_Instance active,
                                          PP_Instance target,
                                          PP_Bool* result);
  void OnHostMsgGetPluginInstanceURL(PP_Instance instance,
                                     SerializedVarReturnValue result);
  void OnHostMsgGetPluginReferrerURL(PP_Instance instance,
                                     SerializedVarReturnValue result);
#endif  // !BUILDFLAG(IS_NACL)

  // Host -> Plugin message handlers.
  void OnPluginMsgMouseLockComplete(PP_Instance instance, int32_t result);

  void MouseLockCompleteInHost(int32_t result, PP_Instance instance);

  // Other helpers.
  void CancelAnyPendingRequestSurroundingText(PP_Instance instance);

  ProxyCompletionCallbackFactory<PPB_Instance_Proxy> callback_factory_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_INSTANCE_PROXY_H_
