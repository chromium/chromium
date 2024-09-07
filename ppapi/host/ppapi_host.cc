// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/host/ppapi_host.h"

#include <stddef.h>

#include <utility>

#include "base/check.h"
#include "base/notreached.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/host_factory.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/instance_message_filter.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"
#include "ppapi/proxy/serialized_handle.h"
#include "ppapi/shared_impl/host_resource.h"

namespace ppapi {
namespace host {

using proxy::SerializedHandle;

namespace {

// Put a cap on the maximum number of resources so we don't explode if the
// renderer starts spamming us.
const size_t kMaxResourcesPerPlugin = 1 << 14;

}  // namespace

PpapiHost::PpapiHost(IPC::Sender* sender,
                     const PpapiPermissions& perms)
    : sender_(sender),
      permissions_(perms),
      next_pending_resource_host_id_(1) {
}

PpapiHost::~PpapiHost() {
  // Delete these explicitly before destruction since then the host is still
  // technically alive in case one of the filters accesses us from the
  // destructor.
  instance_message_filters_.clear();

  // The resources may also want to use us in their destructors.
  resources_.clear();
  pending_resource_hosts_.clear();
}

bool PpapiHost::Send(IPC::Message* msg) {
  return sender_->Send(msg);
}

bool PpapiHost::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PpapiHost, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_ResourceCall,
                        OnHostMsgResourceCall)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_InProcessResourceCall,
                        OnHostMsgInProcessResourceCall)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(PpapiHostMsg_ResourceSyncCall,
                                    OnHostMsgResourceSyncCall)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_ResourceCreated,
                        OnHostMsgResourceCreated)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_AttachToPendingHost,
                        OnHostMsgAttachToPendingHost)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_ResourceDestroyed,
                        OnHostMsgResourceDestroyed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  if (!handled) {
    for (size_t i = 0; i < instance_message_filters_.size(); i++) {
      if (instance_message_filters_[i]->OnInstanceMessageReceived(msg)) {
        handled = true;
        break;
      }
    }
  }

  return handled;
}

void PpapiHost::SendReply(const ReplyMessageContext& context,
                          const IPC::Message& msg) {
  TRACE_EVENT2("ppapi_proxy", "PpapiHost::SendReply", "Class",
               IPC_MESSAGE_ID_CLASS(msg.type()), "Line",
               IPC_MESSAGE_ID_LINE(msg.type()));
  if (context.sync_reply_msg) {
    PpapiHostMsg_ResourceSyncCall::WriteReplyParams(context.sync_reply_msg,
                                                    context.params, msg);
    Send(context.sync_reply_msg);
  } else {
    if (context.routing_id != MSG_ROUTING_NONE) {
      Send(new PpapiHostMsg_InProcessResourceReply(context.routing_id,
                                                   context.params,
                                                   msg));
    } else {
      Send(new PpapiPluginMsg_ResourceReply(context.params, msg));
    }
  }
}

void PpapiHost::SendUnsolicitedReply(PP_Resource resource,
                                     const IPC::Message& msg) {
  std::vector<SerializedHandle> empty;
  SendUnsolicitedReplyWithHandles(resource, msg, &empty);
}

void PpapiHost::SendUnsolicitedReplyWithHandles(
    PP_Resource resource,
    const IPC::Message& msg,
    std::vector<SerializedHandle>* handles) {
  TRACE_EVENT2("ppapi_proxy", "PpapiHost::SendUnsolicitedReplyWithHandles",
               "Class", IPC_MESSAGE_ID_CLASS(msg.type()), "Line",
               IPC_MESSAGE_ID_LINE(msg.type()));
  if (!resource) {
    // Host probably still in a pending state, nothing we can do.
    return;
  }

  proxy::ResourceMessageReplyParams params(resource, 0);
  for (auto& handle : *handles)
    params.AppendHandle(std::move(handle));
  Send(new PpapiPluginMsg_ResourceReply(params, msg));
}

std::unique_ptr<ResourceHost> PpapiHost::CreateResourceHost(
    PP_Resource resource,
    PP_Instance instance,
    const IPC::Message& nested_msg) {
  std::unique_ptr<ResourceHost> resource_host;
  DCHECK(!host_factory_filters_.empty());  // Caller forgot to add a factory.
  for (size_t i = 0; i < host_factory_filters_.size(); i++) {
    resource_host = host_factory_filters_[i]->CreateResourceHost(
        this, resource, instance, nested_msg);
    if (resource_host.get())
      break;
  }
  return resource_host;
}

int PpapiHost::AddPendingResourceHost(
    std::unique_ptr<ResourceHost> resource_host) {
  // The resource ID should not be assigned.
  if (!resource_host.get() || resource_host->pp_resource() != 0) {
    NOTREACHED();
  }

  if (pending_resource_hosts_.size() + resources_.size()
      >= kMaxResourcesPerPlugin) {
    return 0;
  }

  int pending_id = next_pending_resource_host_id_++;
  pending_resource_hosts_[pending_id] = std::move(resource_host);
  return pending_id;
}

void PpapiHost::AddHostFactoryFilter(std::unique_ptr<HostFactory> filter) {
  host_factory_filters_.push_back(std::move(filter));
}

void PpapiHost::AddInstanceMessageFilter(
    std::unique_ptr<InstanceMessageFilter> filter) {
  instance_message_filters_.push_back(std::move(filter));
}

void PpapiHost::OnHostMsgResourceCall(
    const proxy::ResourceMessageCallParams& params,
    const IPC::Message& nested_msg) {
  TRACE_EVENT2("ppapi_proxy", "PpapiHost::OnHostMsgResourceCall", "Class",
               IPC_MESSAGE_ID_CLASS(nested_msg.type()), "Line",
               IPC_MESSAGE_ID_LINE(nested_msg.type()));
  HostMessageContext context(params);
  HandleResourceCall(params, nested_msg, &context);
}

void PpapiHost::OnHostMsgInProcessResourceCall(
    int routing_id,
    const proxy::ResourceMessageCallParams& params,
    const IPC::Message& nested_msg) {
  TRACE_EVENT2("ppapi_proxy", "PpapiHost::OnHostMsgInProcessResourceCall",
               "Class", IPC_MESSAGE_ID_CLASS(nested_msg.type()), "Line",
               IPC_MESSAGE_ID_LINE(nested_msg.type()));
  HostMessageContext context(routing_id, params);
  HandleResourceCall(params, nested_msg, &context);
}

void PpapiHost::OnHostMsgResourceSyncCall(
    const proxy::ResourceMessageCallParams& params,
    const IPC::Message& nested_msg,
    IPC::Message* reply_msg) {
  TRACE_EVENT2("ppapi_proxy", "PpapiHost::OnHostMsgResourceSyncCall", "Class",
               IPC_MESSAGE_ID_CLASS(nested_msg.type()), "Line",
               IPC_MESSAGE_ID_LINE(nested_msg.type()));
  // Sync messages should always have callback set because they always expect
  // a reply from the host.
  if (!params.has_callback()) {
    DLOG(WARNING) << "Expected callback for sync message.";
    return;
  }

  // Stash the |reply_msg| in the context so that it can be used to reply
  // to the sync message.
  HostMessageContext context(params, reply_msg);
  HandleResourceCall(params, nested_msg, &context);
}

void PpapiHost::HandleResourceCall(
    const proxy::ResourceMessageCallParams& params,
    const IPC::Message& nested_msg,
    HostMessageContext* context) {
  ResourceHost* resource_host = GetResourceHost(params.pp_resource());
  if (resource_host) {
    // CAUTION: Handling the message may cause the destruction of this object.
    resource_host->HandleMessage(nested_msg, context);
  } else {
    if (context->params.has_callback()) {
      ReplyMessageContext reply_context = context->MakeReplyMessageContext();
      reply_context.params.set_result(PP_ERROR_BADRESOURCE);
      SendReply(reply_context, context->reply_msg);
    }
  }
}

void PpapiHost::OnHostMsgResourceCreated(
    const proxy::ResourceMessageCallParams& params,
    PP_Instance instance,
    const IPC::Message& nested_msg) {
  TRACE_EVENT2("ppapi_proxy", "PpapiHost::OnHostMsgResourceCreated", "Class",
               IPC_MESSAGE_ID_CLASS(nested_msg.type()), "Line",
               IPC_MESSAGE_ID_LINE(nested_msg.type()));

  if (pending_resource_hosts_.size() + resources_.size()
      >= kMaxResourcesPerPlugin) {
    return;
  }

  // Run through all filters until one grabs this message.
  std::unique_ptr<ResourceHost> resource_host =
      CreateResourceHost(params.pp_resource(), instance, nested_msg);

  if (!resource_host.get()) {
    DLOG(WARNING) << "Resource host creation failed.";
    return;
  }

  // Resource should have been assigned a nonzero PP_Resource.
  DCHECK(resource_host->pp_resource());

  resources_[params.pp_resource()] = std::move(resource_host);
}

void PpapiHost::OnHostMsgAttachToPendingHost(PP_Resource pp_resource,
                                             int pending_host_id) {
  PendingHostResourceMap::iterator found =
      pending_resource_hosts_.find(pending_host_id);
  if (found == pending_resource_hosts_.end()) {
    // Plugin sent a bad ID.
    DLOG(WARNING) << "Bad pending host ID = " << pending_host_id;
    return;
  }
  found->second->SetPPResourceForPendingHost(pp_resource);
  resources_[pp_resource] = std::move(found->second);
  pending_resource_hosts_.erase(found);
}

void PpapiHost::OnHostMsgResourceDestroyed(PP_Resource resource) {
  ResourceMap::iterator found = resources_.find(resource);
  if (found == resources_.end()) {
    DLOG(WARNING) << "Unknown resource ID " << resource;
    return;
  }
  // Invoking the HostResource destructor might result in looking up the
  // PP_Resource in resources_. std::map is not well specified as to whether the
  // element will be there or not. Therefore, we delay destruction of the
  // HostResource until after we've made sure the map no longer contains
  // |resource|.
  std::unique_ptr<ResourceHost> delete_at_end_of_scope(
      std::move(found->second));
  resources_.erase(found);
}

ResourceHost* PpapiHost::GetResourceHost(PP_Resource resource) const {
  ResourceMap::const_iterator found = resources_.find(resource);
  return found == resources_.end() ? NULL : found->second.get();
}

}  // namespace host
}  // namespace ppapi
