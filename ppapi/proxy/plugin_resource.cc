// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/plugin_resource.h"

#include <limits>

#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/ppapi_globals.h"

namespace ppapi {
namespace proxy {

void SafeRunCallback(scoped_refptr<TrackedCallback>* callback, int32_t error) {
  if (TrackedCallback::IsPending(*callback)) {
    scoped_refptr<TrackedCallback> temp;
    callback->swap(temp);
    temp->Run(error);
  }
}

PluginResource::PluginResource(Connection connection, PP_Instance instance)
    : Resource(OBJECT_IS_PROXY, instance),
      connection_(connection),
      next_sequence_number_(1),
      sent_create_to_browser_(false),
      sent_create_to_renderer_(false),
      resource_reply_thread_registrar_(
          PpapiGlobals::Get()->IsPluginGlobals() ?
              PluginGlobals::Get()->resource_reply_thread_registrar() : NULL) {
}

PluginResource::~PluginResource() {
  if (sent_create_to_browser_) {
    connection_.browser_sender()->Send(
        new PpapiHostMsg_ResourceDestroyed(pp_resource()));
  }
  if (sent_create_to_renderer_) {
    connection_.GetRendererSender()->Send(
        new PpapiHostMsg_ResourceDestroyed(pp_resource()));
  }

  if (resource_reply_thread_registrar_.get())
    resource_reply_thread_registrar_->Unregister(pp_resource());
}

void PluginResource::OnReplyReceived(
    const proxy::ResourceMessageReplyParams& params,
    const IPC::Message& msg) {
  TRACE_EVENT2("ppapi proxy", "PluginResource::OnReplyReceived",
               "Class", IPC_MESSAGE_ID_CLASS(msg.type()),
               "Line", IPC_MESSAGE_ID_LINE(msg.type()));
  // Grab the callback for the reply sequence number and run it with |msg|.
  CallbackMap::iterator it = callbacks_.find(params.sequence());
  if (it == callbacks_.end()) {
    DCHECK(false) << "Callback does not exist for an expected sequence number.";
  } else {
    scoped_refptr<PluginResourceCallbackBase> callback = it->second;
    callbacks_.erase(it);
    callback->Run(params, msg);
  }
}

void PluginResource::NotifyLastPluginRefWasDeleted() {
  Resource::NotifyLastPluginRefWasDeleted();

  // The callbacks may hold referrences to this object. Normally, we will get
  // reply messages from the host side and remove them. However, it is possible
  // that some replies from the host never arrive, e.g., the corresponding
  // renderer crashes. In that case, we have to clean up the callbacks,
  // otherwise this object will live forever.
  callbacks_.clear();
}

void PluginResource::NotifyInstanceWasDeleted() {
  Resource::NotifyInstanceWasDeleted();

  // Please see comments in NotifyLastPluginRefWasDeleted() about why we must
  // clean up the callbacks.
  // It is possible that NotifyLastPluginRefWasDeleted() is never called for a
  // resource. For example, those singleton-style resources such as
  // GamepadResource never expose references to the plugin and thus won't
  // receive a NotifyLastPluginRefWasDeleted() call. For those resources, we
  // need to clean up callbacks when the instance goes away.
  callbacks_.clear();
}

void PluginResource::SendCreate(Destination dest, const IPC::Message& msg) {
  TRACE_EVENT2("ppapi proxy", "PluginResource::SendCreate",
               "Class", IPC_MESSAGE_ID_CLASS(msg.type()),
               "Line", IPC_MESSAGE_ID_LINE(msg.type()));
  if (dest == RENDERER) {
    DCHECK(!sent_create_to_renderer_);
    sent_create_to_renderer_ = true;
  } else {
    DCHECK(!sent_create_to_browser_);
    sent_create_to_browser_ = true;
  }
  ResourceMessageCallParams params(pp_resource(), GetNextSequence());
  GetSender(dest)->Send(
      new PpapiHostMsg_ResourceCreated(params, pp_instance(), msg));
}

void PluginResource::AttachToPendingHost(Destination dest,
                                         int pending_host_id) {
  // Connecting to a pending host is a replacement for "create".
  if (dest == RENDERER) {
    DCHECK(!sent_create_to_renderer_);
    sent_create_to_renderer_ = true;
  } else {
    DCHECK(!sent_create_to_browser_);
    sent_create_to_browser_ = true;
  }
  GetSender(dest)->Send(
      new PpapiHostMsg_AttachToPendingHost(pp_resource(), pending_host_id));
}

void PluginResource::Post(Destination dest, const IPC::Message& msg) {
  TRACE_EVENT2("ppapi proxy", "PluginResource::Post",
               "Class", IPC_MESSAGE_ID_CLASS(msg.type()),
               "Line", IPC_MESSAGE_ID_LINE(msg.type()));
  ResourceMessageCallParams params(pp_resource(), GetNextSequence());
  SendResourceCall(dest, params, msg);
}

bool PluginResource::SendResourceCall(
    Destination dest,
    const ResourceMessageCallParams& call_params,
    const IPC::Message& nested_msg) {
  // For in-process plugins, we need to send the routing ID with the request.
  // The browser then uses that routing ID when sending the reply so it will be
  // routed back to the correct RenderFrameImpl.
  if (dest == BROWSER && connection_.in_process()) {
    return GetSender(dest)->Send(new PpapiHostMsg_InProcessResourceCall(
        connection_.browser_sender_routing_id(), call_params, nested_msg));
  } else {
    return GetSender(dest)->Send(
        new PpapiHostMsg_ResourceCall(call_params, nested_msg));
  }
}

int32_t PluginResource::GenericSyncCall(
    Destination dest,
    const IPC::Message& msg,
    IPC::Message* reply,
    ResourceMessageReplyParams* reply_params) {
  TRACE_EVENT2("ppapi proxy", "PluginResource::GenericSyncCall",
               "Class", IPC_MESSAGE_ID_CLASS(msg.type()),
               "Line", IPC_MESSAGE_ID_LINE(msg.type()));
  ResourceMessageCallParams params(pp_resource(), GetNextSequence());
  params.set_has_callback();
  bool success = GetSender(dest)->Send(new PpapiHostMsg_ResourceSyncCall(
      params, msg, reply_params, reply));
  if (success)
    return reply_params->result();
  return PP_ERROR_FAILED;
}

int32_t PluginResource::GetNextSequence() {
  // Return the value with wraparound, making sure we don't make a sequence
  // number with a 0 ID. Note that signed wraparound is undefined in C++ so we
  // manually check.
  int32_t ret = next_sequence_number_;
  if (next_sequence_number_ == std::numeric_limits<int32_t>::max())
    next_sequence_number_ = 1;  // Skip 0 which is invalid.
  else
    next_sequence_number_++;
  return ret;
}

}  // namespace proxy
}  // namespace ppapi
