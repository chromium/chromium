// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>

#include "base/memory/aligned_memory.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/audio/audio_device_info_accessor_for_tests.h"
#include "media/audio/audio_features.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_unittest_util.h"
#include "media/audio/simple_sources.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/limits.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class AudioOutputTest : public testing::TestWithParam<bool> {
 public:
  AudioOutputTest() {
    audio_manager_ =
        AudioManager::CreateForTesting(std::make_unique<TestAudioThread>());
    audio_manager_device_info_ =
        std::make_unique<AudioDeviceInfoAccessorForTests>(audio_manager_.get());
#if defined(OS_ANDROID)
    // The only parameter is used to enable/disable AAudio.
    if (GetParam())
      features_.InitAndEnableFeature(features::kUseAAudioDriver);
#endif
    base::RunLoop().RunUntilIdle();
  }
  ~AudioOutputTest() override {
    if (stream_)
      stream_->Close();
    audio_manager_->Shutdown();
  }

  void CreateWithDefaultParameters() {
    stream_params_ =
        audio_manager_device_info_->GetDefaultOutputStreamParameters();
    stream_ = audio_manager_->MakeAudioOutputStream(
        stream_params_, std::string(), AudioManager::LogCallback());
  }

  // Runs message loop for the specified amount of time.
  void RunMessageLoop(base::TimeDelta delay) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), delay);
    run_loop.Run();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  std::unique_ptr<AudioManager> audio_manager_;
  std::unique_ptr<AudioDeviceInfoAccessorForTests> audio_manager_device_info_;
  AudioParameters stream_params_;
  AudioOutputStream* stream_ = nullptr;
#if defined(OS_ANDROID)
  base::test::ScopedFeatureList features_;
#endif
};

// Test that can it be created and closed.
TEST_P(AudioOutputTest, GetAndClose) {
  ABORT_AUDIO_TEST_IF_NOT(audio_manager_device_info_->HasAudioOutputDevices());
  CreateWithDefaultParameters();
  ASSERT_TRUE(stream_);
}

// Test that it can be opened and closed.
TEST_P(AudioOutputTest, OpenAndClose) {
  ABORT_AUDIO_TEST_IF_NOT(audio_manager_device_info_->HasAudioOutputDevices());

  CreateWithDefaultParameters();
  ASSERT_TRUE(stream_);
  EXPECT_TRUE(stream_->Open());
}

// Verify that Stop() can be called before Start().
TEST_P(AudioOutputTest, StopBeforeStart) {
  ABORT_AUDIO_TEST_IF_NOT(audio_manager_device_info_->HasAudioOutputDevices());
  CreateWithDefaultParameters();
  EXPECT_TRUE(stream_->Open());
  stream_->Stop();
}

// Verify that Stop() can be called more than once.
TEST_P(AudioOutputTest, StopTwice) {
  ABORT_AUDIO_TEST_IF_NOT(audio_manager_device_info_->HasAudioOutputDevices());
  CreateWithDefaultParameters();
  EXPECT_TRUE(stream_->Open());
  SineWaveAudioSource source(1, 200.0, stream_params_.sample_rate());

  stream_->Start(&source);
  stream_->Stop();
  stream_->Stop();
}

// This test produces actual audio for .25 seconds on the default device.
TEST_P(AudioOutputTest, Play200HzTone) {
  ABORT_AUDIO_TEST_IF_NOT(audio_manager_device_info_->HasAudioOutputDevices());

  stream_params_ =
      audio_manager_device_info_->GetDefaultOutputStreamParameters();
  stream_ = audio_manager_->MakeAudioOutputStream(stream_params_, std::string(),
                                                  AudioManager::LogCallback());
  ASSERT_TRUE(stream_);

  SineWaveAudioSource source(1, 200.0, stream_params_.sample_rate());

  // Play for 100ms.
  const int samples_to_play = stream_params_.sample_rate() / 10;

  EXPECT_TRUE(stream_->Open());
  stream_->SetVolume(1.0);

  // Play the stream until position gets past |samples_to_play|.
  base::RunLoop run_loop;
  source.set_on_more_data_callback(
      base::BindLambdaForTesting([&source, &run_loop, samples_to_play]() {
        if (source.pos_samples() >= samples_to_play)
          run_loop.Quit();
      }));
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), TestTimeouts::action_timeout());

  stream_->Start(&source);
  run_loop.Run();

  stream_->Stop();

  EXPECT_FALSE(source.errors());
  EXPECT_GE(source.callbacks(), 1);
  EXPECT_GE(source.pos_samples(), samples_to_play);
}

// Test that SetVolume() and GetVolume() work as expected.
TEST_P(AudioOutputTest, VolumeControl) {
  ABORT_AUDIO_TEST_IF_NOT(audio_manager_device_info_->HasAudioOutputDevices());

  CreateWithDefaultParameters();
  ASSERT_TRUE(stream_);
  EXPECT_TRUE(stream_->Open());

  double volume = 0.0;

  stream_->GetVolume(&volume);
  EXPECT_EQ(volume, 1.0);

  stream_->SetVolume(0.5);

  stream_->GetVolume(&volume);
  EXPECT_LT(volume, 0.51);
  EXPECT_GT(volume, 0.49);
  stream_->Stop();
}

// The test parameter is only relevant on Android. It controls whether or not we
// allow the use of AAudio.
INSTANTIATE_TEST_SUITE_P(Base, AudioOutputTest, testing::Values(false));

#if defined(OS_ANDROID)
// Run tests with AAudio enabled. On Android O and below, this is the same as
// the Base test above, as we only use AAudio on P+.
INSTANTIATE_TEST_SUITE_P(AAudio, AudioOutputTest, testing::Values(true));
#endif

}  // namespace media
