// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/voice_isolation/passthrough_voice_isolation.h"

#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {

TEST(PassthroughVoiceIsolationTest, CorrectFrameSizeAndDelay) {
  PassthroughVoiceIsolation passthrough(480, 480);

  EXPECT_EQ(passthrough.FrameSize(), 480u);
  EXPECT_EQ(passthrough.FramesPerSecond(), 480u);
}

TEST(PassthroughVoiceIsolationTest, ProcessAudioPassthrough) {
  PassthroughVoiceIsolation passthrough(10, 160);
  std::vector<float> input(10, 1.0f);
  std::vector<float> output(10, 0.0f);

  passthrough.ProcessAudio(input, output);

  for (size_t i = 0; i < 10; ++i) {
    EXPECT_EQ(output[i], input[i]);
  }
}

}  // namespace
}  // namespace media
