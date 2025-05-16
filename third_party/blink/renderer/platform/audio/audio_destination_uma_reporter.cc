// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/audio/audio_destination_uma_reporter.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"

namespace blink {

namespace {

const std::string GetExpectedHistogramName(
    const std::string_view& name_base,
    bool use_audio_worklet,
    std::optional<std::string> duration) {
  std::string_view thread_mode =
      use_audio_worklet ? "DualThread." : "SingleThread.";
  std::string sampling_period =
      duration.has_value() ? base::StrCat({".", *duration}) : "";
  return base::StrCat(
      {"WebAudio.AudioDestination.", thread_mode, name_base, sampling_period});
}

}  // namespace

AudioDestinationUmaReporter::AudioDestinationUmaReporter(
    const WebAudioLatencyHint& latency_hint,
    int callback_buffer_size,
    float sample_rate)
    : latency_hint_(latency_hint) {
  expected_callback_interval_ =
      base::Seconds(static_cast<float>(callback_buffer_size) / sample_rate);

  fifo_delay_histogram_name_ =
      GetExpectedHistogramName(kFifoDelayHistogramNameBase, use_audio_worklet_,
                               /*duration=*/std::nullopt);
  fifo_delay_histogram_name_with_latency_tag_ =
      base::StrCat({fifo_delay_histogram_name_, ".",
                    WebAudioLatencyHint::AsString(latency_hint_)});

  fifo_underrun_histogram_name_ = GetExpectedHistogramName(
      kFifoUnderrunHistogramNameBase, use_audio_worklet_,
      /*duration=*/"Intervals");
  fifo_underrun_histogram_name_with_latency_tag_ =
      base::StrCat({fifo_underrun_histogram_name_, ".",
                    WebAudioLatencyHint::AsString(latency_hint_)});

  total_playout_delay_histogram_name_ = GetExpectedHistogramName(
      kTotalPlayoutDelayHistogramNameBase, use_audio_worklet_,
      /*duration=*/std::nullopt);
  total_playout_delay_histogram_name_with_latency_tag_ =
      base::StrCat({total_playout_delay_histogram_name_, ".",
                    WebAudioLatencyHint::AsString(latency_hint_)});

  render_time_ratio_histogram_name_ = GetExpectedHistogramName(
      kRenderTimeRatioHistogramNameBase, use_audio_worklet_,
      /*duration=*/std::nullopt);
  render_time_ratio_histogram_name_with_latency_tag_ =
      base::StrCat({render_time_ratio_histogram_name_, ".",
                    WebAudioLatencyHint::AsString(latency_hint_)});
  request_render_time_ratio_histogram_name_ = GetExpectedHistogramName(
      kRequestRenderTimeRatioHistogramNameBase, use_audio_worklet_,
      /*duration=*/std::nullopt);
  request_render_time_ratio_histogram_name_with_latency_tag_ =
      base::StrCat({request_render_time_ratio_histogram_name_, ".",
                    WebAudioLatencyHint::AsString(latency_hint_)});
  request_render_gap_time_ratio_histogram_name_ = GetExpectedHistogramName(
      kRequestRenderGapTimeRatioHistogramNameBase, use_audio_worklet_,
      /*duration=*/std::nullopt);
  request_render_gap_time_ratio_histogram_name_with_latency_tag_ =
      base::StrCat({request_render_gap_time_ratio_histogram_name_, ".",
                    WebAudioLatencyHint::AsString(latency_hint_)});
}

AudioDestinationUmaReporter::~AudioDestinationUmaReporter() {
  if (is_stream_short_ && callback_count_ > 0) {
    fifo_underrun_histogram_name_ = GetExpectedHistogramName(
        kFifoUnderrunHistogramNameBase, use_audio_worklet_,
        /*duration=*/"Short");
    fifo_underrun_histogram_name_with_latency_tag_ =
        base::StrCat({fifo_underrun_histogram_name_, ".",
                      WebAudioLatencyHint::AsString(latency_hint_)});

    base::UmaHistogramCustomCounts(fifo_underrun_histogram_name_,
                                   fifo_underrun_count_, 1, 1000, 50);
    base::UmaHistogramCustomCounts(
        fifo_underrun_histogram_name_with_latency_tag_, fifo_underrun_count_, 1,
        1000, 50);
  }
}

void AudioDestinationUmaReporter::AddFifoDelay(base::TimeDelta fifo_delay) {
  fifo_delay_sum_ += fifo_delay;
}

void AudioDestinationUmaReporter::AddTotalPlayoutDelay(
    base::TimeDelta total_playout_delay) {
  total_playout_delay_sum_ += total_playout_delay;
}

void AudioDestinationUmaReporter::IncreaseFifoUnderrunCount() {
  fifo_underrun_count_++;
}

void AudioDestinationUmaReporter::Report() {
  if (++callback_count_ >= kMetricsReportCycle) {
    base::UmaHistogramCustomCounts(
        fifo_delay_histogram_name_,
        fifo_delay_sum_.InMilliseconds() / kMetricsReportCycle, 1, 1000, 50);
    base::UmaHistogramCustomCounts(
        fifo_delay_histogram_name_with_latency_tag_,
        fifo_delay_sum_.InMilliseconds() / kMetricsReportCycle, 1, 1000, 50);

    base::UmaHistogramCustomCounts(
        total_playout_delay_histogram_name_,
        total_playout_delay_sum_.InMilliseconds() / kMetricsReportCycle, 1,
        1000, 50);
    base::UmaHistogramCustomCounts(
        total_playout_delay_histogram_name_with_latency_tag_,
        total_playout_delay_sum_.InMilliseconds() / kMetricsReportCycle, 1,
        1000, 50);

    fifo_delay_sum_ = base::TimeDelta();
    total_playout_delay_sum_ = base::TimeDelta();

    base::UmaHistogramCustomCounts(fifo_underrun_histogram_name_,
                                   fifo_underrun_count_, 1, 1000, 50);
    base::UmaHistogramCustomCounts(
        fifo_underrun_histogram_name_with_latency_tag_, fifo_underrun_count_, 1,
        1000, 50);

    int render_time_percentage =
        PercentOfCallbackInterval(render_total_duration_);
    int request_render_time_percentage =
        PercentOfCallbackInterval(request_render_total_duration_);
    int request_render_gap_time_percentage =
        PercentOfCallbackInterval(request_render_gap_total_duration_);

    base::UmaHistogramExactLinear(render_time_ratio_histogram_name_,
                                  render_time_percentage, 101);
    base::UmaHistogramExactLinear(
        render_time_ratio_histogram_name_with_latency_tag_,
        render_time_percentage, 101);
    base::UmaHistogramExactLinear(request_render_time_ratio_histogram_name_,
                                  request_render_time_percentage, 101);
    base::UmaHistogramExactLinear(
        request_render_time_ratio_histogram_name_with_latency_tag_,
        request_render_time_percentage, 101);
    base::UmaHistogramExactLinear(request_render_gap_time_ratio_histogram_name_,
                                  request_render_gap_time_percentage, 101);
    base::UmaHistogramExactLinear(
        request_render_gap_time_ratio_histogram_name_with_latency_tag_,
        request_render_gap_time_percentage, 101);

    callback_count_ = 0;
    fifo_underrun_count_ = 0;
    render_total_duration_ = base::TimeDelta();
    request_render_total_duration_ = base::TimeDelta();
    request_render_gap_total_duration_ = base::TimeDelta();
    is_stream_short_ = false;
  }
}

void AudioDestinationUmaReporter::UpdateMetricNameForDualThreadMode() {
  use_audio_worklet_ = true;
  fifo_delay_histogram_name_ =
      GetExpectedHistogramName(kFifoDelayHistogramNameBase, use_audio_worklet_,
                               /*duration=*/std::nullopt);
  fifo_delay_histogram_name_with_latency_tag_ =
      base::StrCat({fifo_delay_histogram_name_, ".",
                    WebAudioLatencyHint::AsString(latency_hint_)});

  fifo_underrun_histogram_name_ = GetExpectedHistogramName(
      kFifoUnderrunHistogramNameBase, use_audio_worklet_,
      /*duration=*/"Intervals");
  fifo_underrun_histogram_name_with_latency_tag_ =
      base::StrCat({fifo_underrun_histogram_name_, ".",
                    WebAudioLatencyHint::AsString(latency_hint_)});

  total_playout_delay_histogram_name_ = GetExpectedHistogramName(
      kTotalPlayoutDelayHistogramNameBase, use_audio_worklet_,
      /*duration=*/std::nullopt);
  total_playout_delay_histogram_name_with_latency_tag_ =
      base::StrCat({total_playout_delay_histogram_name_, ".",
                    WebAudioLatencyHint::AsString(latency_hint_)});

  render_time_ratio_histogram_name_ = GetExpectedHistogramName(
      kRenderTimeRatioHistogramNameBase, use_audio_worklet_,
      /*duration=*/std::nullopt);
  render_time_ratio_histogram_name_with_latency_tag_ =
      base::StrCat({render_time_ratio_histogram_name_, ".",
                    WebAudioLatencyHint::AsString(latency_hint_)});
  request_render_time_ratio_histogram_name_ = GetExpectedHistogramName(
      kRequestRenderTimeRatioHistogramNameBase, use_audio_worklet_,
      /*duration=*/std::nullopt);
  request_render_time_ratio_histogram_name_with_latency_tag_ =
      base::StrCat({request_render_time_ratio_histogram_name_, ".",
                    WebAudioLatencyHint::AsString(latency_hint_)});
  request_render_gap_time_ratio_histogram_name_ = GetExpectedHistogramName(
      kRequestRenderGapTimeRatioHistogramNameBase, use_audio_worklet_,
      /*duration=*/std::nullopt);
  request_render_gap_time_ratio_histogram_name_with_latency_tag_ =
      base::StrCat({request_render_gap_time_ratio_histogram_name_, ".",
                    WebAudioLatencyHint::AsString(latency_hint_)});
}

int AudioDestinationUmaReporter::PercentOfCallbackInterval(
    base::TimeDelta duration) {
  return static_cast<int>(100 * (duration / expected_callback_interval_) /
                          kMetricsReportCycle);
}

}  // namespace blink
