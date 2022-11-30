// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_io.h"

namespace media {

int AudioOutputStream::AudioSourceCallback::OnMoreData(
    base::TimeDelta delay,
    base::TimeTicks delay_timestamp,
    const AudioGlitchInfo& glitch_info,
    AudioBus* dest,
    bool is_mixing) {
  // Ignore the `is_mixing` flag by default.
  return OnMoreData(delay, delay_timestamp, glitch_info, dest);
}

}  // namespace media
