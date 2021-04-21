// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_VIDEO_STATS_STUB_H_
#define REMOTING_PROTOCOL_VIDEO_STATS_STUB_H_

#include <cstdint>

#include "base/macros.h"

namespace remoting {
namespace protocol {

struct HostFrameStats;

// Interface used to send video frame stats from host to client.
class VideoStatsStub {
 public:
  virtual void OnVideoFrameStats(uint32_t frame_id,
                                 const HostFrameStats& frame_stats) = 0;

 protected:
  VideoStatsStub() {}
  virtual ~VideoStatsStub() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoStatsStub);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_VIDEO_STATS_STUB_H_
