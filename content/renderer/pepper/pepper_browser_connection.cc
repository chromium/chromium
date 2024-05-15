// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_browser_connection.h"

#include <limits>

#include "base/notreached.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/pepper/pepper_in_process_router.h"
#include "content/renderer/render_frame_impl.h"
#include "ipc/ipc_message_macros.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"

namespace content {

PepperBrowserConnection::PepperBrowserConnection(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame),
      RenderFrameObserverTracker<PepperBrowserConnection>(render_frame),
      next_sequence_number_(1) {}

PepperBrowserConnection::~PepperBrowserConnection() {}

bool PepperBrowserConnection::OnMessageReceived(const IPC::Message& msg) {
  // Check if the message is an in-process reply.
  if (PepperInProcessRouter::OnPluginMsgReceived(msg))
    return true;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PepperBrowserConnection, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_CreateResourceHostsFromHostReply,
                        OnMsgCreateResourceHostsFromHostReply)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PepperBrowserConnection::DidCreateInProcessInstance(
    PP_Instance instance,
    int render_frame_id,
    const GURL& document_url,
    const GURL& plugin_url) {
  GetHost()->DidCreateInProcessInstance(instance, render_frame_id, document_url,
                                        plugin_url);
}

void PepperBrowserConnection::DidDeleteInProcessInstance(PP_Instance instance) {
  GetHost()->DidDeleteInProcessInstance(instance);
}

void PepperBrowserConnection::DidCreateOutOfProcessPepperInstance(
    int32_t plugin_child_id,
    int32_t pp_instance,
    bool is_external,
    int32_t render_frame_id,
    const GURL& document_url,
    const GURL& plugin_url,
    bool is_priviledged_context) {
  GetHost()->DidCreateOutOfProcessPepperInstance(
      plugin_child_id, pp_instance, is_external, render_frame_id, document_url,
      plugin_url, is_priviledged_context);
}

void PepperBrowserConnection::DidDeleteOutOfProcessPepperInstance(
    int32_t plugin_child_id,
    int32_t pp_instance,
    bool is_external) {
  GetHost()->DidDeleteOutOfProcessPepperInstance(plugin_child_id, pp_instance,
                                                 is_external);
}

void PepperBrowserConnection::SendBrowserCreate(
    int child_process_id,
    PP_Instance instance,
    const std::vector<IPC::Message>& nested_msgs,
    PendingResourceIDCallback callback) {
  int32_t sequence_number = GetNextSequence();
  pending_create_map_[sequence_number] = std::move(callback);
  ppapi::proxy::ResourceMessageCallParams params(0, sequence_number);
  Send(new PpapiHostMsg_CreateResourceHostsFromHost(
      routing_id(), child_process_id, params, instance, nested_msgs));
}

void PepperBrowserConnection::OnMsgCreateResourceHostsFromHostReply(
    int32_t sequence_number,
    const std::vector<int>& pending_resource_host_ids) {
  // Check that the message is destined for the plugin this object is associated
  // with.
  auto it = pending_create_map_.find(sequence_number);
  if (it != pending_create_map_.end()) {
    std::move(it->second).Run(pending_resource_host_ids);
    pending_create_map_.erase(it);
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

int32_t PepperBrowserConnection::GetNextSequence() {
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

void PepperBrowserConnection::OnDestruct() {
  delete this;
}

mojom::PepperHost* PepperBrowserConnection::GetHost() {
  RenderFrameImpl* render_frame_impl =
      static_cast<RenderFrameImpl*>(render_frame());
  return render_frame_impl->GetPepperHost();
}

}  // namespace content
