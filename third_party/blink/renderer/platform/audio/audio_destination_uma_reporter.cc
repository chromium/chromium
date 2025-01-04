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
        /*bucket_count = */ 50)),
      fifo_underrun_count_uma_callback_(CreateAggregateUmaCallback(
        "FIFOUnderrunCount",
        latency_hint,
        /*max_value = */ 1000,
        /*bucket_count = */ 50)) {}

AudioDestinationUmaReporter::~AudioDestinationUmaReporter() {
  if (is_stream_short_ && callback_count_ > 0) {
    fifo_underrun_count_uma_callback_.Run(fifo_underrun_count_,
                                          SamplingPeriod::kShort);
  }
}

void AudioDestinationUmaReporter::UpdateFifoDelay(base::TimeDelta fifo_delay) {
  fifo_delay_ = fifo_delay;
}

void AudioDestinationUmaReporter::UpdateTotalPlayoutDelay(
    base::TimeDelta total_playout_delay) {
  total_playout_delay_ = total_playout_delay;
}

void AudioDestinationUmaReporter::IncreaseFifoUnderrunCount() {
  fifo_underrun_count_++;
}

void AudioDestinationUmaReporter::Report() {
  fifo_delay_uma_callback_.Run(fifo_delay_.InMilliseconds());
  total_playout_delay_uma_callback_.Run(total_playout_delay_.InMilliseconds());

  if (++callback_count_ >= 1000) {
    fifo_underrun_count_uma_callback_.Run(fifo_underrun_count_,
                                          SamplingPeriod::kIntervals);
    callback_count_ = 0;
    fifo_underrun_count_ = 0;
    is_stream_short_ = false;
  }
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

AudioDestinationUmaReporter::AggregateUmaCallback
AudioDestinationUmaReporter::CreateAggregateUmaCallback(
    const std::string& stat_name,
    WebAudioLatencyHint latency_hint,
    int max_value,
    size_t bucket_count) {
  std::string base_name(
      base::StrCat({"WebAudio.AudioDestination.", stat_name}));
  std::string short_name(base::StrCat({base_name, ".Short"}));
  std::string intervals_name(base::StrCat({base_name, ".Intervals"}));
  std::string short_with_latency_name(
      base::StrCat({short_name, ".", LatencyToString(latency_hint)}));
  std::string intervals_with_latency_name(
      base::StrCat({intervals_name, ".", LatencyToString(latency_hint)}));

  return base::BindRepeating(
      [](int max_value, size_t bucket_count, const std::string& short_name,
         const std::string& intervals_name,
         const std::string& short_with_latency_name,
         const std::string& intervals_with_latency_name, int value,
         SamplingPeriod sampling_period) {
        if (sampling_period == SamplingPeriod::kShort) {
          base::UmaHistogramCustomCounts(short_name, value, 1, max_value,
                                         bucket_count);
          base::UmaHistogramCustomCounts(short_with_latency_name, value, 1,
                                         max_value, bucket_count);
        } else {
          base::UmaHistogramCustomCounts(intervals_name, value, 1, max_value,
                                         bucket_count);
          base::UmaHistogramCustomCounts(intervals_with_latency_name, value, 1,
                                         max_value, bucket_count);
        }
      },
      max_value, bucket_count, std::move(short_name), std::move(intervals_name),
      std::move(short_with_latency_name),
      std::move(intervals_with_latency_name));
}

}  // namespace blink
