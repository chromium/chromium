// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/audio/audio_destination_uma_reporter.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace blink {

namespace {

const char* LatencyToString(WebAudioLatencyHint& latency_hint) {
  switch (latency_hint.Category()) {
    case WebAudioLatencyHint::kCategoryInteractive:
      return "LatencyInteractive";
    case WebAudioLatencyHint::kCategoryBalanced:
      return "LatencyBalanced";
    case WebAudioLatencyHint::kCategoryPlayback:
      return "LatencyPlayback";
    case WebAudioLatencyHint::kCategoryExact:
      return "LatencyExactMs";
    default:
      return "LatencyUnknown";
  }
}

}  // namespace

AudioDestinationUmaReporter::AudioDestinationUmaReporter(
    const WebAudioLatencyHint& latency_hint)
    : fifo_delay_uma_callback_(CreateRealtimeUmaCallback(
        "FIFODelay",
        latency_hint,
        /*max_value = */ 1000,
        /*bucket_count = */ 50)),
      total_playout_delay_uma_callback_(CreateRealtimeUmaCallback(
        "TotalPlayoutDelay",
        latency_hint,
        /*max_value = */ 1000,
        /*bucket_count = */ 50)) {}

void AudioDestinationUmaReporter::UpdateFifoDelay(base::TimeDelta fifo_delay) {
  fifo_delay_ = fifo_delay;
}

void AudioDestinationUmaReporter::UpdateTotalPlayoutDelay(
    base::TimeDelta total_playout_delay) {
  total_playout_delay_ = total_playout_delay;
}

void AudioDestinationUmaReporter::Report() {
  fifo_delay_uma_callback_.Run(fifo_delay_.InMilliseconds());
  total_playout_delay_uma_callback_.Run(total_playout_delay_.InMilliseconds());
}

AudioDestinationUmaReporter::RealtimeUmaCallback
AudioDestinationUmaReporter::CreateRealtimeUmaCallback(
    const std::string& stat_name,
    WebAudioLatencyHint latency_hint,
    int max_value,
    size_t bucket_count) {
  std::string base_name(
      base::StrCat({"WebAudio.AudioDestination.", stat_name}));
  std::string base_with_latency_name(
      base::StrCat({base_name, ".", LatencyToString(latency_hint)}));

  base::HistogramBase* histogram = base::Histogram::FactoryGet(
      std::move(base_name), 1, max_value, bucket_count,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  base::HistogramBase* histogram_with_latency = base::Histogram::FactoryGet(
      std::move(base_with_latency_name), 1, max_value, bucket_count,
      base::HistogramBase::kUmaTargetedHistogramFlag);

  // Histogram pointers from FactoryGet are not owned by the caller. They are
  // never deleted, see crbug.com/79322
  return base::BindRepeating(
      [](base::HistogramBase* histogram,
         base::HistogramBase* histogram_with_latency, int value) {
        histogram->Add(value);
        histogram_with_latency->Add(value);
      },
      base::Unretained(histogram), base::Unretained(histogram_with_latency));
}

}  // namespace blink
