// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_LATENCY_H_
#define MEDIA_BASE_AUDIO_LATENCY_H_

#include "media/base/media_shmem_export.h"

namespace base {
class TimeDelta;
}

namespace media {

class MEDIA_SHMEM_EXPORT AudioLatency {
 public:
  // Categories of expected latencies for input/output audio. Do not change
  // existing values, they are used for UMA histogram reporting.
  enum LatencyType {
    // Specific latency in milliseconds.
    LATENCY_EXACT_MS = 0,
    // Lowest possible latency which does not cause glitches.
    LATENCY_INTERACTIVE = 1,
    // Latency optimized for real time communication.
    LATENCY_RTC = 2,
    // Latency optimized for continuous playback and power saving.
    LATENCY_PLAYBACK = 3,
    // For validation only.
    LATENCY_LAST = LATENCY_PLAYBACK,
    LATENCY_COUNT = LATENCY_LAST + 1
  };

  // Indicates if the OS does not require resampling for playback.
  static bool IsResamplingPassthroughSupported(LatencyType type);

  // |preferred_buffer_size| should be set to 0 if a client has no preference.
  static int GetHighLatencyBufferSize(int sample_rate,
                                      int preferred_buffer_size);

  // |hardware_buffer_size| should be set to 0 if unknown/invalid/not preferred.
  static int GetRtcBufferSize(int sample_rate, int hardware_buffer_size);

  static int GetInteractiveBufferSize(int hardware_buffer_size);

  // Return the closest buffer size for this platform that will result in a
  // latency not less than |duration| for the given |sample_rate|.
  //
  // Requirements:
  // - |hardware_buffer_size| > 0 and |max_allowed_buffer_size| > 0.
  // - If |min_hardware_buffer_size| and |max_hardware_buffer_size| are > 0 then
  //   the following must be true: |min_hardware_buffer_size| <=
  //   |hardware_buffer_size| <= |max_hardware_buffer_size| <=
  //   |max_allowed_buffer_size|.
  //
  // The returned buffer size is guaranteed to be between
  // |min_hardware_buffer_size| and |max_allowed_buffer_size|.
  // |max_hardware_buffer_size| is used to help determine a buffer size that
  // won't cause the caller and the hardware to run at unsynchronized buffer
  // sizes (e.g. hardware running at 4096 and caller running at 4224).
  // |hardware_buffer_size| is the platform's preferred buffer size.
  //
  // It is valid for both the min and max to be zero in which case only
  // |hardware_buffer_size| and multiples of it will be used.
  static int GetExactBufferSize(base::TimeDelta duration,
                                int sample_rate,
                                int hardware_buffer_size,
                                int min_hardware_buffer_size,
                                int max_hardware_buffer_size,
                                int max_allowed_buffer_size);
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_LATENCY_H_
