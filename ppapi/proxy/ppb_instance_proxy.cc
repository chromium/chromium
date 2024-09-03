// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_instance_proxy.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "media/base/limits.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_messaging.h"
#include "ppapi/c/ppb_mouse_lock.h"
#include "ppapi/proxy/browser_font_singleton_resource.h"
#include "ppapi/proxy/enter_proxy.h"
#include "ppapi/proxy/gamepad_resource.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/isolated_file_system_private_resource.h"
#include "ppapi/proxy/message_handler.h"
#include "ppapi/proxy/network_proxy_resource.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/serialized_var.h"
#include "ppapi/proxy/uma_private_resource.h"
#include "ppapi/shared_impl/array_var.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/ppb_url_util_shared.h"
#include "ppapi/shared_impl/ppb_view_shared.h"
#include "ppapi/shared_impl/scoped_pp_var.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_graphics_2d_api.h"
#include "ppapi/thunk/ppb_graphics_3d_api.h"
#include "ppapi/thunk/thunk.h"

// Windows headers interfere with this file.
#ifdef PostMessage
#undef PostMessage
#endif

using ppapi::thunk::EnterInstanceNoLock;
using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_Graphics2D_API;
using ppapi::thunk::PPB_Graphics3D_API;
using ppapi::thunk::PPB_Instance_API;

namespace ppapi {
namespace proxy {

namespace {

#if !BUILDFLAG(IS_NACL)
const char kSerializationError[] = "Failed to convert a PostMessage "
    "argument from a PP_Var to a Javascript value. It may have cycles or be of "
    "an unsupported type.";
#endif

void RequestSurroundingText(PP_Instance instance) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return;  // Instance has gone away while message was pending.

  InstanceData* data = dispatcher->GetInstanceData(instance);
  DCHECK(data);  // Should have it, since we still have a dispatcher.
  data->is_request_surrounding_text_pending = false;
  if (!data->should_do_request_surrounding_text)
    return;

  // Just fake out a RequestSurroundingText message to the proxy for the PPP
  // interface.
  InterfaceProxy* proxy = dispatcher->GetInterfaceProxy(API_ID_PPP_TEXT_INPUT);
  if (!proxy)
    return;
  proxy->OnMessageReceived(PpapiMsg_PPPTextInput_RequestSurroundingText(
      API_ID_PPP_TEXT_INPUT, instance,
      PPB_Instance_Shared::kExtraCharsForTextInput));
}

}  // namespace

PPB_Instance_Proxy::PPB_Instance_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      callback_factory_(this) {
}

PPB_Instance_Proxy::~PPB_Instance_Proxy() {
}

bool PPB_Instance_Proxy::OnMessageReceived(const IPC::Message& msg) {
  // Prevent the dispatcher from going away during a call to ExecuteScript.
  // This must happen OUTSIDE of ExecuteScript since the SerializedVars use
  // the dispatcher upon return of the function (converting the
  // SerializedVarReturnValue/OutParam to a SerializedVar in the destructor).
#if !BUILDFLAG(IS_NACL)
  ScopedModuleReference death_grip(dispatcher());
#endif

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Instance_Proxy, msg)
#if !BUILDFLAG(IS_NACL)
    // Plugin -> Host messages.
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_GetWindowObject,
                        OnHostMsgGetWindowObject)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_GetOwnerElementObject,
                        OnHostMsgGetOwnerElementObject)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_BindGraphics,
                        OnHostMsgBindGraphics)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_IsFullFrame,
                        OnHostMsgIsFullFrame)
    IPC_MESSAGE_HANDLER(
        PpapiHostMsg_PPBInstance_GetAudioHardwareOutputSampleRate,
        OnHostMsgGetAudioHardwareOutputSampleRate)
    IPC_MESSAGE_HANDLER(
        PpapiHostMsg_PPBInstance_GetAudioHardwareOutputBufferSize,
        OnHostMsgGetAudioHardwareOutputBufferSize)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_ExecuteScript,
                        OnHostMsgExecuteScript)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_GetDefaultCharSet,
                        OnHostMsgGetDefaultCharSet)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_PostMessage,
                        OnHostMsgPostMessage)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_SetFullscreen,
                        OnHostMsgSetFullscreen)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_GetScreenSize,
                        OnHostMsgGetScreenSize)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_RequestInputEvents,
                        OnHostMsgRequestInputEvents)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_ClearInputEvents,
                        OnHostMsgClearInputEvents)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_LockMouse,
                        OnHostMsgLockMouse)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_UnlockMouse,
                        OnHostMsgUnlockMouse)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_SetCursor,
                        OnHostMsgSetCursor)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_SetTextInputType,
                        OnHostMsgSetTextInputType)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_UpdateCaretPosition,
                        OnHostMsgUpdateCaretPosition)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_CancelCompositionText,
                        OnHostMsgCancelCompositionText)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_UpdateSurroundingText,
                        OnHostMsgUpdateSurroundingText)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_GetDocumentURL,
                        OnHostMsgGetDocumentURL)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_ResolveRelativeToDocument,
                        OnHostMsgResolveRelativeToDocument)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_DocumentCanRequest,
                        OnHostMsgDocumentCanRequest)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_DocumentCanAccessDocument,
                        OnHostMsgDocumentCanAccessDocument)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_GetPluginInstanceURL,
                        OnHostMsgGetPluginInstanceURL)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_GetPluginReferrerURL,
                        OnHostMsgGetPluginReferrerURL)
#endif  // !BUILDFLAG(IS_NACL)

    // Host -> Plugin messages.
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBInstance_MouseLockComplete,
                        OnPluginMsgMouseLockComplete)

    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

PP_Bool PPB_Instance_Proxy::BindGraphics(PP_Instance instance,
                                         PP_Resource device) {
  // If device is 0, pass a null HostResource. This signals the host to unbind
  // all devices.
  PP_Resource pp_resource = 0;
  if (device) {
    Resource* resource =
        PpapiGlobals::Get()->GetResourceTracker()->GetResource(device);
    if (!resource || resource->pp_instance() != instance)
      return PP_FALSE;
    // We need to pass different resource to Graphics 2D and 3D right now.  Once
    // 3D is migrated to the new design, we should be able to unify this.
    if (resource->AsPPB_Graphics3D_API()) {
      pp_resource = resource->host_resource().host_resource();
    } else if (resource->AsPPB_Graphics2D_API()) {
      pp_resource = resource->pp_resource();
    } else {
      // A bad resource.
      return PP_FALSE;
    }
  }
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_BindGraphics(
        API_ID_PPB_INSTANCE, instance, pp_resource));
  return PP_TRUE;
}

PP_Bool PPB_Instance_Proxy::IsFullFrame(PP_Instance instance) {
  PP_Bool result = PP_FALSE;
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_IsFullFrame(
      API_ID_PPB_INSTANCE, instance, &result));
  return result;
}

const ViewData* PPB_Instance_Proxy::GetViewData(PP_Instance instance) {
  InstanceData* data = static_cast<PluginDispatcher*>(dispatcher())->
      GetInstanceData(instance);
  if (!data)
    return NULL;
  return &data->view;
}

PP_Var PPB_Instance_Proxy::GetWindowObject(PP_Instance instance) {
  ReceiveSerializedVarReturnValue result;
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_GetWindowObject(
      API_ID_PPB_INSTANCE, instance, &result));
  return result.Return(dispatcher());
}

PP_Var PPB_Instance_Proxy::GetOwnerElementObject(PP_Instance instance) {
  ReceiveSerializedVarReturnValue result;
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_GetOwnerElementObject(
      API_ID_PPB_INSTANCE, instance, &result));
  return result.Return(dispatcher());
}

PP_Var PPB_Instance_Proxy::ExecuteScript(PP_Instance instance,
                                         PP_Var script,
                                         PP_Var* exception) {
  ReceiveSerializedException se(dispatcher(), exception);
  if (se.IsThrown())
    return PP_MakeUndefined();

  ReceiveSerializedVarReturnValue result;
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_ExecuteScript(
      API_ID_PPB_INSTANCE, instance,
      SerializedVarSendInput(dispatcher(), script), &se, &result));
  return result.Return(dispatcher());
}

uint32_t PPB_Instance_Proxy::GetAudioHardwareOutputSampleRate(
    PP_Instance instance) {
  uint32_t result = PP_AUDIOSAMPLERATE_NONE;
  dispatcher()->Send(
      new PpapiHostMsg_PPBInstance_GetAudioHardwareOutputSampleRate(
          API_ID_PPB_INSTANCE, instance, &result));
  return result;
}

uint32_t PPB_Instance_Proxy::GetAudioHardwareOutputBufferSize(
    PP_Instance instance) {
  uint32_t result = 0;
  dispatcher()->Send(
      new PpapiHostMsg_PPBInstance_GetAudioHardwareOutputBufferSize(
          API_ID_PPB_INSTANCE, instance, &result));
  return result;
}

PP_Var PPB_Instance_Proxy::GetDefaultCharSet(PP_Instance instance) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return PP_MakeUndefined();

  ReceiveSerializedVarReturnValue result;
  dispatcher->Send(new PpapiHostMsg_PPBInstance_GetDefaultCharSet(
      API_ID_PPB_INSTANCE, instance, &result));
  return result.Return(dispatcher);
}

PP_Bool PPB_Instance_Proxy::IsFullscreen(PP_Instance instance) {
  InstanceData* data = static_cast<PluginDispatcher*>(dispatcher())->
      GetInstanceData(instance);
  if (!data)
    return PP_FALSE;
  return PP_FromBool(data->view.is_fullscreen);
}

PP_Bool PPB_Instance_Proxy::SetFullscreen(PP_Instance instance,
                                          PP_Bool fullscreen) {
  PP_Bool result = PP_FALSE;
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_SetFullscreen(
      API_ID_PPB_INSTANCE, instance, fullscreen, &result));
  return result;
}

PP_Bool PPB_Instance_Proxy::GetScreenSize(PP_Instance instance,
                                          PP_Size* size) {
  PP_Bool result = PP_FALSE;
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_GetScreenSize(
      API_ID_PPB_INSTANCE, instance, &result, size));
  return result;
}

Resource* PPB_Instance_Proxy::GetSingletonResource(PP_Instance instance,
                                                   SingletonResourceID id) {
  InstanceData* data = static_cast<PluginDispatcher*>(dispatcher())->
      GetInstanceData(instance);

  InstanceData::SingletonResourceMap::iterator it =
      data->singleton_resources.find(id);
  if (it != data->singleton_resources.end())
    return it->second.get();

  scoped_refptr<Resource> new_singleton;
  Connection connection(PluginGlobals::Get()->GetBrowserSender(),
                        static_cast<PluginDispatcher*>(dispatcher())->sender());

  switch (id) {
    case GAMEPAD_SINGLETON_ID:
      new_singleton = new GamepadResource(connection, instance);
      break;
    case ISOLATED_FILESYSTEM_SINGLETON_ID:
      new_singleton =
          new IsolatedFileSystemPrivateResource(connection, instance);
      break;
    case NETWORK_PROXY_SINGLETON_ID:
      new_singleton = new NetworkProxyResource(connection, instance);
      break;
    case UMA_SINGLETON_ID:
      new_singleton = new UMAPrivateResource(connection, instance);
      break;
// Flash/trusted resources aren't needed for NaCl.
#if !BUILDFLAG(IS_NACL)
    case BROWSER_FONT_SINGLETON_ID:
      new_singleton = new BrowserFontSingletonResource(connection, instance);
      break;
#else
    case BROWSER_FONT_SINGLETON_ID:
      NOTREACHED();
#endif  // !BUILDFLAG(IS_NACL)
  }

  if (!new_singleton.get()) {
    // Getting here implies that a constructor is missing in the above switch.
    NOTREACHED();
  }

  data->singleton_resources[id] = new_singleton;
  return new_singleton.get();
}

int32_t PPB_Instance_Proxy::RequestInputEvents(PP_Instance instance,
                                               uint32_t event_classes) {
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_RequestInputEvents(
      API_ID_PPB_INSTANCE, instance, false, event_classes));

  // We always register for the classes we can handle, this function validates
  // the flags so we can notify it if anything was invalid, without requiring
  // a sync reply.
  return ValidateRequestInputEvents(false, event_classes);
}

int32_t PPB_Instance_Proxy::RequestFilteringInputEvents(
    PP_Instance instance,
    uint32_t event_classes) {
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_RequestInputEvents(
      API_ID_PPB_INSTANCE, instance, true, event_classes));

  // We always register for the classes we can handle, this function validates
  // the flags so we can notify it if anything was invalid, without requiring
  // a sync reply.
  return ValidateRequestInputEvents(true, event_classes);
}

void PPB_Instance_Proxy::ClearInputEventRequest(PP_Instance instance,
                                                uint32_t event_classes) {
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_ClearInputEvents(
      API_ID_PPB_INSTANCE, instance, event_classes));
}

PP_Var PPB_Instance_Proxy::GetDocumentURL(PP_Instance instance,
                                          PP_URLComponents_Dev* components) {
  ReceiveSerializedVarReturnValue result;
  PP_URLComponents_Dev url_components = {{0}};
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_GetDocumentURL(
      API_ID_PPB_INSTANCE, instance, &url_components, &result));
  if (components)
    *components = url_components;
  return result.Return(dispatcher());
}

#if !BUILDFLAG(IS_NACL)
PP_Var PPB_Instance_Proxy::ResolveRelativeToDocument(
    PP_Instance instance,
    PP_Var relative,
    PP_URLComponents_Dev* components) {
  ReceiveSerializedVarReturnValue result;
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_ResolveRelativeToDocument(
      API_ID_PPB_INSTANCE, instance,
      SerializedVarSendInput(dispatcher(), relative),
      &result));
  return PPB_URLUtil_Shared::ConvertComponentsAndReturnURL(
      result.Return(dispatcher()),
      components);
}

PP_Bool PPB_Instance_Proxy::DocumentCanRequest(PP_Instance instance,
                                               PP_Var url) {
  PP_Bool result = PP_FALSE;
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_DocumentCanRequest(
      API_ID_PPB_INSTANCE, instance,
      SerializedVarSendInput(dispatcher(), url),
      &result));
  return result;
}

PP_Bool PPB_Instance_Proxy::DocumentCanAccessDocument(PP_Instance instance,
                                                      PP_Instance target) {
  PP_Bool result = PP_FALSE;
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_DocumentCanAccessDocument(
      API_ID_PPB_INSTANCE, instance, target, &result));
  return result;
}

PP_Var PPB_Instance_Proxy::GetPluginInstanceURL(
      PP_Instance instance,
      PP_URLComponents_Dev* components) {
  ReceiveSerializedVarReturnValue result;
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_GetPluginInstanceURL(
      API_ID_PPB_INSTANCE, instance, &result));
  return PPB_URLUtil_Shared::ConvertComponentsAndReturnURL(
      result.Return(dispatcher()),
      components);
}

PP_Var PPB_Instance_Proxy::GetPluginReferrerURL(
      PP_Instance instance,
      PP_URLComponents_Dev* components) {
  ReceiveSerializedVarReturnValue result;
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_GetPluginReferrerURL(
      API_ID_PPB_INSTANCE, instance, &result));
  return PPB_URLUtil_Shared::ConvertComponentsAndReturnURL(
      result.Return(dispatcher()),
      components);
}
#endif  // !BUILDFLAG(IS_NACL)

void PPB_Instance_Proxy::PostMessage(PP_Instance instance,
                                     PP_Var message) {
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_PostMessage(
      API_ID_PPB_INSTANCE,
      instance, SerializedVarSendInputShmem(dispatcher(), message,
                                            instance)));
}

int32_t PPB_Instance_Proxy::RegisterMessageHandler(
    PP_Instance instance,
    void* user_data,
    const PPP_MessageHandler_0_2* handler,
    PP_Resource message_loop) {
  InstanceData* data =
      static_cast<PluginDispatcher*>(dispatcher())->GetInstanceData(instance);
  if (!data)
    return PP_ERROR_BADARGUMENT;

  int32_t result = PP_ERROR_FAILED;
  std::unique_ptr<MessageHandler> message_handler = MessageHandler::Create(
      instance, handler, user_data, message_loop, &result);
  if (message_handler)
    data->message_handler = std::move(message_handler);
  return result;
}

void PPB_Instance_Proxy::UnregisterMessageHandler(PP_Instance instance) {
  InstanceData* data =
      static_cast<PluginDispatcher*>(dispatcher())->GetInstanceData(instance);
  if (!data)
    return;
  data->message_handler.reset();
}

PP_Bool PPB_Instance_Proxy::SetCursor(PP_Instance instance,
                                      PP_MouseCursor_Type type,
                                      PP_Resource image,
                                      const PP_Point* hot_spot) {
  // Some of these parameters are important for security. This check is in the
  // plugin process just for the convenience of the caller (since we don't
  // bother returning errors from the other process with a sync message). The
  // parameters will be validated again in the renderer.
  if (!ValidateSetCursorParams(type, image, hot_spot))
    return PP_FALSE;

  HostResource image_host_resource;
  if (image) {
    Resource* cursor_image =
        PpapiGlobals::Get()->GetResourceTracker()->GetResource(image);
    if (!cursor_image || cursor_image->pp_instance() != instance)
      return PP_FALSE;
    image_host_resource = cursor_image->host_resource();
  }

  dispatcher()->Send(new PpapiHostMsg_PPBInstance_SetCursor(
      API_ID_PPB_INSTANCE, instance, static_cast<int32_t>(type),
      image_host_resource, hot_spot ? *hot_spot : PP_MakePoint(0, 0)));
  return PP_TRUE;
}

int32_t PPB_Instance_Proxy::LockMouse(PP_Instance instance,
                                      scoped_refptr<TrackedCallback> callback) {
  // Save the mouse callback on the instance data.
  InstanceData* data = static_cast<PluginDispatcher*>(dispatcher())->
      GetInstanceData(instance);
  if (!data)
    return PP_ERROR_BADARGUMENT;
  if (TrackedCallback::IsPending(data->mouse_lock_callback))
    return PP_ERROR_INPROGRESS;  // Already have a pending callback.
  data->mouse_lock_callback = callback;

  dispatcher()->Send(new PpapiHostMsg_PPBInstance_LockMouse(
      API_ID_PPB_INSTANCE, instance));
  return PP_OK_COMPLETIONPENDING;
}

void PPB_Instance_Proxy::UnlockMouse(PP_Instance instance) {
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_UnlockMouse(
      API_ID_PPB_INSTANCE, instance));
}

void PPB_Instance_Proxy::SetTextInputType(PP_Instance instance,
                                          PP_TextInput_Type type) {
  CancelAnyPendingRequestSurroundingText(instance);
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_SetTextInputType(
      API_ID_PPB_INSTANCE, instance, type));
}

void PPB_Instance_Proxy::UpdateCaretPosition(PP_Instance instance,
                                             const PP_Rect& caret,
                                             const PP_Rect& bounding_box) {
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_UpdateCaretPosition(
      API_ID_PPB_INSTANCE, instance, caret, bounding_box));
}

void PPB_Instance_Proxy::CancelCompositionText(PP_Instance instance) {
  CancelAnyPendingRequestSurroundingText(instance);
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_CancelCompositionText(
      API_ID_PPB_INSTANCE, instance));
}

void PPB_Instance_Proxy::SelectionChanged(PP_Instance instance) {
  // The "right" way to do this is to send the message to the host. However,
  // all it will do is call RequestSurroundingText with a hardcoded number of
  // characters in response, which is an entire IPC round-trip.
  //
  // We can avoid this round-trip by just implementing the
  // RequestSurroundingText logic in the plugin process. If the logic in the
  // host becomes more complex (like a more adaptive number of characters),
  // we'll need to reevanuate whether we want to do the round trip instead.
  //
  // Be careful to post a task to avoid reentering the plugin.

  InstanceData* data =
      static_cast<PluginDispatcher*>(dispatcher())->GetInstanceData(instance);
  if (!data)
    return;
  data->should_do_request_surrounding_text = true;

  if (!data->is_request_surrounding_text_pending) {
    PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
        FROM_HERE,
        RunWhileLocked(base::BindOnce(&RequestSurroundingText, instance)));
    data->is_request_surrounding_text_pending = true;
  }
}

void PPB_Instance_Proxy::UpdateSurroundingText(PP_Instance instance,
                                               const char* text,
                                               uint32_t caret,
                                               uint32_t anchor) {
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_UpdateSurroundingText(
      API_ID_PPB_INSTANCE, instance, text, caret, anchor));
}

#if !BUILDFLAG(IS_NACL)
void PPB_Instance_Proxy::OnHostMsgGetWindowObject(
    PP_Instance instance,
    SerializedVarReturnValue result) {
  if (!dispatcher()->permissions().HasPermission(PERMISSION_PRIVATE))
    return;
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded())
    result.Return(dispatcher(), enter.functions()->GetWindowObject(instance));
}

void PPB_Instance_Proxy::OnHostMsgGetOwnerElementObject(
    PP_Instance instance,
    SerializedVarReturnValue result) {
  if (!dispatcher()->permissions().HasPermission(PERMISSION_PRIVATE))
    return;
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded()) {
    result.Return(dispatcher(),
                  enter.functions()->GetOwnerElementObject(instance));
  }
}

void PPB_Instance_Proxy::OnHostMsgBindGraphics(PP_Instance instance,
                                               PP_Resource device) {
  // Note that we ignroe the return value here. Otherwise, this would need to
  // be a slow sync call, and the plugin side of the proxy will have already
  // validated the resources, so we shouldn't see errors here that weren't
  // already caught.
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded())
    enter.functions()->BindGraphics(instance, device);
}

void PPB_Instance_Proxy::OnHostMsgGetAudioHardwareOutputSampleRate(
    PP_Instance instance, uint32_t* result) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded())
    *result = enter.functions()->GetAudioHardwareOutputSampleRate(instance);
}

void PPB_Instance_Proxy::OnHostMsgGetAudioHardwareOutputBufferSize(
    PP_Instance instance, uint32_t* result) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded())
    *result = enter.functions()->GetAudioHardwareOutputBufferSize(instance);
}

void PPB_Instance_Proxy::OnHostMsgIsFullFrame(PP_Instance instance,
                                              PP_Bool* result) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded())
    *result = enter.functions()->IsFullFrame(instance);
}

void PPB_Instance_Proxy::OnHostMsgExecuteScript(
    PP_Instance instance,
    SerializedVarReceiveInput script,
    SerializedVarOutParam out_exception,
    SerializedVarReturnValue result) {
  if (!dispatcher()->permissions().HasPermission(PERMISSION_PRIVATE))
    return;
  EnterInstanceNoLock enter(instance);
  if (enter.failed())
    return;

  CHECK(!dispatcher()->IsPlugin());
  static_cast<HostDispatcher*>(dispatcher())->set_allow_plugin_reentrancy();

  result.Return(dispatcher(), enter.functions()->ExecuteScript(
      instance,
      script.Get(dispatcher()),
      out_exception.OutParam(dispatcher())));
}

void PPB_Instance_Proxy::OnHostMsgGetDefaultCharSet(
    PP_Instance instance,
    SerializedVarReturnValue result) {
  if (!dispatcher()->permissions().HasPermission(PERMISSION_DEV))
    return;
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded())
    result.Return(dispatcher(), enter.functions()->GetDefaultCharSet(instance));
}

void PPB_Instance_Proxy::OnHostMsgSetFullscreen(PP_Instance instance,
                                                PP_Bool fullscreen,
                                                PP_Bool* result) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded())
    *result = enter.functions()->SetFullscreen(instance, fullscreen);
}


void PPB_Instance_Proxy::OnHostMsgGetScreenSize(PP_Instance instance,
                                                PP_Bool* result,
                                                PP_Size* size) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded())
    *result = enter.functions()->GetScreenSize(instance, size);
}

void PPB_Instance_Proxy::OnHostMsgRequestInputEvents(PP_Instance instance,
                                                     bool is_filtering,
                                                     uint32_t event_classes) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded()) {
    if (is_filtering)
      enter.functions()->RequestFilteringInputEvents(instance, event_classes);
    else
      enter.functions()->RequestInputEvents(instance, event_classes);
  }
}

void PPB_Instance_Proxy::OnHostMsgClearInputEvents(PP_Instance instance,
                                                   uint32_t event_classes) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded())
    enter.functions()->ClearInputEventRequest(instance, event_classes);
}

void PPB_Instance_Proxy::OnHostMsgPostMessage(
    PP_Instance instance,
    SerializedVarReceiveInput message) {
  EnterInstanceNoLock enter(instance);
  if (!message.is_valid_var()) {
    PpapiGlobals::Get()->LogWithSource(
        instance, PP_LOGLEVEL_ERROR, std::string(), kSerializationError);
    return;
  }

  if (enter.succeeded())
    enter.functions()->PostMessage(instance,
                                   message.GetForInstance(dispatcher(),
                                                          instance));
}

void PPB_Instance_Proxy::OnHostMsgLockMouse(PP_Instance instance) {
  // Need to be careful to always issue the callback.
  pp::CompletionCallback cb = callback_factory_.NewCallback(
      &PPB_Instance_Proxy::MouseLockCompleteInHost, instance);

  EnterInstanceNoLock enter(instance, cb.pp_completion_callback());
  if (enter.succeeded())
    enter.SetResult(enter.functions()->LockMouse(instance, enter.callback()));
}

void PPB_Instance_Proxy::OnHostMsgUnlockMouse(PP_Instance instance) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded())
    enter.functions()->UnlockMouse(instance);
}

void PPB_Instance_Proxy::OnHostMsgGetDocumentURL(
    PP_Instance instance,
    PP_URLComponents_Dev* components,
    SerializedVarReturnValue result) {
  if (!dispatcher()->permissions().HasPermission(PERMISSION_DEV))
    return;
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded()) {
    PP_Var document_url = enter.functions()->GetDocumentURL(instance,
                                                            components);
    result.Return(dispatcher(), document_url);
  }
}

void PPB_Instance_Proxy::OnHostMsgResolveRelativeToDocument(
    PP_Instance instance,
    SerializedVarReceiveInput relative,
    SerializedVarReturnValue result) {
  if (!dispatcher()->permissions().HasPermission(PERMISSION_DEV))
    return;
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded()) {
    result.Return(dispatcher(),
                  enter.functions()->ResolveRelativeToDocument(
                      instance, relative.Get(dispatcher()), NULL));
  }
}

void PPB_Instance_Proxy::OnHostMsgDocumentCanRequest(
    PP_Instance instance,
    SerializedVarReceiveInput url,
    PP_Bool* result) {
  if (!dispatcher()->permissions().HasPermission(PERMISSION_DEV))
    return;
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded()) {
    *result = enter.functions()->DocumentCanRequest(instance,
                                                    url.Get(dispatcher()));
  }
}

void PPB_Instance_Proxy::OnHostMsgDocumentCanAccessDocument(PP_Instance active,
                                                            PP_Instance target,
                                                            PP_Bool* result) {
  if (!dispatcher()->permissions().HasPermission(PERMISSION_DEV))
    return;
  EnterInstanceNoLock enter(active);
  if (enter.succeeded())
    *result = enter.functions()->DocumentCanAccessDocument(active, target);
}

void PPB_Instance_Proxy::OnHostMsgGetPluginInstanceURL(
    PP_Instance instance,
    SerializedVarReturnValue result) {
  if (!dispatcher()->permissions().HasPermission(PERMISSION_DEV))
    return;
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded()) {
    result.Return(dispatcher(),
                  enter.functions()->GetPluginInstanceURL(instance, NULL));
  }
}

void PPB_Instance_Proxy::OnHostMsgGetPluginReferrerURL(
    PP_Instance instance,
    SerializedVarReturnValue result) {
  if (!dispatcher()->permissions().HasPermission(PERMISSION_DEV))
    return;
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded()) {
    result.Return(dispatcher(),
                  enter.functions()->GetPluginReferrerURL(instance, NULL));
  }
}

void PPB_Instance_Proxy::OnHostMsgSetCursor(
    PP_Instance instance,
    int32_t type,
    const ppapi::HostResource& custom_image,
    const PP_Point& hot_spot) {
  // This API serves PPB_CursorControl_Dev and PPB_MouseCursor, so is public.
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded()) {
    enter.functions()->SetCursor(
        instance, static_cast<PP_MouseCursor_Type>(type),
        custom_image.host_resource(), &hot_spot);
  }
}

void PPB_Instance_Proxy::OnHostMsgSetTextInputType(PP_Instance instance,
                                                   PP_TextInput_Type type) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded())
    enter.functions()->SetTextInputType(instance, type);
}

void PPB_Instance_Proxy::OnHostMsgUpdateCaretPosition(
    PP_Instance instance,
    const PP_Rect& caret,
    const PP_Rect& bounding_box) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded())
    enter.functions()->UpdateCaretPosition(instance, caret, bounding_box);
}

void PPB_Instance_Proxy::OnHostMsgCancelCompositionText(PP_Instance instance) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded())
    enter.functions()->CancelCompositionText(instance);
}

void PPB_Instance_Proxy::OnHostMsgUpdateSurroundingText(
    PP_Instance instance,
    const std::string& text,
    uint32_t caret,
    uint32_t anchor) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded()) {
    enter.functions()->UpdateSurroundingText(instance, text.c_str(), caret,
                                             anchor);
  }
}
#endif  // !BUILDFLAG(IS_NACL)

void PPB_Instance_Proxy::OnPluginMsgMouseLockComplete(PP_Instance instance,
                                                      int32_t result) {
  if (!dispatcher()->IsPlugin())
    return;

  // Save the mouse callback on the instance data.
  InstanceData* data = static_cast<PluginDispatcher*>(dispatcher())->
      GetInstanceData(instance);
  if (!data)
    return;  // Instance was probably deleted.
  CHECK(TrackedCallback::IsPending(data->mouse_lock_callback));
  data->mouse_lock_callback->Run(result);
}

#if !BUILDFLAG(IS_NACL)
void PPB_Instance_Proxy::MouseLockCompleteInHost(int32_t result,
                                                 PP_Instance instance) {
  dispatcher()->Send(new PpapiMsg_PPBInstance_MouseLockComplete(
      API_ID_PPB_INSTANCE, instance, result));
}
#endif  // !BUILDFLAG(IS_NACL)

void PPB_Instance_Proxy::CancelAnyPendingRequestSurroundingText(
    PP_Instance instance) {
  InstanceData* data = static_cast<PluginDispatcher*>(dispatcher())->
      GetInstanceData(instance);
  if (!data)
    return;  // Instance was probably deleted.
  data->should_do_request_surrounding_text = false;
}

}  // namespace proxy
}  // namespace ppapi
