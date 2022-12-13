// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/renderer/web_engine_audio_output_device.h"

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/fuchsia/audio/fake_audio_consumer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr int kSampleRate = 44100;
constexpr media::ChannelLayout kChannelLayout = media::CHANNEL_LAYOUT_STEREO;
constexpr int kNumChannels = 2;
constexpr uint64_t kTestSessionId = 42;
constexpr base::TimeDelta kPeriod = base::Milliseconds(10);
constexpr int kFramesPerPeriod = 441;

}  // namespace

class TestRenderer : public media::AudioRendererSink::RenderCallback {
 public:
  TestRenderer() = default;
  ~TestRenderer() override = default;

  // AudioRendererSink::Renderer interface.
  int Render(base::TimeDelta delay,
             base::TimeTicks delay_timestamp,
             const media::AudioGlitchInfo& audio_glitch_info,
             media::AudioBus* dest) override {
    EXPECT_EQ(dest->channels(), kNumChannels);
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

  int num_render_errors() const { return num_render_errors_; }

  base::TimeTicks last_presentation_time() const {
    return last_presentation_time_;
  }

 private:
  int frames_rendered_ = 0;
  int num_render_errors_ = 0;
  base::TimeTicks last_presentation_time_;
};

class WebEngineAudioOutputDeviceTest : public testing::Test {
 public:
  WebEngineAudioOutputDeviceTest() {
    fidl::InterfaceHandle<fuchsia::media::AudioConsumer> audio_consumer;
    fake_audio_consumer_ = std::make_unique<media::FakeAudioConsumer>(
        kTestSessionId, audio_consumer.NewRequest());

    output_device_ = WebEngineAudioOutputDevice::Create(
        std::move(audio_consumer),
        base::SingleThreadTaskRunner::GetCurrentDefault());
  }

  ~WebEngineAudioOutputDeviceTest() override {
    // Stop() must be called before destruction to release resources.
    output_device_->Stop();
    // WebEngineAudioOutputDevice::Stop() posts a task to run
    // StopOnAudioThread() on `task_runner_`. RunUntilIdle() ensures the request
    // to stop is fulfilled.
    task_environment_.RunUntilIdle();
  }

 protected:
  void Initialize() {
    output_device_->Initialize(
        media::AudioParameters(
            media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
            media::ChannelLayoutConfig::FromLayout<kChannelLayout>(),
            kSampleRate, kFramesPerPeriod),
        &renderer_);

    task_environment_.RunUntilIdle();
    EXPECT_EQ(renderer_.frames_rendered(), 0);
  }

  void InitializeAndStart() {
    Initialize();

    // As soon as Start() is processed WebEngineAudioOutputDevice is expected to
    // start rendering some samples.
    output_device_->Start();
    task_environment_.RunUntilIdle();
    EXPECT_GT(renderer_.frames_rendered(), 0);
  }

  void CallPumpSamples() {
    output_device_->PumpSamples(base::TimeTicks::Now() +
                                base::Milliseconds(200));
  }

  void ValidatePresentationTime() {
    // Verify that the current renderer lead time is in the
    // [min_lead_time, min_lead_time + 30ms] range. 30ms is chosen to allow
    // WebEngineAudioOutputDevice to pre-render slightely ahead of the target
    // time, while keeping latency reasonably low.
    auto lead_time =
        renderer_.last_presentation_time() - base::TimeTicks::Now();
    EXPECT_GT(lead_time, media::FakeAudioConsumer::kMinLeadTime);
    EXPECT_LT(lead_time,
              media::FakeAudioConsumer::kMinLeadTime + base::Milliseconds(30));
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<media::FakeAudioConsumer> fake_audio_consumer_;
  TestRenderer renderer_;
  scoped_refptr<WebEngineAudioOutputDevice> output_device_;
};

TEST_F(WebEngineAudioOutputDeviceTest, Start) {
  Initialize();

  // Verify that playback doesn't start before Start().
  task_environment_.FastForwardBy(base::Seconds(2));
  EXPECT_EQ(renderer_.frames_rendered(), 0);

  // Rendering should start after Start().
  output_device_->Start();
  task_environment_.RunUntilIdle();
  EXPECT_GT(renderer_.frames_rendered(), 0);

  ValidatePresentationTime();
}

TEST_F(WebEngineAudioOutputDeviceTest, StartAndPlay) {
  InitializeAndStart();

  renderer_.reset_frames_rendered();

  // Try advancing time and verify that WebEngineAudioOutputDevice keeps calling
  // Render().
  for (int i = 0; i < 3; ++i) {
    task_environment_.FastForwardBy(kPeriod);
    EXPECT_EQ(renderer_.frames_rendered(), kFramesPerPeriod);
    renderer_.reset_frames_rendered();
  }
}

TEST_F(WebEngineAudioOutputDeviceTest, Pause) {
  InitializeAndStart();

  renderer_.reset_frames_rendered();

  // Advancing time and verify that WebEngineAudioOutputDevice keeps calling
  // Render().
  task_environment_.FastForwardBy(kPeriod);
  EXPECT_EQ(renderer_.frames_rendered(), kFramesPerPeriod);
  renderer_.reset_frames_rendered();

  // Render() should not be called while paused.
  output_device_->Pause();
  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_EQ(renderer_.frames_rendered(), 0);

  // Unpause the stream and verify that Render() is being called now.
  output_device_->Play();
  task_environment_.FastForwardBy(kPeriod);
  EXPECT_GT(renderer_.frames_rendered(), 0);
}

TEST_F(WebEngineAudioOutputDeviceTest, Underflow) {
  InitializeAndStart();

  renderer_.reset_frames_rendered();

  // Missing the timer once should not cause any issues. Timer tasks can't
  // always run at the exact scheduled time. WebEngineAudioOutputDevice should
  // be resilient to small delays.
  task_environment_.AdvanceClock(kPeriod * 2);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(renderer_.frames_rendered(), kFramesPerPeriod * 2);
  renderer_.reset_frames_rendered();

  // Advance time by 100ms, causing some frames to be skipped.
  task_environment_.AdvanceClock(kPeriod * 10);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(renderer_.frames_rendered(), kFramesPerPeriod * 3);
  renderer_.reset_frames_rendered();

  ValidatePresentationTime();
}

TEST_F(WebEngineAudioOutputDeviceTest, Error) {
  InitializeAndStart();

  renderer_.reset_frames_rendered();

  fake_audio_consumer_.reset();
  task_environment_.RunUntilIdle();

  EXPECT_EQ(renderer_.num_render_errors(), 1);
  EXPECT_EQ(renderer_.frames_rendered(), 0);
}

TEST_F(WebEngineAudioOutputDeviceTest, Stop) {
  InitializeAndStart();

  renderer_.reset_frames_rendered();

  // Call Stop() and then PumpSamples() immediately after that. The callback
  // should not be called.
  output_device_->Stop();
  CallPumpSamples();
  EXPECT_EQ(renderer_.frames_rendered(), 0);
}
