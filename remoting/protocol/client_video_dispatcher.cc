// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/client_video_dispatcher.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "net/socket/stream_socket.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/base/constants.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/message_pipe.h"
#include "remoting/protocol/message_serialization.h"
#include "remoting/protocol/video_stub.h"

namespace remoting::protocol {

struct ClientVideoDispatcher::PendingFrame {
  explicit PendingFrame(int frame_id) : frame_id(frame_id), done(false) {}
  int frame_id;
  bool done;
};

ClientVideoDispatcher::ClientVideoDispatcher(VideoStub* video_stub,
                                             ClientStub* client_stub)
    : ChannelDispatcherBase(kVideoChannelName),
      video_stub_(video_stub),
      client_stub_(client_stub) {}

ClientVideoDispatcher::~ClientVideoDispatcher() = default;

void ClientVideoDispatcher::OnIncomingMessage(
    std::unique_ptr<CompoundBuffer> message) {
  std::unique_ptr<VideoPacket> video_packet =
      ParseMessage<VideoPacket>(message.get());
  if (!video_packet) {
    return;
  }

  int frame_id = video_packet->frame_id();

  if (!video_packet->has_frame_id()) {
    video_stub_->ProcessVideoPacket(std::move(video_packet), base::DoNothing());
    return;
  }

  bool resolution_changed = false;

  if (video_packet->format().has_screen_width() &&
      video_packet->format().has_screen_height()) {
    webrtc::DesktopSize frame_size(video_packet->format().screen_width(),
                                   video_packet->format().screen_height());
    if (!screen_size_.equals(frame_size)) {
      screen_size_ = frame_size;
      resolution_changed = true;
    }
  }

  if (video_packet->format().has_x_dpi() &&
      video_packet->format().has_y_dpi()) {
    webrtc::DesktopVector screen_dpi(video_packet->format().x_dpi(),
                                     video_packet->format().y_dpi());
    if (!screen_dpi_.equals(screen_dpi)) {
      screen_dpi_ = screen_dpi;
      resolution_changed = true;
    }
  }

  // Simulate DesktopLayout message whenever screen size/resolution changes.
  if (resolution_changed) {
    VideoLayout layout;
    VideoTrackLayout* video_track = layout.add_video_track();
    video_track->set_position_x(0);
    video_track->set_position_y(0);
    video_track->set_width(screen_size_.width() * kDefaultDpi /
                           screen_dpi_.x());
    video_track->set_height(screen_size_.height() * kDefaultDpi /
                            screen_dpi_.y());
    video_track->set_x_dpi(screen_dpi_.x());
    video_track->set_y_dpi(screen_dpi_.y());
    client_stub_->SetVideoLayout(layout);
  }

  auto pending_frame =
      pending_frames_.insert(pending_frames_.end(), PendingFrame(frame_id));

  video_stub_->ProcessVideoPacket(
      std::move(video_packet),
      base::BindOnce(&ClientVideoDispatcher::OnPacketDone,
                     weak_factory_.GetWeakPtr(), pending_frame));
}

void ClientVideoDispatcher::OnPacketDone(
    PendingFramesList::iterator pending_frame) {
  // Mark the frame as done.
  DCHECK(!pending_frame->done);
  pending_frame->done = true;

  // Send VideoAck for all packets in the head of the queue that have finished
  // rendering.
  while (!pending_frames_.empty() && pending_frames_.front().done) {
    VideoAck ack_message;
    ack_message.set_frame_id(pending_frames_.front().frame_id);
    message_pipe()->Send(&ack_message, {});
    pending_frames_.pop_front();
  }
}

}  // namespace remoting::protocol
