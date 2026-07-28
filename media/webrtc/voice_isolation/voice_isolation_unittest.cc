// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/voice_isolation/voice_isolation.h"

#include <memory>
#include <numeric>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {

std::unique_ptr<tflite::FlatBufferModel> GetTestModelBuffer() {
  base::FilePath source_root;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root));
  source_root = source_root.AppendASCII("media")
                    .AppendASCII("webrtc")
                    .AppendASCII("voice_isolation")
                    .AppendASCII("test_model_1_2_160_2.tflite");
  return tflite::FlatBufferModel::BuildFromFile(
      source_root.AsUTF8Unsafe().c_str());
}

}  // namespace

TEST(VoiceIsolationTest, ProcessAudioDownmixesAndUpmixes) {
  // Configure the audio parameters to the same internal parameters of
  // VoiceIsolation. In this case the ConvertingAudioFifo should not do
  // resampling, but it WILL do downmixing and upmixing.
  constexpr int kSampleRate = 16000;
  constexpr int kFrameSize = 320;
  AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR,
                         ChannelLayoutConfig::Stereo(), kSampleRate,
                         kFrameSize);

  std::unique_ptr<tflite::FlatBufferModel> model = GetTestModelBuffer();
  std::unique_ptr<VoiceIsolation> voice_isolation =
      VoiceIsolation::Create(model.get(), params);
  ASSERT_NE(voice_isolation, nullptr);

  // Use a 2-channel bus to match the AudioParameters.
  std::unique_ptr<AudioBus> input_bus = AudioBus::Create(2, kFrameSize);
  std::unique_ptr<AudioBus> output_bus = AudioBus::Create(2, kFrameSize);

  // Fill input bus with dummy data.
  std::fill(input_bus->channel(0).begin(), input_bus->channel(0).end(), 2.0f);
  std::fill(input_bus->channel(1).begin(), input_bus->channel(1).end(), 4.0f);

  // Clear output bus to verify changes.
  output_bus->Zero();

  // Skip the first ProcessAudio call to take into account the extra delay in
  // the internal STFT.
  voice_isolation->ProcessAudio(*input_bus, *output_bus);
  voice_isolation->ProcessAudio(*input_bus, *output_bus);

  // Expected results:
  // Downmixing stereo to mono uses 0.5 scale to avoid clipping full scale
  // stereo mixes. Mono channel = left * 0.5 + right * 0.5 = 2.0 * 0.5 + 4.0 *
  // 0.5 = 3.0. Upmixing mono to stereo simply copies the mono channel to both
  // left and right.
  for (int i = 0; i < kFrameSize; ++i) {
    constexpr float expected = 3.0f;
    EXPECT_FLOAT_EQ(output_bus->channel(0)[i], expected);
    EXPECT_FLOAT_EQ(output_bus->channel(1)[i], expected);
  }
}

TEST(VoiceIsolationTest, VoiceIsolationCanAdaptToAudioParameters) {
  // External signal is 48kHz, 10ms frames.
  constexpr int kSampleRate = 48000;
  constexpr int kFrameSize = kSampleRate / 100;
  AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR,
                         ChannelLayoutConfig::Stereo(), kSampleRate,
                         kFrameSize);

  std::unique_ptr<tflite::FlatBufferModel> model = GetTestModelBuffer();
  std::unique_ptr<VoiceIsolation> voice_isolation =
      VoiceIsolation::Create(model.get(), params);
  ASSERT_NE(voice_isolation, nullptr);

  // Use a 3-channel bus to ensure copying happens to all other channels.
  std::unique_ptr<AudioBus> input_bus = AudioBus::Create(2, kFrameSize);
  std::unique_ptr<AudioBus> output_bus = AudioBus::Create(2, kFrameSize);

  // Fill input bus with dummy data.
  std::fill(input_bus->channel(0).begin(), input_bus->channel(0).end(), 42.f);
  std::fill(input_bus->channel(1).begin(), input_bus->channel(1).end(), -1.0f);

  // Clear output bus to verify changes.
  output_bus->Zero();

  // The resamplers and buffers introduce a delay in voice_isolation of at least
  // 4 frames.
  for (int j = 0; j < 4; ++j) {
    voice_isolation->ProcessAudio(*input_bus, *output_bus);
    float output_energy = std::inner_product(
        output_bus->channel(0).begin(), output_bus->channel(0).end(),
        output_bus->channel(0).begin(), 0.0f);
    EXPECT_NEAR(output_energy, 0.0f, 1e-6);
  }

  voice_isolation->ProcessAudio(*input_bus, *output_bus);
  float output_energy = std::inner_product(
      output_bus->channel(0).begin(), output_bus->channel(0).end(),
      output_bus->channel(0).begin(), 0.0f);
  EXPECT_NEAR(output_energy, 0.0f, 1e-6);
}
}  // namespace media
