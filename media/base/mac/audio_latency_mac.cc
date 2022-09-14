// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mac/audio_latency_mac.h"
#include "base/check_op.h"
#include "media/base/limits.h"

namespace media {

int GetMinAudioBufferSizeMacOS(int min_buffer_size, int sample_rate) {
  int buffer_size = min_buffer_size;
  if (sample_rate > 48000) {
    // The default buffer size is too small for higher sample rates and may lead
    // to glitching.  Adjust upwards by multiples of the default size.
    if (sample_rate <= 96000)
      buffer_size = 2 * limits::kMinAudioBufferSize;
    else if (sample_rate <= 192000)
      buffer_size = 4 * limits::kMinAudioBufferSize;
  }
  DCHECK_EQ(limits::kMaxWebAudioBufferSize % buffer_size, 0);
  return buffer_size;
}

}  // namespace media
