// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/android/aaudio_stream_wrapper.h"

#include <aaudio/AAudio.h>

#include <memory>

#include "base/android/android_info.h"
#include "base/functional/callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "media/base/audio_parameters.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

constexpr int kSampleRate = 48000;
constexpr int kFramesPerBuffer = 480;

// Mirrors the teardown delays in aaudio_stream_wrapper.cc. The stream is only
// closed after `kCloseDelay` on Android R and earlier.
constexpr base::TimeDelta kCloseDelay = base::Seconds(1);
constexpr base::TimeDelta kDestructionDelay =
    kCloseDelay + base::Milliseconds(250);

class MockDataCallback : public AAudioStreamWrapper::DataCallback {
 public:
  MockDataCallback() = default;
  ~MockDataCallback() override = default;
  bool OnAudioDataRequested(base::span<float> audio_data) override {
    return true;
  }
  void OnError() override {}
  void OnDeviceChange() override { ++device_change_count_; }

  int device_change_count() const { return device_change_count_; }

 private:
  int device_change_count_ = 0;
};

}  // namespace

class AAudioStreamWrapperTest : public testing::Test {
 protected:
  std::unique_ptr<AAudioStreamWrapper> CreateWrapper() {
    return std::make_unique<AAudioStreamWrapper>(
        &callback_, AAudioStreamWrapper::StreamType::kOutput,
        AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                        ChannelLayoutConfig::Stereo(), kSampleRate,
                        kFramesPerBuffer),
        android::AudioDevice::Default(), AAUDIO_USAGE_MEDIA);
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  MockDataCallback callback_;
};

// Makes sure `AAudioDestructionHelper` outlives `AAudioStream_close()` on all
// Android versions.
TEST_F(AAudioStreamWrapperTest, DeferredHelperOutlivesStreamClose) {
  auto wrapper = CreateWrapper();
  wrapper.reset();

  // On Android R and earlier, we post one task to close the stream (at T+1s)
  // and one to destroy the helper (at T+1.25s). On Android S+, the stream is
  // closed synchronously, and we only post the task which destroys the helper.
  const size_t expected_tasks =
      base::android::android_info::sdk_int() <
              base::android::android_info::SDK_VERSION_S
          ? 2u
          : 1u;
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), expected_tasks);

  // The helper should still be alive once the stream is closed.
  task_environment_.FastForwardBy(kCloseDelay);
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 1u);

  task_environment_.FastForwardBy(kDestructionDelay - kCloseDelay);
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0u);
}

// Makes sure an error callback which was already in flight when the stream was
// closed is ignored, instead of using a destroyed `AAudioDestructionHelper`.
// See crbug.com/574907318.
// Note: this test can't fail deterministically if the helper is destroyed too
//       early, as the late callbacks would then access the destroyed helper.
//       `DeferredHelperOutlivesStreamClose` covers that case.
TEST_F(AAudioStreamWrapperTest, LateErrorCallbackIsIgnored) {
  auto wrapper = CreateWrapper();
  const base::RepeatingClosure error_callback =
      wrapper->GetErrorCallbackForTesting();

  // Errors are forwarded while the wrapper is alive.
  error_callback.Run();
  EXPECT_EQ(callback_.device_change_count(), 1);

  wrapper.reset();

  // Run a late callback.
  error_callback.Run();
  EXPECT_EQ(callback_.device_change_count(), 1);

  // Run another one, once the stream is closed on all Android versions.
  task_environment_.FastForwardBy(kCloseDelay);
  error_callback.Run();
  EXPECT_EQ(callback_.device_change_count(), 1);
}

TEST_F(AAudioStreamWrapperTest, DestroyWithPendingDeferredTasks) {
  // This test verifies that if the TaskEnvironment is destroyed (or sequence shut down)
  // while tasks are pending, it doesn't crash.
  auto wrapper = CreateWrapper();
  wrapper.reset();
  // We leave the tasks pending. The fixture's `task_environment_` will be
  // destroyed on teardown, verifying that destroying pending tasks doesn't crash.
}

}  // namespace media
