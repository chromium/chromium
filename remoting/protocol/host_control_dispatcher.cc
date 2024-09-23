// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/host_control_dispatcher.h"

#include "base/functional/callback_helpers.h"
#include "net/socket/stream_socket.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/base/constants.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/internal.pb.h"
#include "remoting/protocol/clipboard_stub.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/message_pipe.h"
#include "remoting/protocol/message_serialization.h"

namespace remoting::protocol {

HostControlDispatcher::HostControlDispatcher()
    : ChannelDispatcherBase(kControlChannelName) {}
HostControlDispatcher::~HostControlDispatcher() = default;

void HostControlDispatcher::SetCapabilities(const Capabilities& capabilities) {
  ControlMessage message;
  message.mutable_capabilities()->CopyFrom(capabilities);
  message_pipe()->Send(&message, {});
}

void HostControlDispatcher::SetPairingResponse(
    const PairingResponse& pairing_response) {
  ControlMessage message;
  message.mutable_pairing_response()->CopyFrom(pairing_response);
  message_pipe()->Send(&message, {});
}

void HostControlDispatcher::DeliverHostMessage(
    const ExtensionMessage& message) {
  ControlMessage control_message;
  control_message.mutable_extension_message()->CopyFrom(message);
  message_pipe()->Send(&control_message, {});
}

void HostControlDispatcher::SetVideoLayout(const VideoLayout& layout) {
  ControlMessage message;
  message.mutable_video_layout()->CopyFrom(layout);
  message_pipe()->Send(&message, {});
}

void HostControlDispatcher::SetTransportInfo(
    const TransportInfo& transport_info) {
  ControlMessage message;
  message.mutable_transport_info()->CopyFrom(transport_info);
  message_pipe()->Send(&message, {});
}

void HostControlDispatcher::SetActiveDisplay(
    const ActiveDisplay& active_display) {
  ControlMessage message;
  message.mutable_active_display_changed()->CopyFrom(active_display);
  message_pipe()->Send(&message, {});
}

void HostControlDispatcher::InjectClipboardEvent(const ClipboardEvent& event) {
  ControlMessage message;
  message.mutable_clipboard_event()->CopyFrom(event);
  std::size_t message_size = message.ByteSizeLong();
  if (message_size > max_message_size_) {
    // Better to drop the event than drop the connection, which can happen if
    // the browser receives a message larger than it can handle.
    LOG(WARNING) << "Clipboard message dropped because message size "
                 << message_size << " is larger than " << max_message_size_;
    return;
  }
  message_pipe()->Send(&message, {});
}

void HostControlDispatcher::SetCursorShape(
    const CursorShapeInfo& cursor_shape) {
  ControlMessage message;
  message.mutable_cursor_shape()->CopyFrom(cursor_shape);
  std::size_t message_size = message.ByteSizeLong();
  if (message_size > max_message_size_) {
    // Better to drop the event than drop the connection, which can happen if
    // the browser receives a message larger than it can handle.
    LOG(WARNING) << "Cursor message dropped because message size "
                 << message_size << " is larger than " << max_message_size_;
    return;
  }
  message_pipe()->Send(&message, {});
}

void HostControlDispatcher::SetKeyboardLayout(const KeyboardLayout& layout) {
  ControlMessage message;
  message.mutable_keyboard_layout()->CopyFrom(layout);
  message_pipe()->Send(&message, {});
}

void HostControlDispatcher::OnIncomingMessage(
    std::unique_ptr<CompoundBuffer> buffer) {
  DCHECK(clipboard_stub_);
  DCHECK(host_stub_);

  std::unique_ptr<ControlMessage> message =
      ParseMessage<ControlMessage>(buffer.get());
  if (!message) {
    return;
  }

  // TODO(sergeyu): Move message validation from the message handlers here.
  if (message->has_clipboard_event()) {
    clipboard_stub_->InjectClipboardEvent(message->clipboard_event());
  } else if (message->has_client_resolution()) {
    const ClientResolution& resolution = message->client_resolution();
    if ((resolution.has_width_pixels() && resolution.width_pixels() <= 0) ||
        (resolution.has_height_pixels() && resolution.height_pixels() <= 0)) {
      LOG(ERROR) << "Received invalid ClientResolution message.";
      return;
    }
    host_stub_->NotifyClientResolution(resolution);
  } else if (message->has_video_control()) {
    host_stub_->ControlVideo(message->video_control());
  } else if (message->has_audio_control()) {
    host_stub_->ControlAudio(message->audio_control());
  } else if (message->has_capabilities()) {
    host_stub_->SetCapabilities(message->capabilities());
  } else if (message->has_pairing_request()) {
    host_stub_->RequestPairing(message->pairing_request());
  } else if (message->has_extension_message()) {
    host_stub_->DeliverClientMessage(message->extension_message());
  } else if (message->has_select_display()) {
    host_stub_->SelectDesktopDisplay(message->select_display());
  } else if (message->has_peer_connection_parameters()) {
    host_stub_->ControlPeerConnection(message->peer_connection_parameters());
  } else if (message->has_video_layout()) {
    host_stub_->SetVideoLayout(message->video_layout());
  } else if (message->has_cursor_shape()) {
    LOG(WARNING) << "Unexpected control message received: CursorShape";
  } else if (message->has_pairing_response()) {
    LOG(WARNING) << "Unexpected control message received: PairingResponse";
  } else if (message->has_keyboard_layout()) {
    LOG(WARNING) << "Unexpected control message received: KeyboardLayout";
  } else if (message->has_transport_info()) {
    LOG(WARNING) << "Unexpected control message received: TransportInfo";
  } else if (message->has_active_display_changed()) {
    LOG(WARNING) << "Unexpected control message received: ActiveDisplayChanged";
  } else {
    LOG(WARNING) << "Unknown control message received";
  }
}

}  // namespace remoting::protocol
