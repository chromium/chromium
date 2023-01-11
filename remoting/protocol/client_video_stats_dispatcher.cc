// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/client_video_stats_dispatcher.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "net/socket/stream_socket.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/proto/video_stats.pb.h"
#include "remoting/protocol/frame_stats.h"
#include "remoting/protocol/message_pipe.h"
#include "remoting/protocol/message_serialization.h"
#include "remoting/protocol/video_stats_stub.h"

namespace remoting::protocol {

ClientVideoStatsDispatcher::ClientVideoStatsDispatcher(
    const std::string& stream_name,
    VideoStatsStub* video_stats_stub)
    : ChannelDispatcherBase(kVideoStatsChannelNamePrefix + stream_name),
      video_stats_stub_(video_stats_stub) {}

ClientVideoStatsDispatcher::~ClientVideoStatsDispatcher() = default;

void ClientVideoStatsDispatcher::OnIncomingMessage(
    std::unique_ptr<CompoundBuffer> message) {
  std::unique_ptr<FrameStatsMessage> stats_proto =
      ParseMessage<FrameStatsMessage>(message.get());
  if (!stats_proto) {
    return;
  }

  if (!stats_proto->has_frame_id()) {
    LOG(ERROR) << "Received invalid FrameStatsMessage.";
    return;
  }
  video_stats_stub_->OnVideoFrameStats(
      stats_proto->frame_id(),
      HostFrameStats::FromFrameStatsMessage(*stats_proto));
}

}  // namespace remoting::protocol
