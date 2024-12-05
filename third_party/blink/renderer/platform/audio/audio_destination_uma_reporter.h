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

// Logs AudioDestination-related metric on every audio callback.
class PLATFORM_EXPORT AudioDestinationUmaReporter final {
 public:
  explicit AudioDestinationUmaReporter(const WebAudioLatencyHint&);

  void UpdateFifoDelay(base::TimeDelta fifo_delay);
  void UpdateTotalPlayoutDelay(base::TimeDelta total_playout_delay);
  void Report();

 private:
  using RealtimeUmaCallback = base::RepeatingCallback<void(int value)>;

  static RealtimeUmaCallback CreateRealtimeUmaCallback(
      const std::string& stat_name,
      WebAudioLatencyHint latency_hint,
      int max_value,
      size_t bucket_count);

  // The audio delay (ms) computed the number of available frames of the
  // PushPUllFIFO in AudioDestination. Measured and reported at every audio
  // callback.
  base::TimeDelta fifo_delay_;

  // The audio delay (ms) covers the whole pipeline from the WebAudio graph to
  // the speaker. Measured and reported at every audio callback.
  base::TimeDelta total_playout_delay_;

  const RealtimeUmaCallback fifo_delay_uma_callback_;
  const RealtimeUmaCallback total_playout_delay_uma_callback_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_AUDIO_DESTINATION_UMA_REPORTER_H_

