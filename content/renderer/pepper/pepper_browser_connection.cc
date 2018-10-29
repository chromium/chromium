// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_browser_connection.h"

#include <limits>

#include "base/logging.h"
#include "content/common/frame_messages.h"
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
  // We don't need to know if it's a privileged context for in-process plugins.
  // In process plugins are deprecated and the only in-process plugin that
  // exists is the "NaCl plugin" which will never need to know this.
  bool is_privileged_context = false;
  Send(new FrameHostMsg_DidCreateInProcessInstance(
      instance,
      // Browser provides the render process id.
      PepperRendererInstanceData(0, render_frame_id, document_url, plugin_url,
                                 is_privileged_context)));
}

void PepperBrowserConnection::DidDeleteInProcessInstance(PP_Instance instance) {
  Send(new FrameHostMsg_DidDeleteInProcessInstance(instance));
}

void PepperBrowserConnection::SendBrowserCreate(
    int child_process_id,
    PP_Instance instance,
    const std::vector<IPC::Message>& nested_msgs,
    const PendingResourceIDCallback& callback) {
  int32_t sequence_number = GetNextSequence();
  pending_create_map_[sequence_number] = callback;
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
    it->second.Run(pending_resource_host_ids);
    pending_create_map_.erase(it);
  } else {
    NOTREACHED();
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

}  // namespace content
