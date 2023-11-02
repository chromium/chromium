// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PERFORMANCE_TRACKER_H_
#define REMOTING_PROTOCOL_PERFORMANCE_TRACKER_H_

#include <stdint.h>

#include "remoting/base/rate_counter.h"
#include "remoting/base/running_samples.h"
#include "remoting/protocol/frame_stats.h"

namespace remoting::protocol {

// PerformanceTracker defines a bundle of performance counters and statistics
// for chromoting.
class PerformanceTracker : public FrameStatsConsumer {
 public:
  PerformanceTracker();

  PerformanceTracker(const PerformanceTracker&) = delete;
  PerformanceTracker& operator=(const PerformanceTracker&) = delete;

  ~PerformanceTracker() override;

  // Return rates and running-averages for different metrics.
  double video_bandwidth() const { return video_bandwidth_.Rate(); }
  double video_frame_rate() const { return video_frame_rate_.Rate(); }
  double video_packet_rate() const { return video_packet_rate_.Rate(); }
  const RunningSamples& video_capture_ms() const { return video_capture_ms_; }
  const RunningSamples& video_encode_ms() const { return video_encode_ms_; }
  const RunningSamples& video_decode_ms() const { return video_decode_ms_; }
  const RunningSamples& video_paint_ms() const { return video_paint_ms_; }
  const RunningSamples& round_trip_ms() const { return round_trip_ms_; }

  // FrameStatsConsumer interface.
  void OnVideoFrameStats(const FrameStats& stats) override;

 private:
  // The video and packet rate metrics below are updated per video packet
  // received and then, for reporting, averaged over a 1s time-window.
  // Bytes per second for non-empty video-packets.
  RateCounter video_bandwidth_;

  // Frames per second for non-empty video-packets.
  RateCounter video_frame_rate_;

  // Video packets per second, including empty video-packets.
  // This will be greater than the frame rate, as individual frames are
  // contained in packets, some of which might be empty (e.g. when there are no
  // screen changes).
  RateCounter video_packet_rate_;

  // The following running-averages are uploaded to UMA per video packet and
  // also used for display to users, averaged over the N most recent samples.
  // N = kLatencySampleSize.
  RunningSamples video_capture_ms_;
  RunningSamples video_encode_ms_;
  RunningSamples video_decode_ms_;
  RunningSamples video_paint_ms_;
  RunningSamples round_trip_ms_;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_PERFORMANCE_TRACKER_H_
