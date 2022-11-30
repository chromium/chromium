// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_HOST_VIDEO_STATS_DISPATCHER_H_
#define REMOTING_PROTOCOL_HOST_VIDEO_STATS_DISPATCHER_H_

#include "base/memory/weak_ptr.h"
#include "remoting/protocol/channel_dispatcher_base.h"
#include "remoting/protocol/video_stats_stub.h"

namespace remoting::protocol {

class HostVideoStatsDispatcher : public ChannelDispatcherBase,
                                 public VideoStatsStub {
 public:
  explicit HostVideoStatsDispatcher(const std::string& stream_name);

  HostVideoStatsDispatcher(const HostVideoStatsDispatcher&) = delete;
  HostVideoStatsDispatcher& operator=(const HostVideoStatsDispatcher&) = delete;

  ~HostVideoStatsDispatcher() override;

  base::WeakPtr<HostVideoStatsDispatcher> GetWeakPtr();

  // VideoStatsStub interface.
  void OnVideoFrameStats(uint32_t frame_id,
                         const HostFrameStats& frame_stats) override;

 private:
  void OnIncomingMessage(std::unique_ptr<CompoundBuffer> message) override;

  base::WeakPtrFactory<HostVideoStatsDispatcher> weak_factory_{this};
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_HOST_VIDEO_STATS_DISPATCHER_H_
