// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_SILENCE_DETECTOR_H_
#define MEDIA_BASE_SILENCE_DETECTOR_H_

#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/base/reentrancy_checker.h"

namespace media {

class AudioBus;

// Helper class which tracks how many zero'ed frames we have encountered, and
// reports silence after a certain threshold has been met.
// The class is mostly thread-safe: Scan() can be called from a real-time
// thread while IsSilent() is read from a different thread.
// ResetToSilence() should not be called when Scan() can be called from a
// different thread.
class MEDIA_EXPORT SilenceDetector {
 public:
  // `sample_rate` is the audio signal sample rate (Hz).
  // `threshold` is how much zero'ed audio data must be scanned before silence
  // is reported.
  SilenceDetector(int sample_rate, base::TimeDelta threshold);

  SilenceDetector(const SilenceDetector&) = delete;
  SilenceDetector& operator=(const SilenceDetector&) = delete;

  ~SilenceDetector() = default;

  // Resets the internal state to silence.
  // Must not be called when Scan() could be called from another thread.
  void ResetToSilence();

  // Scan audio data from `buffer` for silence.  It is safe to call this
  // from a real-time priority thread.
  void Scan(const AudioBus& buffer);

  // Can be called from any thread.
  bool IsSilent();

 private:
  // Use to make sure Scan() and ResetToSilence() are not called concurrently.
  REENTRANCY_CHECKER(exclusive_use_checker_);

  // Number of zero'ed samples needed before we report silence.
  const int64_t silent_samples_needed_;

  // Number of consecutive frames of silence scanned.
  int64_t consecutive_silent_samples_;

  base::Lock lock_;
  // Starts silent, since the silent -> !silent transition is instantaneous,
  // and we will be in the right state after the first Scan().
  // The !silent -> silent transition takes `threshold` time.
  bool is_silent_ GUARDED_BY(lock_) = true;
};

}  // namespace media

#endif  // MEDIA_BASE_SILENCE_DETECTOR_H_
