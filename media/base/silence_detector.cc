// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/silence_detector.h"

#include "media/base/audio_bus.h"
#include "media/base/audio_timestamp_helper.h"

namespace media {

SilenceDetector::SilenceDetector(int sample_rate, base::TimeDelta threshold)
    : silent_samples_needed_(
          AudioTimestampHelper::TimeToFrames(threshold, sample_rate)),
      consecutive_silent_samples_(silent_samples_needed_) {
  // Start out as silent, by forcing `consecutive_silent_samples_` to the
  // minimum threshold.
  // We prefer starting silent because the silent -> audible transition is
  // instantaneous, whereas it takes `threshold` time to go from audible ->
  // silent.
}

void SilenceDetector::ResetToSilence() {
  // Must never be called when Scan() is being called, as
  // `consecutive_silent_samples_` is not protected by lock.
  NON_REENTRANT_SCOPE(exclusive_use_checker_);

  // Reset `consecutive_silent_samples_` so we report silence.
  // This value is read/written by Scan() on a different thread, but
  // ResetToSilence() must not be called when Scan() could be.
  consecutive_silent_samples_ = silent_samples_needed_;

  base::AutoLock sample_lock(lock_);
  is_silent_ = true;
}

void SilenceDetector::Scan(const AudioBus& buffer) {
  // Must never be called when ResetToSilence() is being called, as
  // `consecutive_silent_samples_` is not protected by lock.
  NON_REENTRANT_SCOPE(exclusive_use_checker_);

  // We can't detect silence in bitstream formats.
  CHECK(!buffer.is_bitstream_format());

  if (buffer.AreFramesZero()) {
    consecutive_silent_samples_ += buffer.frames();
  } else {
    consecutive_silent_samples_ = 0;
  }

  base::AutoTryLock try_lock(lock_);
  if (try_lock.is_acquired()) {
    is_silent_ = consecutive_silent_samples_ >= silent_samples_needed_;
  }
}

bool SilenceDetector::IsSilent() {
  base::AutoLock auto_lock(lock_);
  return is_silent_;
}

}  // namespace media
