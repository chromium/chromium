// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/audio/audio_destination.h"

#include <memory>

#include "media/base/audio_glitch_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/web_audio_device.h"
#include "third_party/blink/public/platform/web_audio_latency_hint.h"
#include "third_party/blink/public/platform/web_audio_sink_descriptor.h"
#include "third_party/blink/renderer/platform/audio/audio_callback_metric_reporter.h"
#include "third_party/blink/renderer/platform/audio/audio_io_callback.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

namespace blink {

namespace {

using ::testing::_;
using ::testing::InSequence;

const LocalFrameToken kFrameToken;

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
  TestPlatform() {
    webaudio_device_ = std::make_unique<MockWebAudioDevice>(
        AudioHardwareSampleRate(), AudioHardwareBufferSize());
  }

  std::unique_ptr<WebAudioDevice> CreateAudioDevice(
      const WebAudioSinkDescriptor& sink_descriptor,
      unsigned number_of_output_channels,
      const WebAudioLatencyHint& latency_hint,
      media::AudioRendererSink::RenderCallback*) override {
    CHECK(webaudio_device_ != nullptr)
        << "Calling CreateAudioDevice (via AudioDestination::Create) multiple "
           "times in one test is not supported.";
    return std::move(webaudio_device_);
  }

  double AudioHardwareSampleRate() override { return 44100; }
  size_t AudioHardwareBufferSize() override { return 512; }
  unsigned AudioHardwareOutputChannels() override { return 2; }

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
  void Render(AudioBus*,
              uint32_t frames_to_process,
              const AudioIOPosition&,
              const AudioCallbackMetric&,
              base::TimeDelta delay,
              const media::AudioGlitchInfo& glitch_info) override {
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
  void CountWASamplesProcessedForRate(std::optional<float> sample_rate) {
    WebAudioLatencyHint latency_hint(WebAudioLatencyHint::kCategoryInteractive);

    const int channel_count =
        Platform::Current()->AudioHardwareOutputChannels();
    const size_t request_frames =
        Platform::Current()->AudioHardwareBufferSize();

    // Assume the default audio device. (i.e. the empty string)
    WebAudioSinkDescriptor sink_descriptor(WebString::FromUTF8(""),
                                           kFrameToken);

    // TODO(https://crbug.com/988121) Replace 128 with the appropriate
    // AudioContextRenderSizeHintCategory.
    constexpr int render_quantum_frames = 128;
    scoped_refptr<AudioDestination> destination = AudioDestination::Create(
        callback_, sink_descriptor, channel_count, latency_hint, sample_rate,
        render_quantum_frames);
    destination->Start();

    destination->Render(
        base::TimeDelta::Min(), base::TimeTicks::Now(), {},
        media::AudioBus::Create(channel_count, request_frames).get());

    // Calculate the expected number of frames to be consumed to produce
    // |request_frames| frames.
    int exact_frames_required = request_frames;
    if (destination->SampleRate() !=
        Platform::Current()->AudioHardwareSampleRate()) {
      exact_frames_required =
          std::ceil(request_frames * destination->SampleRate() /
                    Platform::Current()->AudioHardwareSampleRate());
      // The internal resampler requires media::SincResampler::KernelSize() / 2
      // more frames to flush the output. See sinc_resampler.cc for details.
      exact_frames_required +=
          media::SincResampler::KernelSizeFromRequestFrames(request_frames) / 2;
    }
    const int expected_frames_processed =
        std::ceil(exact_frames_required /
                  static_cast<double>(render_quantum_frames)) *
        render_quantum_frames;

    EXPECT_EQ(expected_frames_processed, callback_.frames_processed_);
  }

 protected:
  AudioCallback callback_;
};

TEST_P(AudioDestinationTest, ResamplingTest) {
#if defined(MEMORY_SANITIZER)
  // TODO(crbug.com/342415791): Fix and re-enable tests with MSan.
  GTEST_SKIP();
#else
  ScopedTestingPlatformSupport<TestPlatform> platform;
  {
    InSequence s;

    EXPECT_CALL(platform->web_audio_device(), Start).Times(1);
    EXPECT_CALL(platform->web_audio_device(), Stop).Times(1);
  }

  CountWASamplesProcessedForRate(GetParam());
#endif
}

TEST_P(AudioDestinationTest, GlitchAndDelay) {
#if defined(MEMORY_SANITIZER)
  // TODO(crbug.com/342415791): Fix and re-enable tests with MSan.
  GTEST_SKIP();
#else
  ScopedTestingPlatformSupport<TestPlatform> platform;
  {
    InSequence s;
    EXPECT_CALL(platform->web_audio_device(), Start).Times(1);
    EXPECT_CALL(platform->web_audio_device(), Stop).Times(1);
  }

  std::optional<float> sample_rate = GetParam();
  WebAudioLatencyHint latency_hint(WebAudioLatencyHint::kCategoryInteractive);

  const int channel_count = Platform::Current()->AudioHardwareOutputChannels();
  const size_t request_frames = Platform::Current()->AudioHardwareBufferSize();

  // Assume the default audio device. (i.e. the empty string)
  WebAudioSinkDescriptor sink_descriptor(WebString::FromUTF8(""), kFrameToken);

  int render_quantum_frames = 128;
  scoped_refptr<AudioDestination> destination = AudioDestination::Create(
      callback_, sink_descriptor, channel_count, latency_hint, sample_rate,
      render_quantum_frames);

  const int kRenderCount = 3;

  media::AudioGlitchInfo glitches[]{
      {.duration = base::Milliseconds(120), .count = 3},
      {},
      {.duration = base::Milliseconds(20), .count = 1}};

  base::TimeDelta delays[]{base::Milliseconds(100), base::Milliseconds(90),
                           base::Milliseconds(80)};

  // When creating the AudioDestination, some silence is added to the fifo to
  // prevent an underrun on the first callback. This contributes a constant
  // delay.
  int priming_frames =
      ceil(request_frames / static_cast<float>(render_quantum_frames)) *
      render_quantum_frames;
  base::TimeDelta priming_delay = audio_utilities::FramesToTime(
      priming_frames, Platform::Current()->AudioHardwareSampleRate());

  auto audio_bus = media::AudioBus::Create(channel_count, request_frames);

  destination->Start();

  for (int i = 0; i < kRenderCount; ++i) {
    destination->Render(delays[i], base::TimeTicks::Now(), glitches[i],
                        audio_bus.get());

    EXPECT_EQ(callback_.glitch_accumulator_.GetAndReset(), glitches[i]);

    if (destination->SampleRate() !=
        Platform::Current()->AudioHardwareSampleRate()) {
      // Resampler kernel adds a bit of a delay.
      EXPECT_GE(callback_.last_latency_, delays[i] + priming_delay);
      EXPECT_LE(callback_.last_latency_,
                delays[i] + base::Milliseconds(1) + priming_delay);
    } else {
      EXPECT_EQ(callback_.last_latency_, delays[i] + priming_delay);
    }
  }

  destination->Stop();
#endif
}

INSTANTIATE_TEST_SUITE_P(/* no label */,
                         AudioDestinationTest,
                         ::testing::Values(std::optional<float>(),
                                           8000,
                                           24000,
                                           44100,
                                           48000,
                                           384000));

}  // namespace

}  // namespace blink
