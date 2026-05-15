// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/public/cpp/sounds/sounds_manager.h"

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/compiler_specific.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "media/audio/simple_sources.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/audio_codecs.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/audio/public/cpp/sounds/audio_stream_handler.h"
#include "services/audio/public/cpp/sounds/test_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace audio {

class SoundsManagerTest : public testing::Test {
 public:
  SoundsManagerTest() = default;
  ~SoundsManagerTest() override = default;

  void SetUp() override {
    sounds_manager_ = SoundsManager::Create(base::DoNothing());
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    sounds_manager_.reset();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  SoundsManager& sounds_manager() { return CHECK_DEREF(sounds_manager_); }

 private:
  std::unique_ptr<SoundsManager> sounds_manager_;
  base::test::TaskEnvironment env_;
};

TEST_F(SoundsManagerTest, Play) {
  base::RunLoop run_loop;
  TestObserver observer(run_loop.QuitClosure());

  AudioStreamHandler::SetObserverForTesting(&observer);

  ASSERT_TRUE(sounds_manager().Initialize(
      kTestAudioKey,
      std::string_view(kTestAudioData, std::size(kTestAudioData)),
      media::AudioCodec::kPCM));
  ASSERT_EQ(20, sounds_manager().GetDuration(kTestAudioKey).InMicroseconds());
  ASSERT_TRUE(sounds_manager().Play(kTestAudioKey));
  run_loop.Run();

  ASSERT_EQ(1, observer.num_play_requests());
  ASSERT_EQ(1, observer.num_stop_requests());

  AudioStreamHandler::SetObserverForTesting(nullptr);
}

TEST_F(SoundsManagerTest, Stop) {
  base::RunLoop run_loop;
  TestObserver observer(run_loop.QuitClosure());

  AudioStreamHandler::SetObserverForTesting(&observer);

  ASSERT_TRUE(sounds_manager().Initialize(
      kTestAudioKey,
      std::string_view(kTestAudioData, std::size(kTestAudioData)),
      media::AudioCodec::kPCM));

  ASSERT_EQ(0, observer.num_play_requests());
  ASSERT_EQ(0, observer.num_stop_requests());

  ASSERT_TRUE(sounds_manager().Play(kTestAudioKey));
  ASSERT_TRUE(sounds_manager().Stop(kTestAudioKey));
  run_loop.Run();

  ASSERT_EQ(1, observer.num_play_requests());
  ASSERT_EQ(1, observer.num_stop_requests());

  AudioStreamHandler::SetObserverForTesting(nullptr);
}

TEST_F(SoundsManagerTest, Uninitialized) {
  ASSERT_FALSE(sounds_manager().Play(kTestAudioKey));
  ASSERT_FALSE(sounds_manager().Stop(kTestAudioKey));
}

}  // namespace audio
