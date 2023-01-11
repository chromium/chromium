// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/client_control_dispatcher.h"

#include <stdint.h>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "net/socket/stream_socket.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/base/constants.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/internal.pb.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/message_pipe.h"
#include "remoting/protocol/message_serialization.h"

namespace remoting::protocol {

namespace {

// 32-bit BGRA is 4 bytes per pixel.
const int kBytesPerPixel = 4;

bool CursorShapeIsValid(const CursorShapeInfo& cursor_shape) {
  if (!cursor_shape.has_data() || !cursor_shape.has_width() ||
      !cursor_shape.has_height() || !cursor_shape.has_hotspot_x() ||
      !cursor_shape.has_hotspot_y()) {
    LOG(ERROR) << "Cursor shape is missing required fields.";
    return false;
  }

  int width = cursor_shape.width();
  int height = cursor_shape.height();

  // Verify that |width| and |height| are within sane limits. Otherwise integer
  // overflow can occur while calculating |cursor_total_bytes| below.
  if (width < 0 || width > (SHRT_MAX / 2) || height < 0 ||
      height > (SHRT_MAX / 2)) {
    LOG(ERROR) << "Cursor dimensions are out of bounds for SetCursor: " << width
               << "x" << height;
    return false;
  }

  uint32_t cursor_total_bytes = width * height * kBytesPerPixel;
  if (cursor_shape.data().size() < cursor_total_bytes) {
    LOG(ERROR) << "Expected " << cursor_total_bytes << " bytes for a " << width
               << "x" << height << " cursor. Only received "
               << cursor_shape.data().size() << " bytes";
    return false;
  }

  return true;
}

}  // namespace

ClientControlDispatcher::ClientControlDispatcher()
    : ChannelDispatcherBase(kControlChannelName) {}
ClientControlDispatcher::~ClientControlDispatcher() = default;

void ClientControlDispatcher::InjectClipboardEvent(
    const ClipboardEvent& event) {
  ControlMessage message;
  message.mutable_clipboard_event()->CopyFrom(event);
  message_pipe()->Send(&message, {});
}

void ClientControlDispatcher::NotifyClientResolution(
    const ClientResolution& resolution) {
  ControlMessage message;
  message.mutable_client_resolution()->CopyFrom(resolution);
  message_pipe()->Send(&message, {});
}

void ClientControlDispatcher::ControlVideo(const VideoControl& video_control) {
  ControlMessage message;
  message.mutable_video_control()->CopyFrom(video_control);
  message_pipe()->Send(&message, {});
}

void ClientControlDispatcher::ControlAudio(const AudioControl& audio_control) {
  ControlMessage message;
  message.mutable_audio_control()->CopyFrom(audio_control);
  message_pipe()->Send(&message, {});
}

void ClientControlDispatcher::SetCapabilities(
    const Capabilities& capabilities) {
  ControlMessage message;
  message.mutable_capabilities()->CopyFrom(capabilities);
  message_pipe()->Send(&message, {});
}

void ClientControlDispatcher::RequestPairing(
    const PairingRequest& pairing_request) {
  ControlMessage message;
  message.mutable_pairing_request()->CopyFrom(pairing_request);
  message_pipe()->Send(&message, {});
}

void ClientControlDispatcher::DeliverClientMessage(
    const ExtensionMessage& message) {
  ControlMessage control_message;
  control_message.mutable_extension_message()->CopyFrom(message);
  message_pipe()->Send(&control_message, {});
}

void ClientControlDispatcher::SelectDesktopDisplay(
    const SelectDesktopDisplayRequest& select_display) {
  ControlMessage message;
  message.mutable_select_display()->CopyFrom(select_display);
  message_pipe()->Send(&message, {});
}

void ClientControlDispatcher::ControlPeerConnection(
    const protocol::PeerConnectionParameters& parameters) {
  ControlMessage message;
  message.mutable_peer_connection_parameters()->CopyFrom(parameters);
  message_pipe()->Send(&message, {});
}

void ClientControlDispatcher::SetVideoLayout(const VideoLayout& video_layout) {
  ControlMessage message;
  message.mutable_video_layout()->CopyFrom(video_layout);
  message_pipe()->Send(&message, {});
}

void ClientControlDispatcher::OnIncomingMessage(
    std::unique_ptr<CompoundBuffer> buffer) {
  DCHECK(client_stub_);
  DCHECK(clipboard_stub_);

  std::unique_ptr<ControlMessage> message =
      ParseMessage<ControlMessage>(buffer.get());
  if (!message) {
    return;
  }

  if (message->has_clipboard_event()) {
    clipboard_stub_->InjectClipboardEvent(message->clipboard_event());
  } else if (message->has_capabilities()) {
    client_stub_->SetCapabilities(message->capabilities());
  } else if (message->has_cursor_shape()) {
    if (CursorShapeIsValid(message->cursor_shape())) {
      client_stub_->SetCursorShape(message->cursor_shape());
    }
  } else if (message->has_pairing_response()) {
    client_stub_->SetPairingResponse(message->pairing_response());
  } else if (message->has_extension_message()) {
    client_stub_->DeliverHostMessage(message->extension_message());
  } else if (message->has_video_layout()) {
    client_stub_->SetVideoLayout(message->video_layout());
  } else {
    LOG(WARNING) << "Unknown control message received.";
  }
}

}  // namespace remoting::protocol
