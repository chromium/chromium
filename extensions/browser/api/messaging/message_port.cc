// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/messaging/message_port.h"

namespace extensions {

MessagePort::MessagePort() = default;
MessagePort::~MessagePort() = default;

void MessagePort::RemoveCommonFrames(const MessagePort& port) {}

bool MessagePort::HasFrame(content::RenderFrameHost* rfh) const {
  return false;
}

void MessagePort::RevalidatePort() {}

void MessagePort::DispatchOnConnect(
    const std::string& channel_name,
    std::unique_ptr<base::DictionaryValue> source_tab,
    int source_frame_id,
    int guest_process_id,
    int guest_render_frame_routing_id,
    const MessagingEndpoint& source_endpoint,
    const std::string& target_extension_id,
    const GURL& source_url) {}

void MessagePort::DispatchOnDisconnect(const std::string& error_message) {}

void MessagePort::OpenPort(int process_id, const PortContext& port_context) {}

void MessagePort::ClosePort(int process_id,
                            int routing_id,
                            int worker_thread_id) {}

void MessagePort::IncrementLazyKeepaliveCount() {}

void MessagePort::DecrementLazyKeepaliveCount() {}

}  // namespace extensions
