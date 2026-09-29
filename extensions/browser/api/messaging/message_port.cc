// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/messaging/message_port.h"

#include <optional>
#include <utility>

#include "content/public/browser/render_frame_host.h"
#include "extensions/common/api/messaging/message.h"
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
    std::optional<base::DictValue> source_tab,
    const ExtensionApiFrameIdMap::FrameData& source_frame,
    int guest_process_id,
    int guest_render_frame_routing_id,
    const MessagingEndpoint& source_endpoint,
    const std::string& target_extension_id,
    const GURL& source_url,
    std::optional<url::Origin> source_origin,
    const std::set<base::UnguessableToken>& open_channel_tracking_ids) {}

void MessagePort::DispatchOnDisconnect(const std::string& error_message) {}

void MessagePort::OpenPort(int process_id, const PortContext& port_context) {}

void MessagePort::ClosePort(int process_id,
                            int routing_id,
                            int worker_thread_id) {}

void MessagePort::IncrementLazyKeepaliveCount(Activity::Type activity_type) {}

void MessagePort::DecrementLazyKeepaliveCount(Activity::Type activity_type) {}

void MessagePort::NotifyResponsePending() {}

void MessagePort::ClosePort(bool close_channel,
                            const std::optional<std::string>& error_message) {
  if (!weak_channel_delegate_) {
    return;
  }
  auto& context = receivers_.current_context();
  std::string error = error_message.value_or(std::string());
  weak_channel_delegate_->ClosePort(port_id_, context.first, context.second,
                                    close_channel, error);
}

void MessagePort::PostMessage(Message message) {
  if (!weak_channel_delegate_) {
    return;
  }
  // Verify `message.user_gesture()` against the sending frame's browser-side
  // `HasTransientUserActivation()` state so a compromised frame renderer cannot
  // arbitrarily forge `user_gesture` set to `true` over Mojo. Unlike
  // `messaging_util::GetMessageMetadata()` in the renderer, this check does not
  // query `LastActivationWasRestricted()` because `content::RenderFrameHost`
  // does not expose `RenderFrameHostImpl::user_activation_state_`'s restricted
  // state. Additionally, a compromised renderer can still spoof unrestricted
  // frame user activation via `blink::mojom::LocalFrameHost`'s
  // `UpdateUserActivationState()` IPC until browser-first user activation
  // tracking (https://crbug.com/848778) is implemented.
  if (message.user_gesture()) {
    // `receivers_.current_context()` is the context
    // `MessagePort::AddReceiver()` bound to the `mojom::MessagePortHost`
    // receiver this call arrived on, which identifies the endpoint that sent
    // `message`. Only frame senders are checked. Service worker senders are not
    // checked: their gesture is synthesized in the renderer by
    // `ExtensionInteractionProvider::Scope::ForWorker()` and is not tracked by
    // the browser, so there is no browser-side state to verify it against.
    const auto& [sender_process_id, sender_port_context] =
        receivers_.current_context();
    if (sender_port_context.is_for_render_frame()) {
      content::RenderFrameHost* sender_frame = content::RenderFrameHost::FromID(
          sender_process_id, sender_port_context.frame->routing_id);
      if (!sender_frame || !sender_frame->HasTransientUserActivation()) {
        message.set_user_gesture(false);
      }
    }
  }
  weak_channel_delegate_->PostMessage(port_id_, std::move(message));
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
