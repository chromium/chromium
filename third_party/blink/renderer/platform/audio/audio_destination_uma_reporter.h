// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_AUDIO_DESTINATION_UMA_REPORTER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_AUDIO_DESTINATION_UMA_REPORTER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "third_party/blink/public/platform/web_audio_latency_hint.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

// Calculate and report AudioDestination-related metric. Some metrics are
// updated at every audio callback, while others are accumulated over miultiple
// callbacks.
class PLATFORM_EXPORT AudioDestinationUmaReporter final {
 public:
  explicit AudioDestinationUmaReporter(const WebAudioLatencyHint&);
  virtual ~AudioDestinationUmaReporter();

  // These methods are not thread-safe and must be called within
  // `AudioDestination::RequestRender()` method for correct instrumentation.
  void UpdateFifoDelay(base::TimeDelta fifo_delay);
  void UpdateTotalPlayoutDelay(base::TimeDelta total_playout_delay);
  void IncreaseFifoUnderrunCount();
  void Report();

 private:
  // Indicates what period samples are aggregated over. kShort means entire
  // streams of less than 1000 callbacks, kIntervals means exactly 1000
  // callbacks.
  enum class SamplingPeriod { kShort, kIntervals };

  using RealtimeUmaCallback = base::RepeatingCallback<void(int value)>;
  using AggregateUmaCallback =
      base::RepeatingCallback<void(int value, SamplingPeriod sampling_period)>;

  static RealtimeUmaCallback CreateRealtimeUmaCallback(
      const std::string& stat_name,
      WebAudioLatencyHint latency_hint,
      int max_value,
      size_t bucket_count);

  static AggregateUmaCallback CreateAggregateUmaCallback(
      const std::string& stat_name,
      WebAudioLatencyHint latency_hint,
      int max_value,
      size_t bucket_count);

  int callback_count_ = 0;
  int fifo_underrun_count_ = 0;

  // The audio delay (ms) computed the number of available frames of the
  // PushPUllFIFO in AudioDestination. Measured and reported at every audio
  // callback.
  base::TimeDelta fifo_delay_;

  // The audio delay (ms) covers the whole pipeline from the WebAudio graph to
  // the speaker. Measured and reported at every audio callback.
  base::TimeDelta total_playout_delay_;

  const RealtimeUmaCallback fifo_delay_uma_callback_;
  const RealtimeUmaCallback total_playout_delay_uma_callback_;
  const AggregateUmaCallback fifo_underrun_count_uma_callback_;

  // Indicates that the current audio stream is less than 1000 callbacks.
  bool is_stream_short_ = true;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_AUDIO_DESTINATION_UMA_REPORTER_H_

