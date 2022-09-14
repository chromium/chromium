// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_POWER_MONITOR_H_
#define MEDIA_BASE_AUDIO_POWER_MONITOR_H_

#include <limits>
#include <utility>

#include "base/synchronization/lock.h"
#include "media/base/media_export.h"

// An audio signal power monitor.  It is periodically provided an AudioBus by
// the native audio thread, and the audio samples in each channel are analyzed
// to determine the average power of the signal over a time period.  Here
// "average power" is a running average calculated by using a first-order
// low-pass filter over the square of the samples scanned.  Whenever reporting
// the power level, this running average is converted to dBFS (decibels relative
// to full-scale) units.
//
// Note that extreme care has been taken to make the AudioPowerMonitor::Scan()
// method safe to be called on the native audio thread.  The code acquires no
// locks, nor engages in any operation that could result in an
// undetermined/unbounded amount of run-time.

namespace base {
class TimeDelta;
}

namespace media {

class AudioBus;

class MEDIA_EXPORT AudioPowerMonitor {
 public:
  // |sample_rate| is the audio signal sample rate (Hz).  |time_constant|
  // characterizes how samples are averaged over time to determine the power
  // level; and is the amount of time it takes a zero power level to increase to
  // ~63.2% of maximum given a step input signal.
  AudioPowerMonitor(int sample_rate, base::TimeDelta time_constant);

  AudioPowerMonitor(const AudioPowerMonitor&) = delete;
  AudioPowerMonitor& operator=(const AudioPowerMonitor&) = delete;

  ~AudioPowerMonitor();

  // Reset power monitor to initial state (zero power level).  This should not
  // be called while another thread is scanning.
  void Reset();

  // Scan more |frames| of audio data from |buffer|.  It is safe to call this
  // from a real-time priority thread.
  void Scan(const AudioBus& buffer, int frames);

  // Returns the current power level in dBFS and clip status.  Clip status is
  // true whenever any *one* sample scanned exceeded maximum amplitude since
  // this method's last invocation.  It is safe to call this method from any
  // thread.
  std::pair<float, bool> ReadCurrentPowerAndClip();

  // dBFS value corresponding to zero power in the audio signal.
  static float zero_power() { return -std::numeric_limits<float>::infinity(); }

  // dBFS value corresponding to maximum power in the audio signal.
  static float max_power() { return 0.0f; }

 private:
  // The weight applied when averaging-in each sample.  Computed from the
  // |sample_rate| and |time_constant|.
  const float sample_weight_;

  // Accumulated results over one or more calls to Scan().  These should only be
  // touched by the thread invoking Scan().
  float average_power_;
  bool has_clipped_;

  // Copies of power and clip status, used to deliver results synchronously
  // across threads.
  base::Lock reading_lock_;
  float power_reading_;
  bool clipped_reading_;
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_POWER_MONITOR_H_
