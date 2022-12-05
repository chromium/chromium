// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/audio/audio_destination.h"

#include <memory>
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

const blink::LocalFrameToken kFrameToken;

class MockWebAudioDevice : public WebAudioDevice {
 public:
  explicit MockWebAudioDevice(double sample_rate, int frames_per_buffer)
      : sample_rate_(sample_rate), frames_per_buffer_(frames_per_buffer) {}

  void Start() override {}
  void Stop() override {}
  void Pause() override {}
  void Resume() override {}
  double SampleRate() override { return sample_rate_; }
  int FramesPerBuffer() override { return frames_per_buffer_; }

 private:
  double sample_rate_;
  int frames_per_buffer_;
};

class TestPlatform : public TestingPlatformSupport {
 public:
  std::unique_ptr<WebAudioDevice> CreateAudioDevice(
      const WebAudioSinkDescriptor& sink_descriptor,
      unsigned number_of_output_channels,
      const WebAudioLatencyHint& latency_hint,
      WebAudioDevice::RenderCallback*) override {
    return std::make_unique<MockWebAudioDevice>(AudioHardwareSampleRate(),
                                                AudioHardwareBufferSize());
  }

  double AudioHardwareSampleRate() override { return 44100; }
  size_t AudioHardwareBufferSize() override { return 512; }
  unsigned AudioHardwareOutputChannels() override { return 2; }
};

class AudioCallback : public blink::AudioIOCallback {
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

void CountWASamplesProcessedForRate(absl::optional<float> sample_rate) {
  WebAudioLatencyHint latency_hint(WebAudioLatencyHint::kCategoryInteractive);
  AudioCallback callback;

  const int channel_count = Platform::Current()->AudioHardwareOutputChannels();
  const size_t request_frames = Platform::Current()->AudioHardwareBufferSize();

  // Assume the default audio device. (i.e. the empty string)
  WebAudioSinkDescriptor sink_descriptor(WebString::FromUTF8(""), kFrameToken);

  // TODO(https://crbug.com/988121) Replace 128 with the appropriate
  // AudioContextRenderSizeHintCategory.
  scoped_refptr<AudioDestination> destination = AudioDestination::Create(
      callback, sink_descriptor, channel_count, latency_hint, sample_rate, 128);
  destination->Start();

  Vector<float> channels[channel_count];
  WebVector<float*> dest_data(static_cast<size_t>(channel_count));
  for (int i = 0; i < channel_count; ++i) {
    channels[i].resize(request_frames);
    dest_data[i] = channels[i].data();
  }
  destination->Render(dest_data, request_frames, 0, 0);

  int exact_frames_required =
      std::ceil(request_frames * destination->SampleRate() /
                Platform::Current()->AudioHardwareSampleRate());
  int expected_frames_processed =
      std::ceil(exact_frames_required /
                static_cast<double>(destination->RenderQuantumFrames())) *
      destination->RenderQuantumFrames();

  EXPECT_EQ(expected_frames_processed, callback.frames_processed_);
}

TEST(AudioDestinationTest, ResamplingTest) {
  ScopedTestingPlatformSupport<TestPlatform> platform;

  CountWASamplesProcessedForRate(absl::optional<float>());
  CountWASamplesProcessedForRate(8000);
  CountWASamplesProcessedForRate(24000);
  CountWASamplesProcessedForRate(44100);
  CountWASamplesProcessedForRate(48000);
  CountWASamplesProcessedForRate(384000);
}

}  // namespace

}  // namespace blink
