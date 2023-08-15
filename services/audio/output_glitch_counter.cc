// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/output_glitch_counter.h"

#include <string>
#include "base/check_op.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/trace_event/trace_event.h"

using media::AudioLatency;

namespace audio {

namespace {

// Used to log if any audio glitches have been detected during an audio session.
// Elements in this enum should not be deleted or rearranged, because they need
// to stay consistent with the existing histogram data.
enum class AudioGlitchResult {
  kNoGlitches = 0,
  kGlitches = 1,
  kMaxValue = kGlitches
};
}  // namespace

OutputGlitchCounter::OutputGlitchCounter(media::AudioLatency::Type latency_tag)
    : latency_tag_(latency_tag),
      overall_counter_(latency_tag, false),
      mixing_counter_(latency_tag, true) {}

OutputGlitchCounter::~OutputGlitchCounter() {
  // Log whether or not there was any dropped data at all.
  std::string histogram_name = "Media.AudioRendererAudioGlitches2";
  AudioGlitchResult audio_glitch_result = overall_counter_.HadGlitches()
                                              ? AudioGlitchResult::kGlitches
                                              : AudioGlitchResult::kNoGlitches;
  base::UmaHistogramEnumeration(histogram_name, audio_glitch_result);
  base::UmaHistogramEnumeration(
      histogram_name + "." + AudioLatency::ToString(latency_tag_),
      audio_glitch_result);
}

void OutputGlitchCounter::ReportMissedCallback(bool missed_callback,
                                               bool is_mixing) {
  overall_counter_.Report(missed_callback);

  if (is_mixing) {
    mixing_counter_.Report(missed_callback);
  } else {
    mixing_counter_.Reset();
  }
}

OutputGlitchCounter::LogStats OutputGlitchCounter::GetLogStats() {
  return LogStats{
      .callback_count_ = overall_counter_.callback_count_ -
                         overall_counter_.total_trailing_miss_count_,
      .miss_count_ = overall_counter_.total_miss_count_ -
                     overall_counter_.total_trailing_miss_count_};
}

OutputGlitchCounter::Counter::Counter(media::AudioLatency::Type latency_tag,
                                      bool mixing)
    : histogram_name_intervals_(
          base::StrCat({"Media.AudioRendererMissedDeadline3",
                        mixing ? ".Mixing" : "", ".Intervals"})),
      histogram_name_intervals_with_latency_(
          histogram_name_intervals_ + "." +
          AudioLatency::ToString(latency_tag)),
      histogram_name_short_(base::StrCat({"Media.AudioRendererMissedDeadline3",
                                          mixing ? ".Mixing" : "", ".Short"})),
      histogram_name_short_with_latency_(histogram_name_short_ + "." +
                                         AudioLatency::ToString(latency_tag)) {
  // Assume a bad case of 192 kHz with 256 sample buffers. One sample represents
  // 1000 buffers, and we want to reserve 60 seconds worth of samples.
  // 60 / (1000 * 256 / 192000) = 45.
  complete_samples_.reserve(45);
}

OutputGlitchCounter::Counter::~Counter() {
  Reset();
}

void OutputGlitchCounter::Counter::Report(bool missed_callback) {
  ++callback_count_;
  if (missed_callback) {
    ++current_miss_count_;
    ++total_miss_count_;
    ++total_trailing_miss_count_;
  }

  // If it's the end of the sample window, then it's time queue the
  // current sample to be uploaded and start adding data to a new sample.
  if (callback_count_ % kSampleInterval == 0) {
    complete_samples_.push_back(current_miss_count_);
    current_miss_count_ = 0;
  }

  if (!missed_callback) {
    // If we did not miss the callback, we know that there are no trailing
    // errors and that we can upload the complete samples (since they do not
    // contain trailing errors either).
    total_trailing_miss_count_ = 0;
    for (size_t miss_count : complete_samples_) {
      base::UmaHistogramCounts1000(histogram_name_intervals_, miss_count);
      base::UmaHistogramCounts1000(histogram_name_intervals_with_latency_,
                                   miss_count);
    }
    complete_samples_.clear();
  }
}

void OutputGlitchCounter::Counter::Reset() {
  // If callback_count_ is 0 then Report() has never been called since the last
  // Reset()/construction, so we should do nothing.
  if (callback_count_ == 0) {
    return;
  }

  // Subtract 'trailing' error counts that will happen if the renderer process
  // was killed or e.g. the page refreshed while the output device was open etc.
  // The counts should then only include data from before the teardown period.
  DCHECK_GE(callback_count_, total_trailing_miss_count_);
  DCHECK_GE(total_miss_count_, total_trailing_miss_count_);
  callback_count_ -= total_trailing_miss_count_;
  total_miss_count_ -= total_trailing_miss_count_;
  DCHECK_GE(callback_count_, total_miss_count_);

  if (0 < callback_count_ && callback_count_ < kSampleInterval) {
    // The stream without trailing glitches is shorter than kSampleInterval
    // callbacks, which means UploadCompleteSamples() has never uploaded
    // anything. Adding stats to dedicated histograms.
    base::UmaHistogramCounts1000(histogram_name_short_, total_miss_count_);
    base::UmaHistogramCounts1000(histogram_name_short_with_latency_,
                                 total_miss_count_);
  }

  callback_count_ = 0;
  current_miss_count_ = 0;
  total_miss_count_ = 0;
  total_trailing_miss_count_ = 0;
  complete_samples_.clear();
}

bool OutputGlitchCounter::Counter::HadGlitches() const {
  return total_miss_count_ - total_trailing_miss_count_ > 0;
}

}  // namespace audio
