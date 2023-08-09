// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_OUTPUT_GLITCH_COUNTER_H_
#define SERVICES_AUDIO_OUTPUT_GLITCH_COUNTER_H_

#include <vector>
#include "base/functional/callback.h"
#include "media/base/audio_latency.h"

namespace audio {

// Class for logging audio glitch data at 10 second intervals, for use by
// SyncReader.
class OutputGlitchCounter {
 public:
  explicit OutputGlitchCounter(media::AudioLatency::Type latency_tag);
  virtual ~OutputGlitchCounter();

  OutputGlitchCounter(const OutputGlitchCounter&) = delete;
  OutputGlitchCounter& operator=(const OutputGlitchCounter&) = delete;

  // Reports an attempt to get audio output from the renderer, and whether the
  // renderer missed the callback or not.
  // Virtual for testing.
  virtual void ReportMissedCallback(bool missed_callback, bool is_mixing);

  // Stats used by SyncReader for logging.
  struct LogStats {
    const size_t callback_count_;
    const size_t miss_count_;
  };
  LogStats GetLogStats();

 private:
  // Keeps track of the callback statistics. We have one instance for all
  // callbacks and one for only the mixing callbacks.
  class Counter final {
   public:
    Counter(media::AudioLatency::Type latency_tag, bool mixing);
    ~Counter();

    Counter(const Counter&) = delete;
    Counter& operator=(const Counter&) = delete;

    // Update the counters for a callback that should be counted by this
    // counter. Does not upload any histograms.
    void Report(bool missed_callback);

    // Uploads any collected data, then resets the counter.
    void Reset();

    bool HadGlitches() const;

   private:
    friend class OutputGlitchCounter;

    // Precomputed histogram names.
    const std::string histogram_name_intervals_;
    const std::string histogram_name_intervals_with_latency_;
    const std::string histogram_name_short_;
    const std::string histogram_name_short_with_latency_;

    // The total number of callbacks received by this counter.
    size_t callback_count_ = 0;

    // The number of missed callbacks in this 1000 callback interval.
    size_t current_miss_count_ = 0;

    // The total number of missed callbacks.
    size_t total_miss_count_ = 0;

    // The total number of missed callbacks since the last successful callback.
    size_t total_trailing_miss_count_ = 0;

    // Samples of the number of missed callbacks during 1000 callback intervals,
    // that we can upload as soon as we are sure that they are not trailing.
    std::vector<size_t> complete_samples_;
  };

  media::AudioLatency::Type latency_tag_;

  Counter overall_counter_;
  Counter mixing_counter_;

  // The number of calls to ReportMissedCallback(), which corresponds to
  // SyncReader::Read(), that constitute a sample period. For 10ms buffer sizes
  // this corresponds to 10 seconds.
  static constexpr int kSampleInterval = 1000;
};

}  // namespace audio

#endif  // SERVICES_AUDIO_OUTPUT_GLITCH_COUNTER_H_
