// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/voice_isolation/passthrough_voice_isolation.h"

#include "base/check_op.h"
#include "base/logging.h"

namespace media {

PassthroughVoiceIsolation::PassthroughVoiceIsolation(size_t frame_size,
                                                     size_t frames_per_second)
    : frame_size_(frame_size), frames_per_second_(frames_per_second) {
  DVLOG(1)
      << "PassthroughVoiceIsolation::PassthroughVoiceIsolation - frame size="
      << frame_size << " frames_per_second=" << frames_per_second;
}

void PassthroughVoiceIsolation::ProcessAudio(base::span<const float> input,
                                             base::span<float> output) {
  DVLOG(1) << "PassthroughVoiceIsolation::ProcessAudio frame_size="
           << input.size();
  CHECK_EQ(input.size(), frame_size_);
  CHECK_EQ(output.size(), frame_size_);
  output.copy_from(input);
}

size_t PassthroughVoiceIsolation::FrameSize() const {
  return frame_size_;
}

size_t PassthroughVoiceIsolation::FramesPerSecond() const {
  return frames_per_second_;
}

}  // namespace media
