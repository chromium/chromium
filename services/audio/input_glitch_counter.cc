// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "services/audio/input_glitch_counter.h"
#include <cstddef>
#include <utility>

#include "base/check_op.h"
#include "base/format_macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"

namespace audio {

namespace {

// Used to log if any audio glitches have been detected during an audio session.
// Elements in this enum should not be added, deleted or rearranged.
enum class AudioGlitchResult {
  kNoGlitches = 0,
  kGlitches = 1,
  kMaxValue = kGlitches
};

}  // namespace

InputGlitchCounter::InputGlitchCounter(
    base::RepeatingCallback<void(const std::string&)> log_callback)
    : log_callback_(std::move(log_callback)) {
  // Reserve one minutes worth of complete samples (assuming 10ms buffers) to
  // hopefully avoid allocating them on the realtime thread.
  complete_samples_.reserve(6);
}

InputGlitchCounter::~InputGlitchCounter() {
  // Subtract 'trailing' error counts that will happen if the renderer process
  // was killed or e.g. the page refreshed while the input device was open etc.
  // The counts should then only include data from before the teardown period.
  DCHECK_GE(write_count_, global_sample_.trailing_missed_read_deadline_count_);
  DCHECK_GE(global_sample_.missed_read_deadline_count_,
            global_sample_.trailing_missed_read_deadline_count_);
  DCHECK_GE(global_sample_.dropped_data_count_,
            global_sample_.trailing_dropped_data_count_);
  write_count_ -= global_sample_.trailing_missed_read_deadline_count_;
  global_sample_.missed_read_deadline_count_ -=
      global_sample_.trailing_missed_read_deadline_count_;
  global_sample_.dropped_data_count_ -=
      global_sample_.trailing_dropped_data_count_;

  // Log whether or not there was any dropped data at all (before the teardown
  // period).
  base::UmaHistogramEnumeration("Media.AudioCapturerAudioGlitches",
                                global_sample_.dropped_data_count_ == 0
                                    ? AudioGlitchResult::kNoGlitches
                                    : AudioGlitchResult::kGlitches);

  std::string log_string = base::StringPrintf(
      "AISW: number of detected audio glitches: %" PRIuS " out of %" PRIuS,
      global_sample_.dropped_data_count_, write_count_);
  log_callback_.Run(log_string);

  if (write_count_ < kSampleInterval) {
    // The stream without trailing glitches is shorter than kSampleInterval
    // callbacks, which means UploadCompleteSamples() has never uploaded
    // anything. Adding stats to dedicated histograms.
    base::UmaHistogramCounts1000("Media.AudioCapturerDroppedDataBelow10s",
                                 global_sample_.dropped_data_count_);
    base::UmaHistogramCounts1000(
        "Media.AudioCapturerMissedReadDeadlineBelow10s",
        global_sample_.missed_read_deadline_count_);
  }

  // All remaining not uploaded |complete_samples_| will be dropped, since the
  // first of them at least partially and the rest fully consist of trailing
  // glitches.
}

void InputGlitchCounter::ReportDroppedData(bool dropped_data) {
  if (dropped_data) {
    ++current_sample_.dropped_data_count_;
    ++global_sample_.dropped_data_count_;
    ++global_sample_.trailing_dropped_data_count_;
  } else {
    // Since we have just successfully written data, we can assume that the
    // receiver is still alive, so there should not be any trailing errors.
    // Thererfore we can upload any complete samples we have stored.
    global_sample_.trailing_dropped_data_count_ = 0;
    global_sample_.trailing_missed_read_deadline_count_ = 0;
    UploadCompleteSamples();
  }
}

void InputGlitchCounter::ReportMissedReadDeadline(bool missed_read_deadline) {
  ++write_count_;
  if (missed_read_deadline) {
    ++current_sample_.missed_read_deadline_count_;
    ++global_sample_.missed_read_deadline_count_;
    ++global_sample_.trailing_missed_read_deadline_count_;
  }

  // If it's the end of the sample window, then it's time queue the current
  // sample to be uploaded and start adding data to a new sample.
  if (write_count_ % kSampleInterval == 0) {
    // The sample will be uploaded once we know that it does not contain
    // trailing errors.
    complete_samples_.push_back(current_sample_);
    current_sample_ = {};
  }
}

void InputGlitchCounter::UploadCompleteSamples() {
  for (Sample& sample : complete_samples_) {
    base::UmaHistogramCounts1000("Media.AudioCapturerDroppedData10sIntervals",
                                 sample.dropped_data_count_);
    base::UmaHistogramCounts1000(
        "Media.AudioCapturerMissedReadDeadline10sIntervals",
        sample.missed_read_deadline_count_);
  }
  complete_samples_.clear();
}

}  // namespace audio
