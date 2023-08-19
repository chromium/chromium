// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/realtime_audio_thread.h"

#include <cstring>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "media/audio/simple_sources.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace audio {

class RealtimeAudioThreadTest : public testing::Test {
 public:
  RealtimeAudioThreadTest() = default;
  ~RealtimeAudioThreadTest() override = default;
};

#if BUILDFLAG(IS_APPLE)
TEST_F(RealtimeAudioThreadTest, GetRealtimePeriod) {
  constexpr base::TimeDelta kPeriod = base::Milliseconds(123);

  RealtimeAudioThread thread("TestThread", kPeriod);

  EXPECT_EQ(thread.GetRealtimePeriod(), kPeriod);
}
#endif

TEST_F(RealtimeAudioThreadTest, StartStop) {
  constexpr base::TimeDelta kPeriod = base::Milliseconds(123);

  RealtimeAudioThread thread("TestThread", kPeriod);

  base::Thread::Options options;
  options.thread_type = base::ThreadType::kRealtimeAudio;
  EXPECT_TRUE(thread.StartWithOptions(std::move(options)));

  thread.Stop();
}

TEST_F(RealtimeAudioThreadTest, StartDestroy) {
  constexpr base::TimeDelta kPeriod = base::Milliseconds(123);

  RealtimeAudioThread thread("TestThread", kPeriod);

  base::Thread::Options options;
  options.thread_type = base::ThreadType::kRealtimeAudio;
  EXPECT_TRUE(thread.StartWithOptions(std::move(options)));

  // ~RealtimeAudioThread() will be called without Stop() here.
}

}  // namespace audio
