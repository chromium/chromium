// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/audio/audio_destination.h"

#include <array>
#include <memory>

#include "base/test/scoped_feature_list.h"
#include "media/audio/audio_features.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_glitch_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_audio_device.h"
#include "third_party/blink/public/platform/web_audio_latency_hint.h"
#include "third_party/blink/public/platform/web_audio_sink_descriptor.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/renderer/platform/audio/audio_callback_metric_reporter.h"
#include "third_party/blink/renderer/platform/audio/audio_io_callback.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

namespace blink {

namespace {

using ::testing::_;

const LocalFrameToken kFrameToken;

constexpr float kDefaultHardwareSampleRate = 44100;
constexpr int kDefaultHardwareBufferSize = 512;
constexpr int kDefaultHardwareOutputChannelNumber = 2;
constexpr int kWebAudioRenderQuantum = 128;

int RoundUpToRenderQuantum(int requested_frames) {
  return std::ceil(requested_frames /
                   static_cast<double>(kWebAudioRenderQuantum)) *
         kWebAudioRenderQuantum;
}

class MockWebAudioDevice : public WebAudioDevice {
 public:
  explicit MockWebAudioDevice(double sample_rate, int frames_per_buffer)
      : sample_rate_(sample_rate), frames_per_buffer_(frames_per_buffer) {}

  MOCK_METHOD(void, Start, (), (override));
  MOCK_METHOD(void, Stop, (), (override));
  MOCK_METHOD(void, Pause, (), (override));
  MOCK_METHOD(void, Resume, (), (override));
  double SampleRate() override { return sample_rate_; }
  int FramesPerBuffer() override { return frames_per_buffer_; }
  int MaxChannelCount() override { return 2; }
  void SetDetectSilence(bool detect_silence) override {}
  media::OutputDeviceStatus MaybeCreateSinkAndGetStatus() override {
    // In this test, we assume the sink creation always succeeds.
    return media::OUTPUT_DEVICE_STATUS_OK;
  }

 private:
  double sample_rate_;
  int frames_per_buffer_;
};

class TestPlatform : public TestingPlatformSupport {
 public:
  TestPlatform() = default;
  ~TestPlatform() override = default;

  void CreateMockWebAudioDevice(float context_sample_rate, int buffer_size) {
    webaudio_device_ =
        std::make_unique<MockWebAudioDevice>(context_sample_rate, buffer_size);
  }

  std::unique_ptr<WebAudioDevice> CreateAudioDevice(
      const WebAudioSinkDescriptor& sink_descriptor,
      unsigned number_of_output_channels,
      const WebAudioLatencyHint& latency_hint,
      std::optional<float> context_sample_rate,
      media::AudioRendererSink::RenderCallback*) override {
    CHECK(webaudio_device_ != nullptr)
        << "Calling CreateAudioDevice (via AudioDestination::Create) multiple "
           "times in one test is not supported.";
    return std::move(webaudio_device_);
  }

  double AudioHardwareSampleRate() override {
    return kDefaultHardwareSampleRate;
  }
  size_t AudioHardwareBufferSize() override {
    return kDefaultHardwareBufferSize;
  }
  unsigned AudioHardwareOutputChannels() override {
    return kDefaultHardwareOutputChannelNumber;
  }

  const MockWebAudioDevice& web_audio_device() {
    CHECK(webaudio_device_ != nullptr)
        << "Finish setting up expectations before calling CreateAudioDevice "
           "(via AudioDestination::Create).";
    return *webaudio_device_;
  }

 private:
  std::unique_ptr<MockWebAudioDevice> webaudio_device_;
};

class AudioCallback : public AudioIOCallback {
 public:
  void Render(AudioBus* audio_bus,
              uint32_t frames_to_process,
              const AudioIOPosition&,
              const AudioCallbackMetric&,
              base::TimeDelta delay,
              const media::AudioGlitchInfo& glitch_info) override {
    audio_bus->Zero();
    frames_processed_ += frames_to_process;
    last_latency_ = delay;
    glitch_accumulator_.Add(glitch_info);
  }

  MOCK_METHOD(void, OnRenderError, (), (final));

  AudioCallback() = default;
  int frames_processed_ = 0;
  media::AudioGlitchInfo::Accumulator glitch_accumulator_;
  base::TimeDelta last_latency_;
};

class AudioDestinationTest
    : public ::testing::TestWithParam<std::optional<float>> {
 public:
  scoped_refptr<AudioDestination> CreateAudioDestination(
      std::optional<float> context_sample_rate,
      WebAudioLatencyHint latency_hint) {
    // Assume the default audio device. (i.e. the empty string)
    WebAudioSinkDescriptor sink_descriptor(WebString::FromUTF8(""),
                                           kFrameToken);
    const int channel_count =
        Platform::Current()->AudioHardwareOutputChannels();

    return AudioDestination::Create(callback_, sink_descriptor, channel_count,
                                    latency_hint, context_sample_rate,
                                    kWebAudioRenderQuantum);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  AudioCallback callback_;
};

// This test verifies that resampling occurs correctly when the AudioContext's
// sample rate differs from the hardware's sample rate. We explicitly disable
// kWebAudioRemoveAudioDestinationResampler to ensure the resampler can be
// created.
TEST_P(AudioDestinationTest, ResamplingTest) {
  feature_list_.InitAndDisableFeature(
      features::kWebAudioRemoveAudioDestinationResampler);
  ScopedTestingPlatformSupport<TestPlatform> platform;
  platform->CreateMockWebAudioDevice(kDefaultHardwareSampleRate,
                                     kDefaultHardwareBufferSize);
  EXPECT_CALL(platform->web_audio_device(), Start).Times(1);
  EXPECT_CALL(platform->web_audio_device(), Stop).Times(1);

  scoped_refptr<AudioDestination> audio_destination = CreateAudioDestination(
      GetParam(),
      WebAudioLatencyHint(WebAudioLatencyHint::kCategoryInteractive));

  const int requested_frames = audio_destination->FramesPerBuffer();

  audio_destination->Start();
  audio_destination->Render(
      base::TimeDelta::Min(), base::TimeTicks::Now(), {},
      media::AudioBus::Create(kDefaultHardwareOutputChannelNumber,
                              requested_frames)
          .get());

  int scaled_requested_frames = requested_frames;

  // Check if resampling was performed and calculate the expected frame count.
  if (audio_destination->SampleRate() !=
      Platform::Current()->AudioHardwareSampleRate()) {
    // Resampler should be created when sample rates differ.
    EXPECT_NE(audio_destination->GetResamplerForTesting(), nullptr);

    // Calculate the scaled frame count based on the sample rate difference.
    scaled_requested_frames =
        std::ceil(requested_frames * audio_destination->SampleRate() /
                  Platform::Current()->AudioHardwareSampleRate());

    // The internal resampler requires media::SincResampler::KernelSize() / 2
    // more frames to flush the output. See sinc_resampler.cc for details.
    scaled_requested_frames +=
        media::SincResampler::KernelSizeFromRequestFrames(requested_frames) / 2;
  } else {
    // No resampler should be created when sample rates are the same.
    EXPECT_EQ(audio_destination->GetResamplerForTesting(), nullptr);
  }

  // Verify that the number of frames processed matches the expected count,
  // rounded up to the render quantum.
  const int expected_processed_frames =
      RoundUpToRenderQuantum(scaled_requested_frames);
  EXPECT_EQ(expected_processed_frames, callback_.frames_processed_);
}

TEST_P(AudioDestinationTest, GlitchAndDelay) {
  feature_list_.InitAndDisableFeature(
      features::kWebAudioRemoveAudioDestinationResampler);
  ScopedTestingPlatformSupport<TestPlatform> platform;
  platform->CreateMockWebAudioDevice(kDefaultHardwareSampleRate,
                                     kDefaultHardwareBufferSize);

  EXPECT_CALL(platform->web_audio_device(), Start).Times(1);
  EXPECT_CALL(platform->web_audio_device(), Stop).Times(1);

  scoped_refptr<AudioDestination> audio_destination = CreateAudioDestination(
      GetParam(),
      WebAudioLatencyHint(WebAudioLatencyHint::kCategoryInteractive));
  const int requested_frames = audio_destination->FramesPerBuffer();

  const int kRenderCount = 3;

  auto glitches = std::to_array<media::AudioGlitchInfo>({
      {.duration = base::Milliseconds(120), .count = 3},
      {},
      {.duration = base::Milliseconds(20), .count = 1},
  });

  auto delays = std::to_array<base::TimeDelta>({
      base::Milliseconds(100),
      base::Milliseconds(90),
      base::Milliseconds(80),
  });

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
  // Desktop platforms bypass the priming delay in the output buffer.
  const int priming_frames = 0;
#else
  // When creating the AudioDestination, some silence is added to the fifo to
  // prevent an underrun on the first callback. This contributes a constant
  // delay.
  const int priming_frames = RoundUpToRenderQuantum(requested_frames);
#endif
  base::TimeDelta priming_delay = audio_utilities::FramesToTime(
      priming_frames, Platform::Current()->AudioHardwareSampleRate());

  auto audio_bus = media::AudioBus::Create(kDefaultHardwareOutputChannelNumber,
                                           requested_frames);

  audio_destination->Start();

  for (int i = 0; i < kRenderCount; ++i) {
    audio_destination->Render(delays[i], base::TimeTicks::Now(), glitches[i],
                              audio_bus.get());

    EXPECT_EQ(callback_.glitch_accumulator_.GetAndReset(), glitches[i]);

    if (audio_destination->SampleRate() !=
        Platform::Current()->AudioHardwareSampleRate()) {
      // Resampler kernel adds a bit of a delay.
      EXPECT_GE(callback_.last_latency_, delays[i] + priming_delay);
      EXPECT_LE(callback_.last_latency_,
                delays[i] + base::Milliseconds(1) + priming_delay);
    } else {
      EXPECT_EQ(callback_.last_latency_, delays[i] + priming_delay);
    }
  }

  audio_destination->Stop();
}

// This test verifies that the AudioDestination resampler is correctly removed
// when the kWebAudioRemoveAudioDestinationResampler feature is enabled
// and WebAudioBypassOutputBuffering is also enabled.
TEST_P(AudioDestinationTest, ResamplerIsRemoved) {
  feature_list_.InitAndEnableFeature(
      features::kWebAudioRemoveAudioDestinationResampler);

  // Ideally we should have two separate test for WebAudioBypassOutputBuffering
  // enabled and disabled. The difference here is the
  // `expected_processed_frames` will be different. In FIFO non-bypass case the
  // buffer is primed so expected processed frames will need to account for
  // that. But considering that `WebAudioBypassOutputBuffering` is soon to be
  // default, we only add the bypass buffer enabled test here.
  blink::WebRuntimeFeatures::EnableFeatureFromString(
      "WebAudioBypassOutputBuffering", true);

  float context_sample_rate = GetParam().value_or(kDefaultHardwareSampleRate);

  // Use a non-default buffer size (kFakeScaledSinkBufferSize) to confirm it's
  // correctly applied in AudioDestination. The specific value of
  // kFakeScaledSinkBufferSize is unimportant, as the scaling calculations are
  // verified in RendererWebAudioDeviceImplBufferSizeTest. This test
  // focuses on ensuring the resampler is not created, allowing us to use the
  // original request frames (kFakeScaledSinkBufferSize) when calculating
  // expected_processed_frames.
  constexpr int kFakeScaledSinkBufferSize = 399;

  ScopedTestingPlatformSupport<TestPlatform> platform;
  platform->CreateMockWebAudioDevice(context_sample_rate,
                                     kFakeScaledSinkBufferSize);
  EXPECT_CALL(platform->web_audio_device(), Start).Times(1);
  EXPECT_CALL(platform->web_audio_device(), Stop).Times(1);

  scoped_refptr<AudioDestination> audio_destination = CreateAudioDestination(
      GetParam(),
      WebAudioLatencyHint(WebAudioLatencyHint::kCategoryInteractive));

  EXPECT_EQ(nullptr, audio_destination->GetResamplerForTesting());

  audio_destination->Start();
  audio_destination->Render(
      base::TimeDelta::Min(), base::TimeTicks::Now(), {},
      media::AudioBus::Create(
          Platform::Current()->AudioHardwareOutputChannels(),
          kFakeScaledSinkBufferSize)
          .get());

  // Calculate the expected number of processed frames, rounded up to the render
  // quantum.
  int expected_processed_frames =
      RoundUpToRenderQuantum(audio_destination->FramesPerBuffer());
  EXPECT_EQ(callback_.frames_processed_, expected_processed_frames);
}

INSTANTIATE_TEST_SUITE_P(/* no label */,
                         AudioDestinationTest,
                         ::testing::Values(std::optional<float>(),
                                           8000,
                                           24000,
                                           44100,
                                           48000,
                                           384000));

TEST_F(AudioDestinationTest, NoUnderrunsWithOutputBufferBypass) {
  // Test that calling Render() after the destination is stopped will not
  // generate underruns when the destination is in output buffer bypass mode.
  blink::WebRuntimeFeatures::EnableFeatureFromString(
      "WebAudioBypassOutputBuffering", true);
  ScopedTestingPlatformSupport<TestPlatform> platform;
  platform->CreateMockWebAudioDevice(kDefaultHardwareSampleRate,
                                     kDefaultHardwareBufferSize);

  const std::optional<float> context_sample_rate = 44100;
  scoped_refptr<AudioDestination> audio_destination = CreateAudioDestination(
      context_sample_rate,
      WebAudioLatencyHint(WebAudioLatencyHint::kCategoryInteractive));

  auto audio_bus = media::AudioBus::Create(
      Platform::Current()->AudioHardwareOutputChannels(),
      audio_destination->FramesPerBuffer());

  audio_destination->Start();
  audio_destination->Render(base::Milliseconds(90), base::TimeTicks::Now(),
                            media::AudioGlitchInfo(), audio_bus.get());
  audio_destination->Stop();
  audio_destination->Render(base::Milliseconds(90), base::TimeTicks::Now(),
                            media::AudioGlitchInfo(), audio_bus.get());

  EXPECT_EQ((audio_destination->GetPushPullFIFOStateForTest()).overflow_count,
            unsigned{0});
  EXPECT_EQ((audio_destination->GetPushPullFIFOStateForTest()).underflow_count,
            unsigned{0});
}

}  // namespace

}  // namespace blink
