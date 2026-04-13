// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/pipewire_audio_capturer.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

TEST(PipewireAudioCapturerTest, Create) {
  // This just tests that Create() doesn't crash.
  // It might return null or a valid capturer depending on the environment.
  auto capturer = PipewireAudioCapturer::Create();
  if (PipewireAudioCapturer::IsSupported()) {
    EXPECT_TRUE(capturer);
  } else {
    EXPECT_FALSE(capturer);
  }
}

}  // namespace remoting
