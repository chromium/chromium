// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/voice_isolation/voice_isolation.h"

#include <algorithm>
#include <memory>
#include <numeric>

#include "base/test/gmock_expected_support.h"
#include "base/test/gtest_util.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/webrtc/voice_isolation/passthrough_voice_isolation.h"
#include "media/webrtc/voice_isolation/stft_voice_isolation.h"
#include "media/webrtc/voice_isolation/voice_isolation_component.h"
#include "media/webrtc/voice_isolation/voice_isolation_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"
#include "third_party/tflite/src/tensorflow/lite/schema/schema_generated.h"

namespace media {
namespace {

// Frame size and rate of the component inside VoiceIsolation: 20 ms at 48 kHz.
constexpr size_t kComponentFrameSize = 960;
constexpr size_t kComponentFramesPerSecond = 50;

// 48 kHz stereo params with a fixed buffer size. Only the component layout or
// `sample_rate` varies between tests.
std::unique_ptr<VoiceIsolation> CreateWithPassthroughComponent(
    size_t component_frame_size,
    size_t component_frames_per_second,
    int sample_rate = 48000) {
  AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR,
                         ChannelLayoutConfig::Stereo(), sample_rate,
                         sample_rate / 100);
  return VoiceIsolation::Create(
      std::make_unique<PassthroughVoiceIsolation>(component_frame_size,
                                                  component_frames_per_second),
      params);
}

}  // namespace

TEST(VoiceIsolationTest, ProcessAudioDownmixesAndUpmixes) {
  // Configure the audio parameters to the same internal parameters of
  // VoiceIsolation. In this case the ConvertingAudioFifo should not do
  // resampling, but it WILL do downmixing and upmixing.
  constexpr int kSampleRate = kComponentFrameSize * kComponentFramesPerSecond;
  AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR,
                         ChannelLayoutConfig::Stereo(), kSampleRate,
                         kComponentFrameSize);

  // Use a passthrough component so that the output only depends on the
  // downmixing and upmixing, not on the model.
  std::unique_ptr<VoiceIsolation> voice_isolation = VoiceIsolation::Create(
      std::make_unique<PassthroughVoiceIsolation>(
          /*frame_size=*/kComponentFrameSize,
          /*frames_per_second=*/kComponentFramesPerSecond),
      params);
  ASSERT_NE(voice_isolation, nullptr);

  // Use a 2-channel bus to match the AudioParameters.
  std::unique_ptr<AudioBus> input_bus =
      AudioBus::Create(2, kComponentFrameSize);
  std::unique_ptr<AudioBus> output_bus =
      AudioBus::Create(2, kComponentFrameSize);

  // Fill input bus with dummy data.
  std::fill(input_bus->channel(0).begin(), input_bus->channel(0).end(), 2.0f);
  std::fill(input_bus->channel(1).begin(), input_bus->channel(1).end(), 4.0f);

  // Clear output bus to verify changes.
  output_bus->Zero();

  // The external parameters match the internal ones and the passthrough
  // component has no STFT, so there is no delay and one call is enough.
  voice_isolation->ProcessAudio(*input_bus, *output_bus);

  // Expected results:
  // Downmixing stereo to mono uses 0.5 scale to avoid clipping full scale
  // stereo mixes. Mono channel = left * 0.5 + right * 0.5 = 2.0 * 0.5 + 4.0 *
  // 0.5 = 3.0. Upmixing mono to stereo simply copies the mono channel to both
  // left and right.
  for (size_t i = 0; i < kComponentFrameSize; ++i) {
    constexpr float expected = 3.0f;
    EXPECT_FLOAT_EQ(output_bus->channel(0)[i], expected);
    EXPECT_FLOAT_EQ(output_bus->channel(1)[i], expected);
  }
}

// TODO(barrerap): Enable once TfLiteVoiceIsolation stops advancing the model
// state when it computes the bias.
TEST(VoiceIsolationTest, DISABLED_VoiceIsolationCanAdaptToAudioParameters) {
  // External signal is 48kHz, 10ms frames.
  constexpr int kSampleRate = 48000;
  constexpr int kFrameSize = kSampleRate / 100;
  AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR,
                         ChannelLayoutConfig::Stereo(), kSampleRate,
                         kFrameSize);

  std::unique_ptr<tflite::FlatBufferModel> model =
      LoadVoiceIsolationTestModel();
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

  // The resamplers, buffers and the STFT introduce a delay of 5 frames. The
  // fifth frame holds the zero-padded start of the first STFT output, which
  // stays silent only if the model starts from its initial state.
  constexpr int kNumLatencyFrames = 5;
  for (int j = 0; j < kNumLatencyFrames; ++j) {
    voice_isolation->ProcessAudio(*input_bus, *output_bus);
    float output_energy = std::inner_product(
        output_bus->channel(0).begin(), output_bus->channel(0).end(),
        output_bus->channel(0).begin(), 0.0f);
    EXPECT_NEAR(output_energy, 0.0f, 1e-6);
  }

  // Run for twice the latency so that the output only holds processed audio,
  // which must reach the output.
  constexpr int kNumSteadyStateFrames = 2 * kNumLatencyFrames;
  for (int j = 0; j < kNumSteadyStateFrames; ++j) {
    voice_isolation->ProcessAudio(*input_bus, *output_bus);
  }
  float output_energy = std::inner_product(
      output_bus->channel(0).begin(), output_bus->channel(0).end(),
      output_bus->channel(0).begin(), 0.0f);
  EXPECT_GT(output_energy, 0.0f);
}

// TODO(crbug.com/568298417): Enable on UBSan once the tests are fixed.
#if defined(UNDEFINED_SANITIZER)
#define MAYBE_TwoStageCreationSucceedsAndProcessesAudio \
  DISABLED_TwoStageCreationSucceedsAndProcessesAudio
#else
#define MAYBE_TwoStageCreationSucceedsAndProcessesAudio \
  TwoStageCreationSucceedsAndProcessesAudio
#endif
TEST(VoiceIsolationTest, MAYBE_TwoStageCreationSucceedsAndProcessesAudio) {
  constexpr int kSampleRate = kComponentFrameSize * kComponentFramesPerSecond;
  AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR,
                         ChannelLayoutConfig::Stereo(), kSampleRate,
                         kComponentFrameSize);

  std::unique_ptr<tflite::FlatBufferModel> model =
      LoadVoiceIsolationTestModel();

  ASSERT_OK_AND_ASSIGN(auto component,
                       VoiceIsolation::CreateComponent(model.get()));

  std::unique_ptr<VoiceIsolation> voice_isolation =
      VoiceIsolation::Create(std::move(component), params);
  ASSERT_NE(voice_isolation, nullptr);

  // The single-stage factory is the reference for the two-stage one.
  std::unique_ptr<VoiceIsolation> reference =
      VoiceIsolation::Create(model.get(), params);
  ASSERT_NE(reference, nullptr);

  // Use a 2-channel bus to match the AudioParameters.
  std::unique_ptr<AudioBus> input_bus =
      AudioBus::Create(2, kComponentFrameSize);
  std::unique_ptr<AudioBus> output_bus =
      AudioBus::Create(2, kComponentFrameSize);
  std::unique_ptr<AudioBus> reference_bus =
      AudioBus::Create(2, kComponentFrameSize);

  std::fill(input_bus->channel(0).begin(), input_bus->channel(0).end(), 2.0f);
  std::fill(input_bus->channel(1).begin(), input_bus->channel(1).end(), 4.0f);
  output_bus->Zero();
  reference_bus->Zero();

  // Both creation paths must produce the same audio for the same input.
  constexpr int kNumFrames = 4;
  for (int frame = 0; frame < kNumFrames; ++frame) {
    voice_isolation->ProcessAudio(*input_bus, *output_bus);
    reference->ProcessAudio(*input_bus, *reference_bus);
    for (size_t i = 0; i < kComponentFrameSize; ++i) {
      EXPECT_FLOAT_EQ(output_bus->channel(0)[i], reference_bus->channel(0)[i]);
      EXPECT_FLOAT_EQ(output_bus->channel(1)[i], reference_bus->channel(1)[i]);
    }
  }

  // Processed audio must reach the output.
  const float output_energy = std::inner_product(
      output_bus->channel(0).begin(), output_bus->channel(0).end(),
      output_bus->channel(0).begin(), 0.0f);
  EXPECT_GT(output_energy, 0.0f);
}

TEST(VoiceIsolationTest, ClearBuffersPurgesStaleLookaheadAudio) {
  constexpr int kSampleRate = 48000;
  constexpr int kFrameSize = kSampleRate / 100;
  AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR,
                         ChannelLayoutConfig::Stereo(), kSampleRate,
                         kFrameSize);

  // Wrap a passthrough component in the real STFT, as VoiceIsolation does with
  // the model, so that the output only depends on the FIFOs and the STFT
  // history.
  std::unique_ptr<VoiceIsolation> voice_isolation = VoiceIsolation::Create(
      std::make_unique<StftVoiceIsolation>(
          std::make_unique<PassthroughVoiceIsolation>(
              /*frame_size=*/2 * kComponentFrameSize,
              /*frames_per_second=*/kComponentFramesPerSecond)),
      params);
  ASSERT_NE(voice_isolation, nullptr);

  std::unique_ptr<AudioBus> input_bus = AudioBus::Create(2, kFrameSize);
  std::unique_ptr<AudioBus> silence_bus = AudioBus::Create(2, kFrameSize);
  std::unique_ptr<AudioBus> output_bus = AudioBus::Create(2, kFrameSize);

  std::fill(input_bus->channel(0).begin(), input_bus->channel(0).end(), 1.0f);
  std::fill(input_bus->channel(1).begin(), input_bus->channel(1).end(), 1.0f);
  silence_bus->Zero();
  output_bus->Zero();

  // Feed 5 consecutive frames containing a constant signal so lookahead FIFOs
  // and STFT overlap-add buffers are fully populated and emitting non-zero
  // audio.
  constexpr int kNumActiveFrames = 5;
  for (int i = 0; i < kNumActiveFrames; ++i) {
    voice_isolation->ProcessAudio(*input_bus, *output_bus);
  }
  const float active_energy = std::inner_product(
      output_bus->channel(0).begin(), output_bus->channel(0).end(),
      output_bus->channel(0).begin(), 0.0f);
  EXPECT_GT(active_energy, 0.0f);

  // Clear all internal FIFOs and STFT history.
  voice_isolation->ClearBuffers();

  // Feed pure silence and verify that no stranded samples from before the
  // clear leak into the output across the entire priming window and beyond.
  constexpr int kNumSilenceFramesToVerify = 6;
  for (int i = 0; i < kNumSilenceFramesToVerify; ++i) {
    voice_isolation->ProcessAudio(*silence_bus, *output_bus);
    for (int sample = 0; sample < kFrameSize; ++sample) {
      EXPECT_EQ(output_bus->channel(0)[sample], 0.0f)
          << "Non-zero sample leaked at frame " << i << ", sample " << sample;
      EXPECT_EQ(output_bus->channel(1)[sample], 0.0f)
          << "Non-zero sample leaked at frame " << i << ", sample " << sample;
    }
  }
}

// TODO(barrerap): Enable once TfLiteVoiceIsolation::ClearBuffers() resets the
// model resource variables.
TEST(VoiceIsolationTest, DISABLED_ClearBuffersMatchesFreshInstance) {
  constexpr int kSampleRate = 48000;
  constexpr int kFrameSize = kSampleRate / 100;
  AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR,
                         ChannelLayoutConfig::Stereo(), kSampleRate,
                         kFrameSize);

  std::unique_ptr<tflite::FlatBufferModel> model =
      LoadVoiceIsolationTestModel();
  std::unique_ptr<VoiceIsolation> voice_isolation =
      VoiceIsolation::Create(model.get(), params);
  ASSERT_NE(voice_isolation, nullptr);

  std::unique_ptr<AudioBus> input_bus = AudioBus::Create(2, kFrameSize);
  std::unique_ptr<AudioBus> silence_bus = AudioBus::Create(2, kFrameSize);
  std::unique_ptr<AudioBus> output_bus = AudioBus::Create(2, kFrameSize);
  std::unique_ptr<AudioBus> reference_bus = AudioBus::Create(2, kFrameSize);

  std::fill(input_bus->channel(0).begin(), input_bus->channel(0).end(), 1.0f);
  std::fill(input_bus->channel(1).begin(), input_bus->channel(1).end(), 1.0f);
  silence_bus->Zero();
  output_bus->Zero();
  reference_bus->Zero();

  // Feed 5 consecutive frames containing a constant signal so lookahead FIFOs,
  // STFT overlap-add buffers and the model state are fully populated and
  // emitting non-zero audio.
  constexpr int kNumActiveFrames = 5;
  for (int i = 0; i < kNumActiveFrames; ++i) {
    voice_isolation->ProcessAudio(*input_bus, *output_bus);
  }
  const float active_energy = std::inner_product(
      output_bus->channel(0).begin(), output_bus->channel(0).end(),
      output_bus->channel(0).begin(), 0.0f);
  EXPECT_GT(active_energy, 0.0f);

  // Clear all internal FIFOs, STFT history, and model state.
  voice_isolation->ClearBuffers();

  // A freshly created instance is the reference: after clearing, no stranded
  // samples or model state from before the clear may change the output.
  std::unique_ptr<VoiceIsolation> reference =
      VoiceIsolation::Create(model.get(), params);
  ASSERT_NE(reference, nullptr);

  constexpr int kNumSilenceFramesToVerify = 6;
  for (int i = 0; i < kNumSilenceFramesToVerify; ++i) {
    voice_isolation->ProcessAudio(*silence_bus, *output_bus);
    reference->ProcessAudio(*silence_bus, *reference_bus);
    for (int sample = 0; sample < kFrameSize; ++sample) {
      EXPECT_FLOAT_EQ(output_bus->channel(0)[sample],
                      reference_bus->channel(0)[sample])
          << "Stale audio leaked at frame " << i << ", sample " << sample;
      EXPECT_FLOAT_EQ(output_bus->channel(1)[sample],
                      reference_bus->channel(1)[sample])
          << "Stale audio leaked at frame " << i << ", sample " << sample;
    }
  }
}

// Verifies that VoiceIsolation::CreateComponent propagates initialization
// errors from the underlying TfLiteVoiceIsolation implementation when provided
// an invalid model, and that VoiceIsolation::Create returns nullptr.
TEST(VoiceIsolationTest, CreateComponentFailsOnInvalidModel) {
  // Build an invalid model that passes FlatBuffer structural verification but
  // contains no execution subgraphs or tensors.
  FakeModel bogus = BuildBogusModel();
  ASSERT_NE(bogus.model, nullptr);

  // Calling CreateComponent must propagate the underlying interpreter failure.
  auto result = VoiceIsolation::CreateComponent(bogus.model.get());
  EXPECT_THAT(result,
              base::test::ErrorIs(
                  VoiceIsolationCreationResult::kInterpreterCreationFailed));

  // The wrapper VoiceIsolation::Create must gracefully return nullptr on error.
  constexpr int kSampleRate = 48000;
  constexpr int kFrameSize = 320;
  AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR,
                         ChannelLayoutConfig::Stereo(), kSampleRate,
                         kFrameSize);
  EXPECT_EQ(VoiceIsolation::Create(bogus.model.get(), params), nullptr);
}

TEST(VoiceIsolationDeathTest, ComponentWithDifferentFrameSizeIsRejected) {
  EXPECT_CHECK_DEATH(CreateWithPassthroughComponent(2 * kComponentFrameSize,
                                                    kComponentFramesPerSecond));
}

TEST(VoiceIsolationDeathTest, ComponentWithDifferentFrameRateIsRejected) {
  EXPECT_CHECK_DEATH(CreateWithPassthroughComponent(
      kComponentFrameSize, kComponentFramesPerSecond / 2));
}

TEST(VoiceIsolationDeathTest, Non48kHzStreamIsRejected) {
  EXPECT_CHECK_DEATH(CreateWithPassthroughComponent(
      kComponentFrameSize, kComponentFramesPerSecond, /*sample_rate=*/44100));
}

// Positive control: the same helper with valid inputs doesn't crash, so each
// death above comes from the mismatch under test.
TEST(VoiceIsolationTest, MatchingComponentAnd48kHzStreamIsAccepted) {
  EXPECT_NE(CreateWithPassthroughComponent(kComponentFrameSize,
                                           kComponentFramesPerSecond),
            nullptr);
}

}  // namespace media
