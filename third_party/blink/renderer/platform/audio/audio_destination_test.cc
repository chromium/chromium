// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/audio/audio_destination.h"

#include <memory>

#include "media/base/audio_glitch_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
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
  media::OutputDeviceStatus CreateSinkAndGetDeviceStatus() override {
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
              const AudioCallbackMetric&) override {
    frames_processed_ += frames_to_process;
  }

  AudioCallback() = default;
  int frames_processed_ = 0;
};

class AudioDestinationTest
    : public ::testing::TestWithParam<absl::optional<float>> {
 public:
  void CountWASamplesProcessedForRate(absl::optional<float> sample_rate) {
    WebAudioLatencyHint latency_hint(WebAudioLatencyHint::kCategoryInteractive);
    AudioCallback callback;

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
        callback, sink_descriptor, channel_count, latency_hint, sample_rate,
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
                  static_cast<double>(destination->RenderQuantumFrames())) *
        destination->RenderQuantumFrames();

    EXPECT_EQ(expected_frames_processed, callback.frames_processed_);
  }
};

TEST_P(AudioDestinationTest, ResamplingTest) {
  ScopedTestingPlatformSupport<TestPlatform> platform;
  {
    InSequence s;

    EXPECT_CALL(platform->web_audio_device(), Start).Times(1);
    EXPECT_CALL(platform->web_audio_device(), Stop).Times(1);
  }

  CountWASamplesProcessedForRate(GetParam());
}

INSTANTIATE_TEST_SUITE_P(/* no label */,
                         AudioDestinationTest,
                         ::testing::Values(absl::optional<float>(),
                                           8000,
                                           24000,
                                           44100,
                                           48000,
                                           384000));

}  // namespace

}  // namespace blink
