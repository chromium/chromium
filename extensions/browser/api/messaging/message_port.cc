// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/messaging/message_port.h"

#include "extensions/common/api/messaging/port_context.h"

namespace extensions {

MessagePort::MessagePort(base::WeakPtr<ChannelDelegate> channel_delegate,
                         const PortId& port_id)
    : weak_channel_delegate_(channel_delegate), port_id_(port_id) {}

MessagePort::~MessagePort() = default;

void MessagePort::RemoveCommonFrames(const MessagePort& port) {}

bool MessagePort::HasFrame(
    const content::GlobalRenderFrameHostToken& frame_token) const {
  return false;
}

void MessagePort::RevalidatePort() {}

void MessagePort::DispatchOnConnect(
    mojom::ChannelType channel_type,
    const std::string& channel_name,
    std::optional<base::Value::Dict> source_tab,
    const ExtensionApiFrameIdMap::FrameData& source_frame,
    int guest_process_id,
    int guest_render_frame_routing_id,
    const MessagingEndpoint& source_endpoint,
    const std::string& target_extension_id,
    const GURL& source_url,
    std::optional<url::Origin> source_origin) {}

void MessagePort::DispatchOnDisconnect(const std::string& error_message) {}

void MessagePort::OpenPort(int process_id, const PortContext& port_context) {}

void MessagePort::ClosePort(int process_id,
                            int routing_id,
                            int worker_thread_id) {}

void MessagePort::IncrementLazyKeepaliveCount(Activity::Type activity_type) {}

void MessagePort::DecrementLazyKeepaliveCount(Activity::Type activity_type) {}

void MessagePort::NotifyResponsePending() {}

void MessagePort::ClosePort(bool close_channel) {
  if (!weak_channel_delegate_) {
    return;
  }
  auto& context = receivers_.current_context();
  weak_channel_delegate_->ClosePort(port_id_, context.first, context.second,
                                    close_channel);
}

void MessagePort::PostMessage(Message message) {
  if (!weak_channel_delegate_) {
    return;
  }
  weak_channel_delegate_->PostMessage(port_id_, message);
}

void MessagePort::ResponsePending() {
  if (!weak_channel_delegate_) {
    return;
  }
  weak_channel_delegate_->NotifyResponsePending(port_id_);
}

void MessagePort::AddReceiver(
    mojo::PendingAssociatedReceiver<mojom::MessagePortHost> receiver,
    int render_process_id,
    const PortContext& port_context) {
  receivers_.Add(this, std::move(receiver),
                 std::make_pair(render_process_id, port_context));
}

}  // namespace extensions
