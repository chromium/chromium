// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/performance_tracker.h"

#include "base/bind.h"
#include "remoting/protocol/frame_stats.h"

namespace {

// We take the last 10 latency numbers and report the average.
const int kLatencySampleSize = 10;

// UMA histogram names.
const char kRoundTripLatencyHistogram[] = "Chromoting.Video.RoundTripLatency";
const char kVideoCaptureLatencyHistogram[] = "Chromoting.Video.CaptureLatency";
const char kVideoEncodeLatencyHistogram[] = "Chromoting.Video.EncodeLatency";
const char kVideoDecodeLatencyHistogram[] = "Chromoting.Video.DecodeLatency";
const char kVideoPaintLatencyHistogram[] = "Chromoting.Video.PaintLatency";
const char kVideoFrameRateHistogram[] = "Chromoting.Video.FrameRate";
const char kVideoPacketRateHistogram[] = "Chromoting.Video.PacketRate";
const char kVideoBandwidthHistogram[] = "Chromoting.Video.Bandwidth";
const char kCapturePendingLatencyHistogram[] =
    "Chromoting.Video.CapturePendingLatency";
const char kCaptureOverheadHistogram[] = "Chromoting.Video.CaptureOverhead";
const char kEncodePendingLatencyHistogram[] =
    "Chromoting.Video.EncodePendingLatency";
const char kSendPendingLatencyHistogram[] =
    "Chromoting.Video.SendPendingLatency";
const char kNetworkLatencyHistogram[] = "Chromoting.Video.NetworkLatency";

// Custom count and custom time histograms are log-scaled by default. This
// results in fine-grained buckets at lower values and wider-ranged buckets
// closer to the maximum.
// The values defined for each histogram below are based on the 99th percentile
// numbers for the corresponding metric over a recent 28-day period.
// Values above the maximum defined for a histogram end up in the max-bucket.
// If the minimum for a UMA histogram is set to be < 1, it is implicitly
// normalized to 1.
// See $/src/base/metrics/histogram.h for more details.

// Video-specific metrics are stored in a custom times histogram.
const int kVideoActionsHistogramsMinMs = 1;
const int kVideoActionsHistogramsMaxMs = 250;
const int kVideoActionsHistogramsBuckets = 50;

// Round-trip latency values are stored in a custom times histogram.
const int kLatencyHistogramMinMs = 1;
const int kLatencyHistogramMaxMs = 20000;
const int kLatencyHistogramBuckets = 50;

// Bandwidth statistics are stored in a custom counts histogram.
const int kBandwidthHistogramMinBps = 0;
const int kBandwidthHistogramMaxBps = 10 * 1000 * 1000;
const int kBandwidthHistogramBuckets = 100;

// Frame rate is stored in a custom enum histogram, because we we want to record
// the frequency of each discrete value, rather than using log-scaled buckets.
// We don't expect video frame rate to be greater than 40fps. Setting a maximum
// of 100fps will leave some room for future improvements, and account for any
// bursts of packets. Enum histograms expect samples to be less than the
// boundary value, so set to 101.
const int kMaxFramesPerSec = 101;

void UpdateUmaEnumHistogramStub(const std::string& histogram_name,
                                int64_t value,
                                int histogram_max) {}

void UpdateUmaCustomHistogramStub(const std::string& histogram_name,
                                  int64_t value,
                                  int histogram_min,
                                  int histogram_max,
                                  int histogram_buckets) {}
}  // namespace

namespace remoting {
namespace protocol {

PerformanceTracker::PerformanceTracker()
    : video_bandwidth_(base::Seconds(kStatsUpdatePeriodSeconds)),
      video_frame_rate_(base::Seconds(kStatsUpdatePeriodSeconds)),
      video_packet_rate_(base::Seconds(kStatsUpdatePeriodSeconds)),
      video_capture_ms_(kLatencySampleSize),
      video_encode_ms_(kLatencySampleSize),
      video_decode_ms_(kLatencySampleSize),
      video_paint_ms_(kLatencySampleSize),
      round_trip_ms_(kLatencySampleSize) {
  uma_custom_counts_updater_ =
      base::BindRepeating(&UpdateUmaCustomHistogramStub);
  uma_custom_times_updater_ =
      base::BindRepeating(&UpdateUmaCustomHistogramStub);
  uma_enum_histogram_updater_ =
      base::BindRepeating(&UpdateUmaEnumHistogramStub);
}

PerformanceTracker::~PerformanceTracker() = default;

void PerformanceTracker::SetUpdateUmaCallbacks(
    UpdateUmaCustomHistogramCallback update_uma_custom_counts_callback,
    UpdateUmaCustomHistogramCallback update_uma_custom_times_callback,
    UpdateUmaEnumHistogramCallback update_uma_enum_histogram_callback) {
  DCHECK(!update_uma_custom_counts_callback.is_null());
  DCHECK(!update_uma_custom_times_callback.is_null());
  DCHECK(!update_uma_enum_histogram_callback.is_null());

  uma_custom_counts_updater_ = update_uma_custom_counts_callback;
  uma_custom_times_updater_ = update_uma_custom_times_callback;
  uma_enum_histogram_updater_ = update_uma_enum_histogram_callback;
}

void PerformanceTracker::OnVideoFrameStats(const FrameStats& stats) {
  if (!is_paused_ && !upload_uma_stats_timer_.IsRunning()) {
    upload_uma_stats_timer_.Start(
        FROM_HERE, base::Seconds(kStatsUpdatePeriodSeconds),
        base::BindRepeating(&PerformanceTracker::UploadRateStatsToUma,
                            base::Unretained(this)));
  }

  // Record this received packet, even if it is empty.
  video_packet_rate_.Record(1);

  // Use only non-empty frames to estimate frame rate.
  if (stats.host_stats.frame_size)
    video_frame_rate_.Record(1);

  video_bandwidth_.Record(stats.host_stats.frame_size);

  if (stats.host_stats.capture_delay != base::TimeDelta::Max()) {
    video_capture_ms_.Record(stats.host_stats.capture_delay.InMilliseconds());
    uma_custom_times_updater_.Run(
        kVideoCaptureLatencyHistogram,
        stats.host_stats.capture_delay.InMilliseconds(),
        kVideoActionsHistogramsMinMs, kVideoActionsHistogramsMaxMs,
        kVideoActionsHistogramsBuckets);
  }

  if (stats.host_stats.encode_delay != base::TimeDelta::Max()) {
    video_encode_ms_.Record(stats.host_stats.encode_delay.InMilliseconds());
    uma_custom_times_updater_.Run(
        kVideoEncodeLatencyHistogram,
        stats.host_stats.encode_delay.InMilliseconds(),
        kVideoActionsHistogramsMinMs, kVideoActionsHistogramsMaxMs,
        kVideoActionsHistogramsBuckets);
  }

  if (stats.host_stats.capture_pending_delay != base::TimeDelta::Max()) {
    uma_custom_times_updater_.Run(
        kCapturePendingLatencyHistogram,
        stats.host_stats.capture_pending_delay.InMilliseconds(),
        kVideoActionsHistogramsMinMs, kVideoActionsHistogramsMaxMs,
        kVideoActionsHistogramsBuckets);
  }

  if (stats.host_stats.capture_overhead_delay != base::TimeDelta::Max()) {
    uma_custom_times_updater_.Run(
        kCaptureOverheadHistogram,
        stats.host_stats.capture_overhead_delay.InMilliseconds(),
        kVideoActionsHistogramsMinMs, kVideoActionsHistogramsMaxMs,
        kVideoActionsHistogramsBuckets);
  }

  if (stats.host_stats.encode_pending_delay != base::TimeDelta::Max()) {
    uma_custom_times_updater_.Run(
        kEncodePendingLatencyHistogram,
        stats.host_stats.encode_pending_delay.InMilliseconds(),
        kVideoActionsHistogramsMinMs, kVideoActionsHistogramsMaxMs,
        kVideoActionsHistogramsBuckets);
  }

  if (stats.host_stats.send_pending_delay != base::TimeDelta::Max()) {
    uma_custom_times_updater_.Run(
        kSendPendingLatencyHistogram,
        stats.host_stats.send_pending_delay.InMilliseconds(),
        kVideoActionsHistogramsMinMs, kVideoActionsHistogramsMaxMs,
        kVideoActionsHistogramsBuckets);
  }

  DCHECK(!stats.client_stats.time_received.is_null());

  // Report decode and render delay only for non-empty frames.
  if (stats.host_stats.frame_size > 0) {
    DCHECK(!stats.client_stats.time_rendered.is_null());
    DCHECK(!stats.client_stats.time_decoded.is_null());
    base::TimeDelta decode_delay =
        stats.client_stats.time_decoded - stats.client_stats.time_received;
    video_decode_ms_.Record(decode_delay.InMilliseconds());
    uma_custom_times_updater_.Run(
        kVideoDecodeLatencyHistogram, decode_delay.InMilliseconds(),
        kVideoActionsHistogramsMinMs, kVideoActionsHistogramsMaxMs,
        kVideoActionsHistogramsBuckets);

    base::TimeDelta render_delay =
        stats.client_stats.time_rendered - stats.client_stats.time_decoded;
    video_paint_ms_.Record(render_delay.InMilliseconds());
    uma_custom_times_updater_.Run(
        kVideoPaintLatencyHistogram, render_delay.InMilliseconds(),
        kVideoActionsHistogramsMinMs, kVideoActionsHistogramsMaxMs,
        kVideoActionsHistogramsBuckets);
  }

  // |latest_event_timestamp| is set only for the first frame after an input
  // event.
  if (stats.host_stats.latest_event_timestamp.is_null())
    return;

  // For empty frames use time_received as time_rendered.
  base::TimeTicks time_rendered = (stats.host_stats.frame_size > 0)
                                      ? stats.client_stats.time_rendered
                                      : stats.client_stats.time_received;
  base::TimeDelta round_trip_latency =
      time_rendered - stats.host_stats.latest_event_timestamp;
  round_trip_ms_.Record(round_trip_latency.InMilliseconds());
  uma_custom_times_updater_.Run(
      kRoundTripLatencyHistogram, round_trip_latency.InMilliseconds(),
      kLatencyHistogramMinMs, kLatencyHistogramMaxMs, kLatencyHistogramBuckets);

  // Report estimated network latency.
  if (stats.host_stats.capture_delay != base::TimeDelta::Max() &&
      stats.host_stats.encode_delay != base::TimeDelta::Max() &&
      stats.host_stats.capture_pending_delay != base::TimeDelta::Max() &&
      stats.host_stats.capture_overhead_delay != base::TimeDelta::Max() &&
      stats.host_stats.encode_pending_delay != base::TimeDelta::Max() &&
      stats.host_stats.send_pending_delay != base::TimeDelta::Max()) {
    // Calculate total processing time on host and client.
    base::TimeDelta total_processing_latency =
        stats.host_stats.capture_delay + stats.host_stats.encode_delay +
        stats.host_stats.capture_pending_delay +
        stats.host_stats.capture_overhead_delay +
        stats.host_stats.encode_pending_delay +
        stats.host_stats.send_pending_delay +
        (time_rendered - stats.client_stats.time_received);
    base::TimeDelta network_latency =
        round_trip_latency - total_processing_latency;
    uma_custom_times_updater_.Run(
        kNetworkLatencyHistogram, network_latency.InMilliseconds(),
        kVideoActionsHistogramsMinMs, kVideoActionsHistogramsMaxMs,
        kVideoActionsHistogramsBuckets);
  }
}

void PerformanceTracker::UploadRateStatsToUma() {
  uma_enum_histogram_updater_.Run(kVideoFrameRateHistogram, video_frame_rate(),
                                  kMaxFramesPerSec);
  uma_enum_histogram_updater_.Run(kVideoPacketRateHistogram,
                                  video_packet_rate(), kMaxFramesPerSec);
  uma_custom_counts_updater_.Run(
      kVideoBandwidthHistogram, video_bandwidth(), kBandwidthHistogramMinBps,
      kBandwidthHistogramMaxBps, kBandwidthHistogramBuckets);
}

void PerformanceTracker::OnPauseStateChanged(bool paused) {
  is_paused_ = paused;
  if (is_paused_) {
    // Pause the UMA timer when the video is paused. It will be unpaused in
    // RecordVideoFrameStats() when a new frame is received.
    upload_uma_stats_timer_.Stop();
  }
}

}  // namespace protocol
}  // namespace remoting
