// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_VIDEO_STATS_STUB_H_
#define REMOTING_PROTOCOL_VIDEO_STATS_STUB_H_

#include <cstdint>

namespace remoting::protocol {

struct HostFrameStats;

// Interface used to send video frame stats from host to client.
class VideoStatsStub {
 public:
  VideoStatsStub(const VideoStatsStub&) = delete;
  VideoStatsStub& operator=(const VideoStatsStub&) = delete;

  virtual void OnVideoFrameStats(uint32_t frame_id,
                                 const HostFrameStats& frame_stats) = 0;

 protected:
  VideoStatsStub() = default;
  virtual ~VideoStatsStub() = default;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_VIDEO_STATS_STUB_H_
