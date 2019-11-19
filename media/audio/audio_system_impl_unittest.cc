// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_system_impl.h"

#include "base/test/task_environment.h"
#include "media/audio/audio_system_test_util.h"
#include "media/audio/audio_thread_impl.h"
#include "media/audio/mock_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// TODO(olka): These are the only tests for AudioSystemHelper. Make sure that
// AudioSystemHelper is tested if AudioSystemImpl goes away.

// Typed tests cannot be parametrized, so using template parameter instead of
// inheriting from TestWithParams<>
template <bool use_audio_thread>
class AudioSystemImplTestBase : public testing::Test {
 public:
  AudioSystemImplTestBase() = default;

  ~AudioSystemImplTestBase() override = default;

  void SetUp() override {
    audio_manager_ = std::make_unique<MockAudioManager>(
        std::make_unique<TestAudioThread>(use_audio_thread));
    audio_system_ = std::make_unique<AudioSystemImpl>(audio_manager_.get());
  }
  void TearDown() override { audio_manager_->Shutdown(); }

 protected:
  MockAudioManager* audio_manager() { return audio_manager_.get(); }
  AudioSystem* audio_system() { return audio_system_.get(); }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<MockAudioManager> audio_manager_;
  std::unique_ptr<AudioSystem> audio_system_;
  // AudioSystemTester tester_;
};

using AudioSystemTestBaseVariations =
    testing::Types<AudioSystemImplTestBase<false>,
                   AudioSystemImplTestBase<true>>;

INSTANTIATE_TYPED_TEST_SUITE_P(AudioSystemImpl,
                               AudioSystemTestTemplate,
                               AudioSystemTestBaseVariations);

}  // namespace media
