// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/audio/fuchsia_audio_output_device.h"

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/fuchsia/audio/fake_audio_consumer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

constexpr int kSampleRate = 44100;
constexpr ChannelLayout kChannelLayout = CHANNEL_LAYOUT_STEREO;
constexpr int kNumChannels = 2;
constexpr uint64_t kTestSessionId = 42;
constexpr base::TimeDelta kPeriod = base::TimeDelta::FromMilliseconds(10);
constexpr int kFramesPerPeriod = 441;

class TestRenderer : public AudioRendererSink::RenderCallback {
 public:
  TestRenderer() = default;
  ~TestRenderer() override = default;

  // AudioRendererSink::Renderer interface.
  int Render(base::TimeDelta delay,
             base::TimeTicks delay_timestamp,
             int prior_frames_skipped,
             AudioBus* dest) override {
    EXPECT_EQ(dest->channels(), kNumChannels);
    frames_skipped_ += prior_frames_skipped;
    frames_rendered_ += dest->frames();

    EXPECT_GT(delay, base::TimeDelta());
    auto presentation_time = delay_timestamp + delay;
    EXPECT_GT(presentation_time, last_presentation_time_);
    last_presentation_time_ = presentation_time;

    return dest->frames();
  }
  void OnRenderError() override { num_render_errors_++; }

  int frames_rendered() const { return frames_rendered_; }
  void reset_frames_rendered() { frames_rendered_ = 0; }

  int frames_skipped() const { return frames_skipped_; }
  int num_render_errors() const { return num_render_errors_; }

  base::TimeTicks last_presentation_time() const {
    return last_presentation_time_;
  }

 private:
  int frames_rendered_ = 0;
  int frames_skipped_ = 0;
  int num_render_errors_ = 0;
  base::TimeTicks last_presentation_time_;
};

class FuchsiaAudioOutputDeviceTest : public testing::Test {
 public:
  FuchsiaAudioOutputDeviceTest() {
    fidl::InterfaceHandle<fuchsia::media::AudioConsumer> audio_consumer;
    fake_audio_consumer_ = std::make_unique<FakeAudioConsumer>(
        kTestSessionId, audio_consumer.NewRequest());

    output_device_ = FuchsiaAudioOutputDevice::Create(
        std::move(audio_consumer), base::ThreadTaskRunnerHandle::Get());
  }

  ~FuchsiaAudioOutputDeviceTest() override { output_device_->Stop(); }

 protected:
  void Initialize() {
    output_device_->Initialize(
        AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY, kChannelLayout,
                        kSampleRate, kFramesPerPeriod),
        &renderer_);

    task_environment_.RunUntilIdle();
    EXPECT_EQ(renderer_.frames_rendered(), 0);
  }

  void InitializeAndStart() {
    Initialize();

    // As soon as Start() is processed FuchsiaAudioOutputDevice is expected to
    // start rendering some samples.
    output_device_->Start();
    task_environment_.RunUntilIdle();
    EXPECT_GT(renderer_.frames_rendered(), 0);
  }

  void CallPumpSamples() {
    output_device_->PumpSamples(
        base::TimeTicks::Now() + base::TimeDelta::FromMilliseconds(200), 0);
  }

  void ValidatePresentationTime() {
    // Verify that the current renderer lead time is in the
    // [min_lead_time, min_lead_time + 30ms] range. 30ms is chosen to allow
    // FuchsiaAudioOutputDevice to pre-render slightely ahead of the target
    // time, while keeping latency reasonably low.
    auto lead_time =
        renderer_.last_presentation_time() - base::TimeTicks::Now();
    EXPECT_GT(lead_time, FakeAudioConsumer::kMinLeadTime);
    EXPECT_LT(lead_time, FakeAudioConsumer::kMinLeadTime +
                             base::TimeDelta::FromMilliseconds(30));
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<FakeAudioConsumer> fake_audio_consumer_;
  TestRenderer renderer_;
  scoped_refptr<FuchsiaAudioOutputDevice> output_device_;
};

TEST_F(FuchsiaAudioOutputDeviceTest, Start) {
  Initialize();

  // Verify that playback doesn't start before Start().
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(2));
  EXPECT_EQ(renderer_.frames_rendered(), 0);

  // Rendering should start after Start().
  output_device_->Start();
  task_environment_.RunUntilIdle();
  EXPECT_GT(renderer_.frames_rendered(), 0);

  ValidatePresentationTime();
}

TEST_F(FuchsiaAudioOutputDeviceTest, StartAndPlay) {
  InitializeAndStart();

  renderer_.reset_frames_rendered();

  // Try advancing time and verify that FuchsiaAudioOutputDevice keeps calling
  // Render().
  for (int i = 0; i < 3; ++i) {
    task_environment_.FastForwardBy(kPeriod);
    EXPECT_EQ(renderer_.frames_rendered(), kFramesPerPeriod);
    EXPECT_EQ(renderer_.frames_skipped(), 0);
    renderer_.reset_frames_rendered();
  }
}

TEST_F(FuchsiaAudioOutputDeviceTest, Pause) {
  InitializeAndStart();

  renderer_.reset_frames_rendered();

  // Advancing time and verify that FuchsiaAudioOutputDevice keeps calling
  // Render().
  task_environment_.FastForwardBy(kPeriod);
  EXPECT_EQ(renderer_.frames_rendered(), kFramesPerPeriod);
  EXPECT_EQ(renderer_.frames_skipped(), 0);
  renderer_.reset_frames_rendered();

  // Render() should not be called while paused.
  output_device_->Pause();
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(10));
  EXPECT_EQ(renderer_.frames_rendered(), 0);

  // Unpause the stream and verify that Render() is being called now.
  output_device_->Play();
  task_environment_.FastForwardBy(kPeriod);
  EXPECT_GT(renderer_.frames_rendered(), 0);
  EXPECT_EQ(renderer_.frames_skipped(), 0);
}

TEST_F(FuchsiaAudioOutputDeviceTest, Underflow) {
  InitializeAndStart();

  renderer_.reset_frames_rendered();

  // Missing the timer once should not cause any issues. Timer tasks can't
  // always run at the exact scheduled time. FuchsiaAudioOutputDevice should
  // be resilient to small delays.
  task_environment_.AdvanceClock(kPeriod * 2);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(renderer_.frames_rendered(), kFramesPerPeriod * 2);
  EXPECT_EQ(renderer_.frames_skipped(), 0);
  renderer_.reset_frames_rendered();

  // Advance time by 100ms, causing some frames to be skipped.
  task_environment_.AdvanceClock(kPeriod * 10);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(renderer_.frames_rendered(), kFramesPerPeriod);
  EXPECT_EQ(renderer_.frames_skipped(), kFramesPerPeriod * 9);
  renderer_.reset_frames_rendered();

  ValidatePresentationTime();
}

TEST_F(FuchsiaAudioOutputDeviceTest, Error) {
  InitializeAndStart();

  renderer_.reset_frames_rendered();

  fake_audio_consumer_.reset();
  task_environment_.RunUntilIdle();

  EXPECT_EQ(renderer_.num_render_errors(), 1);
  EXPECT_EQ(renderer_.frames_rendered(), 0);
}

TEST_F(FuchsiaAudioOutputDeviceTest, Stop) {
  InitializeAndStart();

  renderer_.reset_frames_rendered();

  // Call Stop() and then PumpSamples() immediately after that. The callback
  // should not be called.
  output_device_->Stop();
  CallPumpSamples();
  EXPECT_EQ(renderer_.frames_rendered(), 0);
}

}  // namespace media
