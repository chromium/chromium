// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_context.h"

#include <memory>

#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_audio_device.h"
#include "third_party/blink/public/platform/web_audio_latency_hint.h"
#include "third_party/blink/renderer/core/core_initializer.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

namespace blink {

namespace {
static bool web_audio_device_paused_;

class MockWebAudioDeviceForAudioContext : public WebAudioDevice {
 public:
  explicit MockWebAudioDeviceForAudioContext(double sample_rate,
                                             int frames_per_buffer)
      : sample_rate_(sample_rate), frames_per_buffer_(frames_per_buffer) {}
  ~MockWebAudioDeviceForAudioContext() override = default;

  void Start() override {}
  void Stop() override {}
  void Pause() override { web_audio_device_paused_ = true; }
  void Resume() override { web_audio_device_paused_ = false; }
  double SampleRate() override { return sample_rate_; }
  int FramesPerBuffer() override { return frames_per_buffer_; }

 private:
  double sample_rate_;
  int frames_per_buffer_;
};

class AudioContextTestPlatform : public TestingPlatformSupport {
 public:
  std::unique_ptr<WebAudioDevice> CreateAudioDevice(
      unsigned number_of_input_channels,
      unsigned number_of_channels,
      const WebAudioLatencyHint& latency_hint,
      WebAudioDevice::RenderCallback*,
      const WebString& device_id) override {
    double buffer_size = 0;
    const double interactive_size = AudioHardwareBufferSize();
    const double balanced_size = AudioHardwareBufferSize() * 2;
    const double playback_size = AudioHardwareBufferSize() * 4;
    switch (latency_hint.Category()) {
      case WebAudioLatencyHint::kCategoryInteractive:
        buffer_size = interactive_size;
        break;
      case WebAudioLatencyHint::kCategoryBalanced:
        buffer_size = balanced_size;
        break;
      case WebAudioLatencyHint::kCategoryPlayback:
        buffer_size = playback_size;
        break;
      case WebAudioLatencyHint::kCategoryExact:
        buffer_size =
            clampTo(latency_hint.Seconds() * AudioHardwareSampleRate(),
                    static_cast<double>(AudioHardwareBufferSize()),
                    static_cast<double>(playback_size));
        break;
      default:
        NOTREACHED();
        break;
    }

    return std::make_unique<MockWebAudioDeviceForAudioContext>(
        AudioHardwareSampleRate(), buffer_size);
  }

  double AudioHardwareSampleRate() override { return 44100; }
  size_t AudioHardwareBufferSize() override { return 128; }
};

}  // anonymous namespace

class AudioContextTest : public PageTestBase {
 protected:
  AudioContextTest() :
      platform_(new ScopedTestingPlatformSupport<AudioContextTestPlatform>) {}

  ~AudioContextTest() override { platform_.reset(); }

  void SetUp() override {
    PageTestBase::SetUp(IntSize());
    CoreInitializer::GetInstance().ProvideModulesToPage(GetPage(), nullptr);
  }

  void ResetAudioContextManagerForAudioContext(AudioContext* audio_context) {
    audio_context->audio_context_manager_.reset();
  }

  void SetContextState(AudioContext* audio_context,
                       AudioContext::AudioContextState state) {
    audio_context->SetContextState(state);
  }

 private:
  std::unique_ptr<ScopedTestingPlatformSupport<AudioContextTestPlatform>>
      platform_;
};

TEST_F(AudioContextTest, AudioContextOptions_WebAudioLatencyHint) {
  AudioContextOptions* interactive_options = AudioContextOptions::Create();
  interactive_options->setLatencyHint(
      AudioContextLatencyCategoryOrDouble::FromAudioContextLatencyCategory(
          "interactive"));
  AudioContext* interactive_context = AudioContext::Create(
      GetDocument(), interactive_options, ASSERT_NO_EXCEPTION);

  AudioContextOptions* balanced_options = AudioContextOptions::Create();
  balanced_options->setLatencyHint(
      AudioContextLatencyCategoryOrDouble::FromAudioContextLatencyCategory(
          "balanced"));
  AudioContext* balanced_context = AudioContext::Create(
      GetDocument(), balanced_options, ASSERT_NO_EXCEPTION);
  EXPECT_GT(balanced_context->baseLatency(),
            interactive_context->baseLatency());

  AudioContextOptions* playback_options = AudioContextOptions::Create();
  playback_options->setLatencyHint(
      AudioContextLatencyCategoryOrDouble::FromAudioContextLatencyCategory(
          "playback"));
  AudioContext* playback_context = AudioContext::Create(
      GetDocument(), playback_options, ASSERT_NO_EXCEPTION);
  EXPECT_GT(playback_context->baseLatency(), balanced_context->baseLatency());

  AudioContextOptions* exact_too_small_options = AudioContextOptions::Create();
  exact_too_small_options->setLatencyHint(
      AudioContextLatencyCategoryOrDouble::FromDouble(
          interactive_context->baseLatency() / 2));
  AudioContext* exact_too_small_context = AudioContext::Create(
      GetDocument(), exact_too_small_options, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(exact_too_small_context->baseLatency(),
            interactive_context->baseLatency());

  const double exact_latency_sec =
      (interactive_context->baseLatency() + playback_context->baseLatency()) /
      2;
  AudioContextOptions* exact_ok_options = AudioContextOptions::Create();
  exact_ok_options->setLatencyHint(
      AudioContextLatencyCategoryOrDouble::FromDouble(exact_latency_sec));
  AudioContext* exact_ok_context = AudioContext::Create(
      GetDocument(), exact_ok_options, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(exact_ok_context->baseLatency(), exact_latency_sec);

  AudioContextOptions* exact_too_big_options = AudioContextOptions::Create();
  exact_too_big_options->setLatencyHint(
      AudioContextLatencyCategoryOrDouble::FromDouble(
          playback_context->baseLatency() * 2));
  AudioContext* exact_too_big_context = AudioContext::Create(
      GetDocument(), exact_too_big_options, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(exact_too_big_context->baseLatency(),
            playback_context->baseLatency());
}

TEST_F(AudioContextTest, AudioContextAudibility_ServiceUnbind) {
  AudioContextOptions* options = AudioContextOptions::Create();
  AudioContext* audio_context =
      AudioContext::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);

  audio_context->set_was_audible_for_testing(true);
  ResetAudioContextManagerForAudioContext(audio_context);
  SetContextState(audio_context, AudioContext::AudioContextState::kSuspended);

  ScopedTestingPlatformSupport<TestingPlatformSupport> platform;
  platform->RunUntilIdle();
}

TEST_F(AudioContextTest, ExecutionContextPaused) {
  AudioContextOptions* options = AudioContextOptions::Create();
  AudioContext* audio_context =
      AudioContext::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);

  audio_context->set_was_audible_for_testing(true);
  EXPECT_FALSE(web_audio_device_paused_);
  GetDocument().SetLifecycleState(mojom::FrameLifecycleState::kFrozen);
  EXPECT_TRUE(web_audio_device_paused_);
  GetDocument().SetLifecycleState(mojom::FrameLifecycleState::kRunning);
  EXPECT_FALSE(web_audio_device_paused_);
}

}  // namespace blink
