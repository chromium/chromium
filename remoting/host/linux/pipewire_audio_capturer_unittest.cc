// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/pipewire_audio_capturer.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "remoting/proto/audio.pb.h"
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

// Verifies that a capturer can be started and immediately torn down without
// crashing. The PipeWire stream uses a real-time data thread, so the
// destruction sequence must ensure that thread has drained before stream
// resources owned by the capturer are released.
TEST(PipewireAudioCapturerTest, StartAndDestroy) {
  if (!PipewireAudioCapturer::IsSupported()) {
    GTEST_SKIP() << "PipeWire is not available in this environment.";
  }

  base::test::TaskEnvironment task_environment;
  for (int i = 0; i < 5; ++i) {
    auto capturer = PipewireAudioCapturer::Create();
    ASSERT_TRUE(capturer);
    capturer->Start(
        base::BindRepeating([](std::unique_ptr<AudioPacket> packet) {}));
    capturer.reset();
  }
}

}  // namespace remoting
