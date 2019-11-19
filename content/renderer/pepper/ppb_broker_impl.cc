// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/ppb_broker_impl.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "content/common/frame_messages.h"
#include "content/common/view_messages.h"
#include "content/renderer/pepper/host_globals.h"
#include "content/renderer/pepper/pepper_broker.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/plugin_module.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/platform_file.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_plugin_container.h"

using ppapi::PlatformFileToInt;
using ppapi::thunk::PPB_Broker_API;
using ppapi::TrackedCallback;

namespace content {

enum class PepperBrokerAction { CREATE = 0, CONNECT = 1, NUM };

// PPB_Broker_Impl ------------------------------------------------------

PPB_Broker_Impl::PPB_Broker_Impl(PP_Instance instance)
    : Resource(ppapi::OBJECT_IS_IMPL, instance),
      broker_(nullptr),
      connect_callback_(),
      pipe_handle_(PlatformFileToInt(base::SyncSocket::kInvalidHandle)),
      routing_id_(RenderThreadImpl::current()->GenerateRoutingID()) {
  ChildThreadImpl::current()->GetRouter()->AddRoute(routing_id_, this);

  UMA_HISTOGRAM_ENUMERATION("Pepper.BrokerAction", PepperBrokerAction::CREATE,
                            PepperBrokerAction::NUM);
}

PPB_Broker_Impl::~PPB_Broker_Impl() {
  if (broker_) {
    broker_->Disconnect(this);
    broker_ = nullptr;
  }

  // The plugin owns the handle.
  pipe_handle_ = PlatformFileToInt(base::SyncSocket::kInvalidHandle);
  ChildThreadImpl::current()->GetRouter()->RemoveRoute(routing_id_);
}

PPB_Broker_API* PPB_Broker_Impl::AsPPB_Broker_API() { return this; }

int32_t PPB_Broker_Impl::Connect(
    scoped_refptr<TrackedCallback> connect_callback) {
  // TODO(ddorwin): Return PP_ERROR_FAILED if plugin is in-process.

  if (broker_) {
    // May only be called once.
    return PP_ERROR_FAILED;
  }

  PepperPluginInstanceImpl* plugin_instance =
      HostGlobals::Get()->GetInstance(pp_instance());
  if (!plugin_instance)
    return PP_ERROR_FAILED;
  PluginModule* module = plugin_instance->module();
  const base::FilePath& broker_path = module->path();

  // The callback must be populated now in case we are connected to the broker
  // and BrokerConnected is called before ConnectToBroker returns.
  // Because it must be created now, it must be aborted and cleared if
  // ConnectToBroker fails.
  connect_callback_ = connect_callback;

  broker_ = module->GetBroker();
  if (!broker_) {
    broker_ = new PepperBroker(module);

    // Have the browser start the broker process for us.
    RenderThreadImpl::current()->Send(
        new FrameHostMsg_OpenChannelToPpapiBroker(routing_id_, broker_path));
  }

  RenderThreadImpl::current()->Send(
      new ViewHostMsg_RequestPpapiBrokerPermission(
          plugin_instance->render_frame()->render_view()->GetRoutingID(),
          routing_id_,
          GetDocumentUrl(),
          broker_path));

  // Adds a reference, ensuring that the broker is not deleted when
  // |broker| goes out of scope.
  broker_->AddPendingConnect(this);

  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_Broker_Impl::GetHandle(int32_t* handle) {
  if (pipe_handle_ == PlatformFileToInt(base::SyncSocket::kInvalidHandle))
    return PP_ERROR_FAILED;  // Handle not set yet.
  *handle = pipe_handle_;
  return PP_OK;
}

GURL PPB_Broker_Impl::GetDocumentUrl() {
  PepperPluginInstanceImpl* plugin_instance =
      HostGlobals::Get()->GetInstance(pp_instance());
  return plugin_instance->container()->GetDocument().Url();
}

// Transfers ownership of the handle to the plugin.
void PPB_Broker_Impl::BrokerConnected(int32_t handle, int32_t result) {
  DCHECK(pipe_handle_ == PlatformFileToInt(base::SyncSocket::kInvalidHandle));
  DCHECK(result == PP_OK ||
         handle == PlatformFileToInt(base::SyncSocket::kInvalidHandle));

  pipe_handle_ = handle;

  // Synchronous calls are not supported.
  DCHECK(TrackedCallback::IsPending(connect_callback_));

  connect_callback_->Run(result);
}

bool PPB_Broker_Impl::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Broker_Impl, message)
    IPC_MESSAGE_HANDLER(ViewMsg_PpapiBrokerChannelCreated,
                        OnPpapiBrokerChannelCreated)
    IPC_MESSAGE_HANDLER(ViewMsg_PpapiBrokerPermissionResult,
                        OnPpapiBrokerPermissionResult)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPB_Broker_Impl::OnPpapiBrokerChannelCreated(
    base::ProcessId broker_pid,
    const IPC::ChannelHandle& handle) {
  broker_->OnBrokerChannelConnected(broker_pid, handle);
}

void PPB_Broker_Impl::OnPpapiBrokerPermissionResult(bool result) {
  broker_->OnBrokerPermissionResult(this, result);
}

}  // namespace content
