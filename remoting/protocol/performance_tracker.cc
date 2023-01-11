// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/performance_tracker.h"

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "remoting/protocol/frame_stats.h"

namespace {

// Constant used to calculate the average for rate metrics.
constexpr int kStatsUpdatePeriodSeconds = 1;

// We take the last 10 latency numbers and report the average.
constexpr int kLatencySampleSize = 10;

}  // namespace

namespace remoting::protocol {

PerformanceTracker::PerformanceTracker()
    : video_bandwidth_(base::Seconds(kStatsUpdatePeriodSeconds)),
      video_frame_rate_(base::Seconds(kStatsUpdatePeriodSeconds)),
      video_packet_rate_(base::Seconds(kStatsUpdatePeriodSeconds)),
      video_capture_ms_(kLatencySampleSize),
      video_encode_ms_(kLatencySampleSize),
      video_decode_ms_(kLatencySampleSize),
      video_paint_ms_(kLatencySampleSize),
      round_trip_ms_(kLatencySampleSize) {}

PerformanceTracker::~PerformanceTracker() = default;

void PerformanceTracker::OnVideoFrameStats(const FrameStats& stats) {
  // Record this received packet, even if it is empty.
  video_packet_rate_.Record(1);

  // Use only non-empty frames to estimate frame rate.
  if (stats.host_stats.frame_size) {
    video_frame_rate_.Record(1);
  }

  video_bandwidth_.Record(stats.host_stats.frame_size);

  if (stats.host_stats.capture_delay != base::TimeDelta::Max()) {
    video_capture_ms_.Record(stats.host_stats.capture_delay.InMilliseconds());
  }

  if (stats.host_stats.encode_delay != base::TimeDelta::Max()) {
    video_encode_ms_.Record(stats.host_stats.encode_delay.InMilliseconds());
  }

  DCHECK(!stats.client_stats.time_received.is_null());

  // Report decode and render delay only for non-empty frames.
  if (stats.host_stats.frame_size > 0) {
    DCHECK(!stats.client_stats.time_rendered.is_null());
    DCHECK(!stats.client_stats.time_decoded.is_null());
    base::TimeDelta decode_delay =
        stats.client_stats.time_decoded - stats.client_stats.time_received;
    video_decode_ms_.Record(decode_delay.InMilliseconds());

    base::TimeDelta render_delay =
        stats.client_stats.time_rendered - stats.client_stats.time_decoded;
    video_paint_ms_.Record(render_delay.InMilliseconds());
  }

  // |latest_event_timestamp| is set only for the first frame after an input
  // event.
  if (stats.host_stats.latest_event_timestamp.is_null()) {
    return;
  }

  // For empty frames use time_received as time_rendered.
  base::TimeTicks time_rendered = (stats.host_stats.frame_size > 0)
                                      ? stats.client_stats.time_rendered
                                      : stats.client_stats.time_received;
  base::TimeDelta round_trip_latency =
      time_rendered - stats.host_stats.latest_event_timestamp;
  round_trip_ms_.Record(round_trip_latency.InMilliseconds());
}

}  // namespace remoting::protocol
