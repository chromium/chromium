// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_io.h"

namespace media {

int AudioOutputStream::AudioSourceCallback::OnMoreData(
    base::TimeDelta delay,
    base::TimeTicks delay_timestamp,
    int prior_frames_skipped,
    AudioBus* dest,
    bool is_mixing) {
  // Ignore the `is_mixing` flag by default.
  return OnMoreData(delay, delay_timestamp, prior_frames_skipped, dest);
}

}  // namespace media