// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CLIENT_VIDEO_STATS_DISPATCHER_H_
#define REMOTING_PROTOCOL_CLIENT_VIDEO_STATS_DISPATCHER_H_

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/channel_dispatcher_base.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting::protocol {

class VideoStatsStub;

class ClientVideoStatsDispatcher : public ChannelDispatcherBase {
 public:
  ClientVideoStatsDispatcher(const std::string& stream_name,
                             VideoStatsStub* video_stats_stub);

  ClientVideoStatsDispatcher(const ClientVideoStatsDispatcher&) = delete;
  ClientVideoStatsDispatcher& operator=(const ClientVideoStatsDispatcher&) =
      delete;

  ~ClientVideoStatsDispatcher() override;

 private:
  void OnIncomingMessage(std::unique_ptr<CompoundBuffer> message) override;

  raw_ptr<VideoStatsStub> video_stats_stub_;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_CLIENT_VIDEO_STATS_DISPATCHER_H_
