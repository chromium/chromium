// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PERFORMANCE_TRACKER_H_
#define REMOTING_PROTOCOL_PERFORMANCE_TRACKER_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "remoting/base/rate_counter.h"
#include "remoting/base/running_samples.h"
#include "remoting/protocol/frame_stats.h"

namespace remoting {
namespace protocol {

// PerformanceTracker defines a bundle of performance counters and statistics
// for chromoting.
class PerformanceTracker : public FrameStatsConsumer {
 public:
  // Callback that updates UMA custom counts or custom times histograms.
  typedef base::RepeatingCallback<void(const std::string& histogram_name,
                                       int64_t value,
                                       int histogram_min,
                                       int histogram_max,
                                       int histogram_buckets)>
      UpdateUmaCustomHistogramCallback;

  // Callback that updates UMA enumeration histograms.
  typedef base::RepeatingCallback<
      void(const std::string& histogram_name, int64_t value, int histogram_max)>
      UpdateUmaEnumHistogramCallback;

  PerformanceTracker();

  PerformanceTracker(const PerformanceTracker&) = delete;
  PerformanceTracker& operator=(const PerformanceTracker&) = delete;

  ~PerformanceTracker() override;

  // Constant used to calculate the average for rate metrics and used by the
  // plugin for the frequency at which stats should be updated.
  static const int kStatsUpdatePeriodSeconds = 1;

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

  // Sets callbacks in ChromotingInstance to update a UMA custom counts, custom
  // times or enum histogram.
  void SetUpdateUmaCallbacks(
      UpdateUmaCustomHistogramCallback update_uma_custom_counts_callback,
      UpdateUmaCustomHistogramCallback update_uma_custom_times_callback,
      UpdateUmaEnumHistogramCallback update_uma_enum_histogram_callback);

  void OnPauseStateChanged(bool paused);

 private:
  // Updates frame-rate, packet-rate and bandwidth UMA statistics.
  void UploadRateStatsToUma();

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

  // Used to update UMA stats, if set.
  UpdateUmaCustomHistogramCallback uma_custom_counts_updater_;
  UpdateUmaCustomHistogramCallback uma_custom_times_updater_;
  UpdateUmaEnumHistogramCallback uma_enum_histogram_updater_;

  bool is_paused_ = false;

  base::RepeatingTimer upload_uma_stats_timer_;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_PERFORMANCE_TRACKER_H_
