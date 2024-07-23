// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/proxy/ppb_testing_proxy.h"

#include <stddef.h>

#include "base/run_loop.h"
#include "ppapi/c/private/ppb_testing_private.h"
#include "ppapi/proxy/enter_proxy.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_graphics_2d_api.h"
#include "ppapi/thunk/ppb_input_event_api.h"

using ppapi::thunk::EnterInstance;
using ppapi::thunk::EnterResource;
using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_Graphics2D_API;
using ppapi::thunk::PPB_InputEvent_API;

namespace ppapi {
namespace proxy {

namespace {

PP_Bool ReadImageData(PP_Resource graphics_2d,
                      PP_Resource image,
                      const PP_Point* top_left) {
  ProxyAutoLock lock;
  Resource* image_object =
      PpapiGlobals::Get()->GetResourceTracker()->GetResource(image);
  if (!image_object)
    return PP_FALSE;
  Resource* graphics_2d_object =
      PpapiGlobals::Get()->GetResourceTracker()->GetResource(graphics_2d);
  if (!graphics_2d_object ||
      image_object->pp_instance() != graphics_2d_object->pp_instance())
    return PP_FALSE;

  EnterResourceNoLock<PPB_Graphics2D_API> enter(graphics_2d, true);
  if (enter.failed())
    return PP_FALSE;
  const HostResource& host_image = image_object->host_resource();
  return enter.object()->ReadImageData(host_image.host_resource(), top_left) ?
      PP_TRUE : PP_FALSE;
}

void RunMessageLoop(PP_Instance instance) {
  CHECK(PpapiGlobals::Get()->GetMainThreadMessageLoop()->
      BelongsToCurrentThread());
  PpapiGlobals::Get()->RunMsgLoop();
}

void QuitMessageLoop(PP_Instance instance) {
  CHECK(PpapiGlobals::Get()->GetMainThreadMessageLoop()->
            BelongsToCurrentThread());
  PpapiGlobals::Get()->QuitMsgLoop();
}

uint32_t GetLiveObjectsForInstance(PP_Instance instance_id) {
  ProxyAutoLock lock;
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance_id);
  if (!dispatcher)
    return static_cast<uint32_t>(-1);

  uint32_t result = 0;
  dispatcher->Send(new PpapiHostMsg_PPBTesting_GetLiveObjectsForInstance(
      API_ID_PPB_TESTING, instance_id, &result));
  return result;
}

PP_Bool IsOutOfProcess() {
  return PP_TRUE;
}

void SimulateInputEvent(PP_Instance instance_id, PP_Resource input_event) {
  ProxyAutoLock lock;
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance_id);
  if (!dispatcher)
    return;
  EnterResourceNoLock<PPB_InputEvent_API> enter(input_event, false);
  if (enter.failed())
    return;

  const InputEventData& input_event_data = enter.object()->GetInputEventData();
  dispatcher->Send(new PpapiHostMsg_PPBTesting_SimulateInputEvent(
      API_ID_PPB_TESTING, instance_id, input_event_data));
}

PP_Var GetDocumentURL(PP_Instance instance, PP_URLComponents_Dev* components) {
  EnterInstance enter(instance);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.functions()->GetDocumentURL(instance, components);
}

// TODO(dmichael): Ideally we could get a way to check the number of vars in the
// host-side tracker when running out-of-process, to make sure the proxy does
// not leak host-side vars.
uint32_t GetLiveVars(PP_Var live_vars[], uint32_t array_size) {
  ProxyAutoLock lock;
  std::vector<PP_Var> vars =
      PpapiGlobals::Get()->GetVarTracker()->GetLiveVars();
  for (size_t i = 0u;
       i < std::min(static_cast<size_t>(array_size), vars.size());
       ++i)
    live_vars[i] = vars[i];
  return static_cast<uint32_t>(vars.size());
}

void SetMinimumArrayBufferSizeForShmem(PP_Instance instance,
                                       uint32_t threshold) {
  ProxyAutoLock lock;
  RawVarDataGraph::SetMinimumArrayBufferSizeForShmemForTest(threshold);
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return;
  dispatcher->Send(
      new PpapiHostMsg_PPBTesting_SetMinimumArrayBufferSizeForShmem(
          API_ID_PPB_TESTING, threshold));
}

void RunV8GC(PP_Instance instance) {
  // TODO(raymes): Implement this if we need it.
  NOTIMPLEMENTED();
}

const PPB_Testing_Private testing_interface = {
    &ReadImageData,
    &RunMessageLoop,
    &QuitMessageLoop,
    &GetLiveObjectsForInstance,
    &IsOutOfProcess,
    &SimulateInputEvent,
    &GetDocumentURL,
    &GetLiveVars,
    &SetMinimumArrayBufferSizeForShmem,
    &RunV8GC};

}  // namespace

PPB_Testing_Proxy::PPB_Testing_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      ppb_testing_impl_(NULL) {
  if (!dispatcher->IsPlugin()) {
    ppb_testing_impl_ = static_cast<const PPB_Testing_Private*>(
        dispatcher->local_get_interface()(PPB_TESTING_PRIVATE_INTERFACE));
  }
}

PPB_Testing_Proxy::~PPB_Testing_Proxy() {
}

// static
const PPB_Testing_Private* PPB_Testing_Proxy::GetProxyInterface() {
  return &testing_interface;
}

bool PPB_Testing_Proxy::OnMessageReceived(const IPC::Message& msg) {
  if (!dispatcher()->permissions().HasPermission(PERMISSION_TESTING))
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Testing_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBTesting_ReadImageData,
                        OnMsgReadImageData)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBTesting_GetLiveObjectsForInstance,
                        OnMsgGetLiveObjectsForInstance)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBTesting_SimulateInputEvent,
                        OnMsgSimulateInputEvent)
    IPC_MESSAGE_HANDLER(
        PpapiHostMsg_PPBTesting_SetMinimumArrayBufferSizeForShmem,
        OnMsgSetMinimumArrayBufferSizeForShmem)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPB_Testing_Proxy::OnMsgReadImageData(
    const HostResource& device_context_2d,
    const HostResource& image,
    const PP_Point& top_left,
    PP_Bool* result) {
  *result = ppb_testing_impl_->ReadImageData(
      device_context_2d.host_resource(), image.host_resource(), &top_left);
}

void PPB_Testing_Proxy::OnMsgRunMessageLoop(PP_Instance instance) {
  ppb_testing_impl_->RunMessageLoop(instance);
}

void PPB_Testing_Proxy::OnMsgQuitMessageLoop(PP_Instance instance) {
  ppb_testing_impl_->QuitMessageLoop(instance);
}

void PPB_Testing_Proxy::OnMsgGetLiveObjectsForInstance(PP_Instance instance,
                                                       uint32_t* result) {
  *result = ppb_testing_impl_->GetLiveObjectsForInstance(instance);
}

void PPB_Testing_Proxy::OnMsgSimulateInputEvent(
    PP_Instance instance,
    const InputEventData& input_event) {
  scoped_refptr<PPB_InputEvent_Shared> input_event_impl(
      new PPB_InputEvent_Shared(OBJECT_IS_PROXY, instance, input_event));
  ppb_testing_impl_->SimulateInputEvent(instance,
                                        input_event_impl->pp_resource());
}

void PPB_Testing_Proxy::OnMsgSetMinimumArrayBufferSizeForShmem(
    uint32_t threshold) {
  RawVarDataGraph::SetMinimumArrayBufferSizeForShmemForTest(threshold);
}

}  // namespace proxy
}  // namespace ppapi
