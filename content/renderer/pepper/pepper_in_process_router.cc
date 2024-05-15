// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_in_process_router.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/pepper/renderer_ppapi_host_impl.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sender.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/resource_tracker.h"

using ppapi::UnpackMessage;

namespace content {

class PepperInProcessRouter::Channel : public IPC::Sender {
 public:
  Channel(const base::RepeatingCallback<bool(IPC::Message*)>& callback)
      : callback_(callback) {}

  ~Channel() override {}

  bool Send(IPC::Message* message) override { return callback_.Run(message); }

 private:
  base::RepeatingCallback<bool(IPC::Message*)> callback_;
};

PepperInProcessRouter::PepperInProcessRouter(RendererPpapiHostImpl* host_impl)
    : host_impl_(host_impl), pending_message_id_(0), reply_result_(false) {
  browser_channel_ = std::make_unique<Channel>(base::BindRepeating(
      &PepperInProcessRouter::SendToBrowser, base::Unretained(this)));
  host_to_plugin_router_ = std::make_unique<Channel>(base::BindRepeating(
      &PepperInProcessRouter::SendToPlugin, base::Unretained(this)));
  plugin_to_host_router_ = std::make_unique<Channel>(base::BindRepeating(
      &PepperInProcessRouter::SendToHost, base::Unretained(this)));
}

PepperInProcessRouter::~PepperInProcessRouter() {}

IPC::Sender* PepperInProcessRouter::GetPluginToRendererSender() {
  return plugin_to_host_router_.get();
}

IPC::Sender* PepperInProcessRouter::GetRendererToPluginSender() {
  return host_to_plugin_router_.get();
}

ppapi::proxy::Connection PepperInProcessRouter::GetPluginConnection(
    PP_Instance instance) {
  int routing_id = 0;
  RenderFrame* frame = host_impl_->GetRenderFrameForInstance(instance);
  if (frame)
    routing_id = frame->GetRoutingID();
  return ppapi::proxy::Connection(
      browser_channel_.get(), plugin_to_host_router_.get(), routing_id);
}

// static
bool PepperInProcessRouter::OnPluginMsgReceived(const IPC::Message& msg) {
  // Emulate the proxy by dispatching the relevant message here.
  ppapi::proxy::ResourceMessageReplyParams reply_params;
  IPC::Message nested_msg;

  if (msg.type() == PpapiPluginMsg_ResourceReply::ID) {
    // Resource reply from the renderer (no routing id).
    if (!UnpackMessage<PpapiPluginMsg_ResourceReply>(
            msg, &reply_params, &nested_msg)) {
      NOTREACHED_IN_MIGRATION();
      return false;
    }
  } else if (msg.type() == PpapiHostMsg_InProcessResourceReply::ID) {
    // Resource reply from the browser (has a routing id).
    if (!UnpackMessage<PpapiHostMsg_InProcessResourceReply>(
            msg, &reply_params, &nested_msg)) {
      NOTREACHED_IN_MIGRATION();
      return false;
    }
  } else {
    return false;
  }
  ppapi::Resource* resource =
      ppapi::PpapiGlobals::Get()->GetResourceTracker()->GetResource(
          reply_params.pp_resource());
  // If the resource doesn't exist, it may have been destroyed so just ignore
  // the message.
  if (resource)
    resource->OnReplyReceived(reply_params, nested_msg);
  return true;
}

bool PepperInProcessRouter::SendToHost(IPC::Message* msg) {
  std::unique_ptr<IPC::Message> message(msg);

  if (!message->is_sync()) {
    // If this is a resource destroyed message, post a task to dispatch it.
    // Dispatching it synchronously can cause the host to re-enter the proxy
    // code while we're still in the resource destructor, leading to a crash.
    // http://crbug.com/276368.
    // This won't cause message reordering problems because the resource
    // destroyed message is always the last one sent for a resource.
    if (message->type() == PpapiHostMsg_ResourceDestroyed::ID) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&PepperInProcessRouter::DispatchHostMsg,
                                    weak_factory_.GetWeakPtr(),
                                    base::Owned(message.release())));
      return true;
    } else {
      bool result = host_impl_->GetPpapiHost()->OnMessageReceived(*message);
      DCHECK(result) << "The message was not handled by the host.";
      return true;
    }
  }

  pending_message_id_ = IPC::SyncMessage::GetMessageId(*message);
  reply_deserializer_ =
      static_cast<IPC::SyncMessage*>(message.get())->TakeReplyDeserializer();
  reply_result_ = false;

  bool result = host_impl_->GetPpapiHost()->OnMessageReceived(*message);
  DCHECK(result) << "The message was not handled by the host.";

  pending_message_id_ = 0;
  reply_deserializer_.reset(nullptr);
  return reply_result_;
}

bool PepperInProcessRouter::SendToPlugin(IPC::Message* msg) {
  std::unique_ptr<IPC::Message> message(msg);
  CHECK(!msg->is_sync());
  if (IPC::SyncMessage::IsMessageReplyTo(*message, pending_message_id_)) {
    if (!msg->is_reply_error())
      reply_result_ = reply_deserializer_->SerializeOutputParameters(*message);
  } else {
    CHECK(!pending_message_id_);
    // Dispatch plugin messages from the message loop.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&PepperInProcessRouter::DispatchPluginMsg,
                                  weak_factory_.GetWeakPtr(),
                                  base::Owned(message.release())));
  }
  return true;
}

void PepperInProcessRouter::DispatchHostMsg(IPC::Message* msg) {
  bool handled = host_impl_->GetPpapiHost()->OnMessageReceived(*msg);
  DCHECK(handled);
}

void PepperInProcessRouter::DispatchPluginMsg(IPC::Message* msg) {
  bool handled = OnPluginMsgReceived(*msg);
  DCHECK(handled);
}

bool PepperInProcessRouter::SendToBrowser(IPC::Message* msg) {
  return RenderThread::Get()->Send(msg);
}

}  // namespace content
