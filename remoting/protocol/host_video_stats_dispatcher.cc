// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/host_video_stats_dispatcher.h"

#include <utility>

#include "base/bind.h"
#include "net/socket/stream_socket.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/base/constants.h"
#include "remoting/proto/video_stats.pb.h"
#include "remoting/protocol/frame_stats.h"
#include "remoting/protocol/message_pipe.h"
#include "remoting/protocol/message_serialization.h"
#include "remoting/protocol/video_stats_stub.h"

namespace remoting {
namespace protocol {

HostVideoStatsDispatcher::HostVideoStatsDispatcher(
    const std::string& stream_name)
    : ChannelDispatcherBase(kVideoStatsChannelNamePrefix + stream_name) {}

HostVideoStatsDispatcher::~HostVideoStatsDispatcher() = default;

void HostVideoStatsDispatcher::OnVideoFrameStats(uint32_t frame_id,
                                                 const HostFrameStats& stats) {
  FrameStatsMessage message;
  message.set_frame_id(frame_id);
  stats.ToFrameStatsMessage(&message);
  message_pipe()->Send(&message, {});
}

void HostVideoStatsDispatcher::OnIncomingMessage(
    std::unique_ptr<CompoundBuffer> message) {}

}  // namespace protocol
}  // namespace remoting
