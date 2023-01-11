// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/plugin_message_filter.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "ipc/ipc_channel.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"
#include "ppapi/proxy/resource_reply_thread_registrar.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/resource_tracker.h"

namespace ppapi {
namespace proxy {

PluginMessageFilter::PluginMessageFilter(
    std::set<PP_Instance>* seen_instance_ids,
    scoped_refptr<ResourceReplyThreadRegistrar> registrar)
    : seen_instance_ids_(seen_instance_ids),
      resource_reply_thread_registrar_(registrar),
      sender_(NULL) {
}

PluginMessageFilter::~PluginMessageFilter() {
}

void PluginMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  sender_ = channel;
}

void PluginMessageFilter::OnFilterRemoved() {
  sender_ = NULL;
}

bool PluginMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PluginMessageFilter, message)
    IPC_MESSAGE_HANDLER(PpapiMsg_ReserveInstanceId, OnMsgReserveInstanceId)
    IPC_MESSAGE_HANDLER(PpapiPluginMsg_ResourceReply, OnMsgResourceReply)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool PluginMessageFilter::Send(IPC::Message* msg) {
  if (sender_)
    return sender_->Send(msg);
  delete msg;
  return false;
}

void PluginMessageFilter::AddResourceMessageFilter(
    const scoped_refptr<ResourceMessageFilter>& filter) {
  resource_filters_.push_back(filter);
}

// static
void PluginMessageFilter::DispatchResourceReplyForTest(
    const ResourceMessageReplyParams& reply_params,
    const IPC::Message& nested_msg) {
  DispatchResourceReply(reply_params, nested_msg);
}

void PluginMessageFilter::OnMsgReserveInstanceId(PP_Instance instance,
                                                 bool* usable) {
  // If |seen_instance_ids_| is set to NULL, we are not supposed to see this
  // message.
  CHECK(seen_instance_ids_);
  // See the message definition for how this works.
  if (seen_instance_ids_->find(instance) != seen_instance_ids_->end()) {
    // Instance ID already seen, reject it.
    *usable = false;
    return;
  }

  // This instance ID is new so we can return that it's usable and mark it as
  // used for future reference.
  seen_instance_ids_->insert(instance);
  *usable = true;
}

void PluginMessageFilter::OnMsgResourceReply(
    const ResourceMessageReplyParams& reply_params,
    const IPC::Message& nested_msg) {
  for (const auto& filter_ptr : resource_filters_) {
    if (filter_ptr->OnResourceReplyReceived(reply_params, nested_msg))
      return;
  }
  scoped_refptr<base::SingleThreadTaskRunner> target =
      resource_reply_thread_registrar_->GetTargetThread(reply_params,
                                                        nested_msg);
  target->PostTask(FROM_HERE, base::BindOnce(&DispatchResourceReply,
                                             reply_params, nested_msg));
}

// static
void PluginMessageFilter::DispatchResourceReply(
    const ResourceMessageReplyParams& reply_params,
    const IPC::Message& nested_msg) {
  ProxyAutoLock lock;
  Resource* resource = PpapiGlobals::Get()->GetResourceTracker()->GetResource(
      reply_params.pp_resource());
  if (!resource) {
    DVLOG_IF(1, reply_params.sequence() != 0)
        << "Pepper resource reply message received but the resource doesn't "
           "exist (probably has been destroyed).";
    return;
  }
  resource->OnReplyReceived(reply_params, nested_msg);
}

}  // namespace proxy
}  // namespace ppapi
