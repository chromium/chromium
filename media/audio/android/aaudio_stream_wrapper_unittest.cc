// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/android/aaudio_stream_wrapper.h"

#include <aaudio/AAudio.h>

#include <memory>

#include "base/android/android_info.h"
#include "base/test/task_environment.h"
#include "media/base/audio_parameters.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

class MockDataCallback : public AAudioStreamWrapper::DataCallback {
 public:
  MockDataCallback() = default;
  ~MockDataCallback() override = default;
  bool OnAudioDataRequested(base::span<float> audio_data) override {
    return true;
  }
  void OnError() override {}
  void OnDeviceChange() override {}
};

}  // namespace

class AAudioStreamWrapperTest : public testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  MockDataCallback callback_;
};

TEST_F(AAudioStreamWrapperTest, DeferredHelperOutlivesStreamClose) {
  AudioParameters params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         ChannelLayoutConfig::Stereo(), 48000, 480);

  auto wrapper = std::make_unique<AAudioStreamWrapper>(
      &callback_, AAudioStreamWrapper::StreamType::kOutput, params,
      android::AudioDevice::Default(), AAUDIO_USAGE_MEDIA);

  // Destroy the wrapper. This should trigger the deferred teardown on API <= 30.
  wrapper.reset();

  auto sdk_int = base::android::android_info::sdk_int();
  if (sdk_int < 31) {
    // We expect two tasks to be posted:
    // 1. Close task at T+1s.
    // 2. Free task at T+1.25s.
    EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 2u);

    // Fast forward to T+1s (but not 1.25s).
    task_environment_.FastForwardBy(base::Seconds(1));

    // The close task should have run, but the free task should still be pending.
    EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 1u);

    // Fast forward the rest.
    task_environment_.FastForwardBy(base::Milliseconds(250));
    EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0u);
  } else {
    // On API >= 31, destruction should be synchronous, no tasks posted.
    EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0u);
  }
}

TEST_F(AAudioStreamWrapperTest, DestroyWithPendingDeferredTasks) {
  // This test verifies that if the TaskEnvironment is destroyed (or sequence shut down)
  // while tasks are pending, it doesn't crash.
  AudioParameters params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         ChannelLayoutConfig::Stereo(), 48000, 480);
  auto wrapper = std::make_unique<AAudioStreamWrapper>(
      &callback_, AAudioStreamWrapper::StreamType::kOutput, params,
      android::AudioDevice::Default(), AAUDIO_USAGE_MEDIA);
  wrapper.reset();
  // We leave the tasks pending. The fixture's `task_environment_` will be
  // destroyed on teardown, verifying that destroying pending tasks doesn't crash.
}

}  // namespace media
