// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/voice_isolation/voice_isolation.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <numbers>
#include <numeric>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/gtest_util.h"
#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/channel_layout.h"
#include "media/base/channel_mixer.h"
#include "media/webrtc/voice_isolation/buffered_voice_isolation.h"
#include "media/webrtc/voice_isolation/mock_voice_isolation.h"
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

// Frame size and rate of the component inside VoiceIsolation: 10 ms at 48 kHz,
// as delivered by APM.
constexpr size_t kComponentFrameSize = VoiceIsolation::kFrameSize;
constexpr size_t kComponentFramesPerSecond =
    VoiceIsolation::kSampleRate / VoiceIsolation::kFrameSize;
constexpr int kSampleRateHz = VoiceIsolation::kSampleRate;

// Constant stereo input levels. Downmixing stereo to mono uses a 0.5 gain per
// channel to avoid clipping full scale stereo mixes, and upmixing mono to
// stereo copies the mono channel to both left and right.
constexpr float kLeftLevel = 2.0f;
constexpr float kRightLevel = 4.0f;
constexpr float kMixedLevel = (kLeftLevel + kRightLevel) / 2;

// Low-band passband gain of `BuildStatelessTestModel()` through
// `TfLiteVoiceIsolation`, which mixes 0.9 of the dereverberated output with
// 0.1 of the denoising-only output.
constexpr float kDereverbMixWeight = 0.9f;
constexpr float kStatelessModelLowBandGain =
    kDereverbMixWeight * kStatelessModelDereverbGain +
    (1.0f - kDereverbMixWeight) * kStatelessModelDenoisingOnlyGain;

// 48 kHz stereo params with a fixed buffer size. Only the component layout or
// `sample_rate` varies between tests.
std::unique_ptr<VoiceIsolation> CreateWithPassthroughComponent(
    size_t component_frame_size,
    size_t component_frames_per_second,
    int sample_rate = kSampleRateHz) {
  AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR,
                         ChannelLayoutConfig::Stereo(), sample_rate,
                         kComponentFrameSize);
  return VoiceIsolation::Create(
      std::make_unique<PassthroughVoiceIsolation>(component_frame_size,
                                                  component_frames_per_second),
      params);
}

// Returns a VoiceIsolation around a zero-latency passthrough component, which
// isolates the channel mixing done by VoiceIsolation itself.
std::unique_ptr<VoiceIsolation> CreatePassthroughVoiceIsolation(
    const ChannelLayoutConfig& layout) {
  AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR, layout,
                         kSampleRateHz, kComponentFrameSize);
  return VoiceIsolation::Create(
      std::make_unique<PassthroughVoiceIsolation>(kComponentFrameSize,
                                                  kComponentFramesPerSecond),
      params);
}

// Returns a mock component with the 10 ms frame configuration. NiceMock
// silences the getter calls.
std::unique_ptr<testing::NiceMock<MockVoiceIsolationComponent>>
CreateMockComponent() {
  std::unique_ptr<testing::NiceMock<MockVoiceIsolationComponent>>
      mock_component =
          std::make_unique<testing::NiceMock<MockVoiceIsolationComponent>>();
  ON_CALL(*mock_component, FrameSize())
      .WillByDefault(testing::Return(kComponentFrameSize));
  ON_CALL(*mock_component, FramesPerSecond())
      .WillByDefault(testing::Return(kComponentFramesPerSecond));
  return mock_component;
}

// Returns a signal of `total_samples` samples containing a Hann^2-windowed
// cosine pulse of length `pulse_length` and frequency `carrier_frequency_hz`
// starting at `pulse_start_index` (peaking at `pulse_start_index +
// pulse_length / 2`).
std::vector<float> MakeTonePulse(size_t total_samples,
                                 size_t pulse_start_index,
                                 size_t pulse_length,
                                 float carrier_frequency_hz) {
  CHECK_LE(pulse_start_index + pulse_length, total_samples);
  std::vector<float> signal(total_samples, 0.0f);
  for (size_t i = 0; i < pulse_length; ++i) {
    const float sine =
        std::sin(std::numbers::pi_v<float> * static_cast<float>(i) /
                 static_cast<float>(pulse_length));
    const float hann = sine * sine;
    const float envelope = hann * hann;
    const float phase =
        2.0f * std::numbers::pi_v<float> * carrier_frequency_hz *
        (static_cast<float>(i) - static_cast<float>(pulse_length / 2)) /
        static_cast<float>(kSampleRateHz);
    signal[pulse_start_index + i] = envelope * std::cos(phase);
  }
  return signal;
}

// Streams a mono `input` signal (whose length must be a multiple of
// `kComponentFrameSize`) block by block through `voice_isolation` and returns
// the concatenated mono output.
std::vector<float> ProcessInBlocks(VoiceIsolation& voice_isolation,
                                   base::span<const float> input) {
  CHECK_EQ(input.size() % kComponentFrameSize, 0u);
  const size_t num_blocks = input.size() / kComponentFrameSize;
  std::unique_ptr<AudioBus> input_bus =
      AudioBus::Create(1, kComponentFrameSize);
  std::unique_ptr<AudioBus> output_bus =
      AudioBus::Create(1, kComponentFrameSize);
  std::vector<float> output(input.size(), 0.0f);

  for (size_t block = 0; block < num_blocks; ++block) {
    const size_t offset = block * kComponentFrameSize;
    input_bus->channel(0).copy_from_nonoverlapping(
        input.subspan(offset, kComponentFrameSize));

    voice_isolation.ProcessAudio(*input_bus, *output_bus);

    base::span(output)
        .subspan(offset, kComponentFrameSize)
        .copy_from_nonoverlapping(output_bus->channel(0));
  }
  return output;
}

}  // namespace

TEST(VoiceIsolationTest, CreateComponentProcesses10MsFrames) {
  std::unique_ptr<tflite::FlatBufferModel> model =
      LoadVoiceIsolationTestModel();

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<VoiceIsolationComponent> component,
                       VoiceIsolation::CreateComponent(model.get()));

  ASSERT_TRUE(component);
  EXPECT_EQ(component->FrameSize(), kComponentFrameSize);
  EXPECT_EQ(component->FramesPerSecond(), kComponentFramesPerSecond);

  // The STFT overlap-add delays the output by 10 ms, and the buffering by one
  // more 10 ms frame. The model itself adds no delay.
  constexpr base::TimeDelta kStftDelay = base::Milliseconds(10);
  constexpr base::TimeDelta kBufferingDelay = base::Milliseconds(10);
  EXPECT_EQ(component->AlgorithmicDelay(), kStftDelay + kBufferingDelay);
}

TEST(VoiceIsolationTest, VoiceIsolationCanAdaptToAudioParameters) {
  AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR,
                         ChannelLayoutConfig::Stereo(), kSampleRateHz,
                         kComponentFrameSize);

  std::unique_ptr<tflite::FlatBufferModel> model =
      LoadVoiceIsolationTestModel();
  std::unique_ptr<VoiceIsolation> voice_isolation =
      VoiceIsolation::Create(model.get(), params);
  ASSERT_NE(voice_isolation, nullptr);

  // Use a stereo bus matching the AudioParameters.
  std::unique_ptr<AudioBus> input_bus =
      AudioBus::Create(2, kComponentFrameSize);
  std::unique_ptr<AudioBus> output_bus =
      AudioBus::Create(2, kComponentFrameSize);

  // Fill the input bus with a Hann-windowed 1 kHz stereo tone so that the
  // signal lies inside the 50..7500 Hz model passband. A step or DC input
  // would straddle the bypassed DC bin (gain 1.0) and the model passband
  // (gain 1.1 from the 0.9 / 0.1 mix of the stateful test model's outputs),
  // dispersing energy into the zero-padded start of the first STFT window.
  constexpr float kToneFrequencyHz = 1000.0f;
  constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;
  for (size_t i = 0; i < kComponentFrameSize; ++i) {
    const float window_sine =
        std::sin(std::numbers::pi_v<float> * static_cast<float>(i) /
                 static_cast<float>(kComponentFrameSize));
    const float window = window_sine * window_sine;
    const float time_s = static_cast<float>(i) / kSampleRateHz;
    const float sample = window * std::sin(kTwoPi * kToneFrequencyHz * time_s);
    input_bus->channel(0)[i] = kLeftLevel * sample;
    input_bus->channel(1)[i] = kRightLevel * sample;
  }

  // Clear output bus to verify changes.
  output_bus->Zero();

  // BufferedVoiceIsolation and the STFT introduce a delay of two 10 ms frames.
  // The second frame holds the zero-padded start of the first STFT output,
  // which stays silent only if the model starts from its initial state.
  constexpr int kNumLatencyFrames = 2;
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

TEST(VoiceIsolationTest, TwoStageCreationSucceedsAndProcessesAudio) {
  AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR,
                         ChannelLayoutConfig::Stereo(), kSampleRateHz,
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

  std::fill(input_bus->channel(0).begin(), input_bus->channel(0).end(),
            kLeftLevel);
  std::fill(input_bus->channel(1).begin(), input_bus->channel(1).end(),
            kRightLevel);
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

// After bias subtraction, the stateless test model scales the dereverberated
// output by `kStatelessModelDereverbGain` and the denoising-only output by
// `kStatelessModelDenoisingOnlyGain`. The low band (including the 7 kHz tone)
// is scaled by `0.9 * kStatelessModelDereverbGain + 0.1 *
// kStatelessModelDenoisingOnlyGain`, while the speech mask at 7 kHz scales the
// 16 kHz high-band tone by `kStatelessModelDenoisingOnlyGain`.
TEST(VoiceIsolationTest, KeepsHighBandWithStatelessModel) {
  constexpr int kSampleRate = kComponentFrameSize * kComponentFramesPerSecond;
  AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR,
                         ChannelLayoutConfig::Stereo(), kSampleRate,
                         kComponentFrameSize);

  FakeModel fake_model = BuildStatelessTestModel();
  std::unique_ptr<VoiceIsolation> voice_isolation =
      VoiceIsolation::Create(fake_model.model.get(), params);
  ASSERT_NE(voice_isolation, nullptr);

  // Fill both channels with a 7 kHz tone, which sets the high band gain, plus
  // a 16 kHz tone in the high band. Both tones repeat every frame, so the
  // energy of an output frame does not depend on the processing delay.
  constexpr float kGainToneHz = 7000.0f;
  constexpr float kHighBandToneHz = 16000.0f;
  constexpr float kToneAmplitude = 0.25f;
  constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;
  std::unique_ptr<AudioBus> input_bus =
      AudioBus::Create(2, kComponentFrameSize);
  std::unique_ptr<AudioBus> output_bus =
      AudioBus::Create(2, kComponentFrameSize);
  for (size_t i = 0; i < kComponentFrameSize; ++i) {
    const float time_s = static_cast<float>(i) / kSampleRate;
    const float sample =
        kToneAmplitude * (std::sin(kTwoPi * kGainToneHz * time_s) +
                          std::sin(kTwoPi * kHighBandToneHz * time_s));
    input_bus->channel(0)[i] = sample;
    input_bus->channel(1)[i] = sample;
  }
  const float input_energy = std::inner_product(
      input_bus->channel(0).begin(), input_bus->channel(0).end(),
      input_bus->channel(0).begin(), 0.0f);

  // Let the FIFOs, the STFT and the speech mask reach steady state.
  constexpr int kNumWarmUpFrames = 5;
  for (int frame = 0; frame < kNumWarmUpFrames; ++frame) {
    voice_isolation->ProcessAudio(*input_bus, *output_bus);
  }

  // The two equal-amplitude tones are orthogonal, so the output-to-input
  // energy ratio is the average of their squared gains. Without the high band,
  // it would be only `kStatelessModelLowBandGain * kStatelessModelLowBandGain /
  // 2.0f` (~0.138 instead of ~0.419).
  constexpr float kHighBandGain = kStatelessModelDenoisingOnlyGain;
  constexpr float kExpectedEnergyRatio =
      (kStatelessModelLowBandGain * kStatelessModelLowBandGain +
       kHighBandGain * kHighBandGain) /
      2.0f;
  constexpr int kNumCheckedFrames = 5;
  constexpr float kEnergyRatioTolerance = 0.05f;
  for (int frame = 0; frame < kNumCheckedFrames; ++frame) {
    voice_isolation->ProcessAudio(*input_bus, *output_bus);
    const float output_energy = std::inner_product(
        output_bus->channel(0).begin(), output_bus->channel(0).end(),
        output_bus->channel(0).begin(), 0.0f);
    EXPECT_NEAR(output_energy / input_energy, kExpectedEnergyRatio,
                kEnergyRatioTolerance)
        << "Frame " << frame;
  }
}

TEST(VoiceIsolationTest, ClearBuffersPurgesStaleLookaheadAudio) {
  AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR,
                         ChannelLayoutConfig::Stereo(), kSampleRateHz,
                         kComponentFrameSize);

  // Wrap a passthrough component in the real STFT and BufferedVoiceIsolation,
  // as VoiceIsolation does with the model, so that the output only depends on
  // BufferedVoiceIsolation and the STFT history.
  constexpr size_t kStftFrameSize = 2 * kComponentFrameSize;
  constexpr size_t kStftFramesPerSecond = kComponentFramesPerSecond / 2;
  std::unique_ptr<VoiceIsolation> voice_isolation = VoiceIsolation::Create(
      std::make_unique<BufferedVoiceIsolation>(
          std::make_unique<StftVoiceIsolation>(
              std::make_unique<PassthroughVoiceIsolation>(
                  /*frame_size=*/2 * kStftFrameSize,
                  /*frames_per_second=*/kStftFramesPerSecond))),
      params);
  ASSERT_NE(voice_isolation, nullptr);

  std::unique_ptr<AudioBus> input_bus =
      AudioBus::Create(2, kComponentFrameSize);
  std::unique_ptr<AudioBus> silence_bus =
      AudioBus::Create(2, kComponentFrameSize);
  std::unique_ptr<AudioBus> output_bus =
      AudioBus::Create(2, kComponentFrameSize);

  std::fill(input_bus->channel(0).begin(), input_bus->channel(0).end(), 1.0f);
  std::fill(input_bus->channel(1).begin(), input_bus->channel(1).end(), 1.0f);
  silence_bus->Zero();
  output_bus->Zero();

  // Feed 5 consecutive frames containing a constant signal so the
  // BufferedVoiceIsolation and STFT overlap-add buffers are fully populated
  // and emitting non-zero audio.
  constexpr int kNumActiveFrames = 5;
  for (int i = 0; i < kNumActiveFrames; ++i) {
    voice_isolation->ProcessAudio(*input_bus, *output_bus);
  }
  const float active_energy = std::inner_product(
      output_bus->channel(0).begin(), output_bus->channel(0).end(),
      output_bus->channel(0).begin(), 0.0f);
  EXPECT_GT(active_energy, 0.0f);

  // Clear the buffering and STFT history.
  voice_isolation->ClearBuffers();

  // Feed pure silence and verify that no stranded samples from before the
  // clear leak into the output across the entire priming window and beyond.
  constexpr int kNumSilenceFramesToVerify = 6;
  for (int i = 0; i < kNumSilenceFramesToVerify; ++i) {
    voice_isolation->ProcessAudio(*silence_bus, *output_bus);
    for (size_t sample = 0; sample < kComponentFrameSize; ++sample) {
      EXPECT_EQ(output_bus->channel(0)[sample], 0.0f)
          << "Non-zero sample leaked at frame " << i << ", sample " << sample;
      EXPECT_EQ(output_bus->channel(1)[sample], 0.0f)
          << "Non-zero sample leaked at frame " << i << ", sample " << sample;
    }
  }
}

TEST(VoiceIsolationTest, ClearBuffersMatchesFreshInstance) {
  AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR,
                         ChannelLayoutConfig::Stereo(), kSampleRateHz,
                         kComponentFrameSize);

  std::unique_ptr<tflite::FlatBufferModel> model =
      LoadVoiceIsolationTestModel();
  std::unique_ptr<VoiceIsolation> voice_isolation =
      VoiceIsolation::Create(model.get(), params);
  ASSERT_NE(voice_isolation, nullptr);

  std::unique_ptr<AudioBus> input_bus =
      AudioBus::Create(2, kComponentFrameSize);
  std::unique_ptr<AudioBus> silence_bus =
      AudioBus::Create(2, kComponentFrameSize);
  std::unique_ptr<AudioBus> output_bus =
      AudioBus::Create(2, kComponentFrameSize);
  std::unique_ptr<AudioBus> reference_bus =
      AudioBus::Create(2, kComponentFrameSize);

  std::fill(input_bus->channel(0).begin(), input_bus->channel(0).end(), 1.0f);
  std::fill(input_bus->channel(1).begin(), input_bus->channel(1).end(), 1.0f);
  silence_bus->Zero();
  output_bus->Zero();
  reference_bus->Zero();

  // Feed 5 consecutive frames containing a constant signal so the
  // BufferedVoiceIsolation, STFT overlap-add buffers and the model state are
  // fully populated and emitting non-zero audio.
  constexpr int kNumActiveFrames = 5;
  for (int i = 0; i < kNumActiveFrames; ++i) {
    voice_isolation->ProcessAudio(*input_bus, *output_bus);
  }
  const float active_energy = std::inner_product(
      output_bus->channel(0).begin(), output_bus->channel(0).end(),
      output_bus->channel(0).begin(), 0.0f);
  EXPECT_GT(active_energy, 0.0f);

  // Clear the buffering, STFT history, and model state.
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
    for (size_t sample = 0; sample < kComponentFrameSize; ++sample) {
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
  AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR,
                         ChannelLayoutConfig::Stereo(), kSampleRateHz,
                         kComponentFrameSize);
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

TEST(VoiceIsolationTest, SupportsOnlyValid48kHzParametersWith10MsBuffers) {
  const AudioParameters supported(AudioParameters::AUDIO_PCM_LINEAR,
                                  ChannelLayoutConfig::Stereo(), kSampleRateHz,
                                  kComponentFrameSize);
  EXPECT_TRUE(VoiceIsolation::SupportsAudioParameters(supported));

  // Each case below breaks only one requirement of SupportsAudioParameters().
  // Wrong sample rate (still valid, still 480 frames per buffer).
  const AudioParameters non_48khz(
      AudioParameters::AUDIO_PCM_LINEAR, ChannelLayoutConfig::Stereo(),
      AudioParameters::kAudioCDSampleRate, kComponentFrameSize);
  EXPECT_FALSE(VoiceIsolation::SupportsAudioParameters(non_48khz));

  // Wrong buffer size (still valid, still 48 kHz).
  const AudioParameters twenty_ms_buffers(
      AudioParameters::AUDIO_PCM_LINEAR, ChannelLayoutConfig::Stereo(),
      kSampleRateHz, /*frames_per_buffer=*/2 * kComponentFrameSize);
  EXPECT_FALSE(VoiceIsolation::SupportsAudioParameters(twenty_ms_buffers));

  // Invalid parameters (CHANNEL_LAYOUT_NONE has 0 channels, but 48 kHz / 480).
  const AudioParameters no_channels(AudioParameters::AUDIO_PCM_LINEAR,
                                    ChannelLayoutConfig(), kSampleRateHz,
                                    kComponentFrameSize);
  EXPECT_FALSE(VoiceIsolation::SupportsAudioParameters(no_channels));
}

TEST(VoiceIsolationTest, StereoIsDownmixedAndUpmixedWithoutAddedLatency) {
  std::unique_ptr<VoiceIsolation> voice_isolation =
      CreatePassthroughVoiceIsolation(ChannelLayoutConfig::Stereo());
  std::unique_ptr<AudioBus> input_bus =
      AudioBus::Create(2, kComponentFrameSize);
  std::unique_ptr<AudioBus> output_bus =
      AudioBus::Create(2, kComponentFrameSize);
  std::fill(input_bus->channel(0).begin(), input_bus->channel(0).end(),
            kLeftLevel);
  std::fill(input_bus->channel(1).begin(), input_bus->channel(1).end(),
            kRightLevel);

  voice_isolation->ProcessAudio(*input_bus, *output_bus);

  // Both channels hold the downmixed level on the very first call: the mixing
  // adds no latency on top of the component. The levels are exactly
  // representable, so the mix is bit-exact.
  for (size_t i = 0; i < kComponentFrameSize; ++i) {
    EXPECT_EQ(output_bus->channel(0)[i], kMixedLevel);
    EXPECT_EQ(output_bus->channel(1)[i], kMixedLevel);
  }
}

TEST(VoiceIsolationTest, SurroundIsMixedLikeChannelMixerRoundTrip) {
  const ChannelLayoutConfig surround_layout =
      ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_5_1>();
  const int channels = surround_layout.channels();
  std::unique_ptr<VoiceIsolation> voice_isolation =
      CreatePassthroughVoiceIsolation(surround_layout);
  std::unique_ptr<AudioBus> input_bus =
      AudioBus::Create(channels, kComponentFrameSize);
  std::unique_ptr<AudioBus> output_bus =
      AudioBus::Create(channels, kComponentFrameSize);

  // Give every input channel a distinct level so misrouted channels show up.
  for (int ch = 0; ch < channels; ++ch) {
    std::fill(input_bus->channel(ch).begin(), input_bus->channel(ch).end(),
              static_cast<float>(ch + 1));
  }

  // Compute the reference by downmixing to mono and upmixing back with
  // ChannelMixer directly.
  ChannelMixer downmixer(surround_layout, ChannelLayoutConfig::Mono());
  ChannelMixer upmixer(ChannelLayoutConfig::Mono(), surround_layout);
  std::unique_ptr<AudioBus> mono_bus = AudioBus::Create(1, kComponentFrameSize);
  std::unique_ptr<AudioBus> expected_bus =
      AudioBus::Create(channels, kComponentFrameSize);
  downmixer.Transform(input_bus.get(), mono_bus.get());
  upmixer.Transform(mono_bus.get(), expected_bus.get());

  voice_isolation->ProcessAudio(*input_bus, *output_bus);

  for (int ch = 0; ch < channels; ++ch) {
    EXPECT_THAT(output_bus->channel(ch),
                testing::ElementsAreArray(expected_bus->channel(ch)))
        << "Mismatch at channel " << ch;
  }
}

TEST(VoiceIsolationTest, MonoIsProcessedWithoutMixing) {
  std::unique_ptr<VoiceIsolation> voice_isolation =
      CreatePassthroughVoiceIsolation(ChannelLayoutConfig::Mono());
  std::unique_ptr<AudioBus> input_bus =
      AudioBus::Create(1, kComponentFrameSize);
  std::unique_ptr<AudioBus> output_bus =
      AudioBus::Create(1, kComponentFrameSize);
  std::iota(input_bus->channel(0).begin(), input_bus->channel(0).end(), 0.0f);

  voice_isolation->ProcessAudio(*input_bus, *output_bus);

  // The passthrough output is bit-exact with the input on the first call.
  EXPECT_THAT(output_bus->channel(0),
              testing::ElementsAreArray(input_bus->channel(0)));
}

TEST(VoiceIsolationTest, ClearBuffersForwardsToComponent) {
  std::unique_ptr<testing::NiceMock<MockVoiceIsolationComponent>>
      mock_component = CreateMockComponent();
  EXPECT_CALL(*mock_component, ClearBuffers()).Times(1);
  AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR,
                         ChannelLayoutConfig::Stereo(), kSampleRateHz,
                         kComponentFrameSize);
  std::unique_ptr<VoiceIsolation> voice_isolation =
      VoiceIsolation::Create(std::move(mock_component), params);
  ASSERT_TRUE(voice_isolation);

  voice_isolation->ClearBuffers();
}

TEST(VoiceIsolationDeathTest, CreateDiesOnNon10MsBuffers) {
  constexpr int kTwentyMsFrameSize = 2 * kComponentFrameSize;
  AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR,
                         ChannelLayoutConfig::Stereo(), kSampleRateHz,
                         kTwentyMsFrameSize);

  EXPECT_CHECK_DEATH(VoiceIsolation::Create(CreateMockComponent(), params));
}

TEST(VoiceIsolationDeathTest, ProcessAudioDiesOnChannelCountMismatch) {
  std::unique_ptr<VoiceIsolation> voice_isolation =
      CreatePassthroughVoiceIsolation(ChannelLayoutConfig::Stereo());
  std::unique_ptr<AudioBus> valid_bus =
      AudioBus::Create(2, kComponentFrameSize);
  std::unique_ptr<AudioBus> wrong_channels_bus_1 =
      AudioBus::Create(1, kComponentFrameSize);
  std::unique_ptr<AudioBus> wrong_channels_bus_2 =
      AudioBus::Create(1, kComponentFrameSize);

  // Both buses mismatch `channels_`.
  EXPECT_CHECK_DEATH(voice_isolation->ProcessAudio(*wrong_channels_bus_1,
                                                   *wrong_channels_bus_2));
  // `input_bus` matches `channels_`, but `output_bus` does not.
  EXPECT_CHECK_DEATH(
      voice_isolation->ProcessAudio(*valid_bus, *wrong_channels_bus_1));
}

TEST(VoiceIsolationDeathTest, ProcessAudioDiesOnFrameCountMismatch) {
  AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR,
                         ChannelLayoutConfig::Mono(), kSampleRateHz,
                         kComponentFrameSize);
  std::unique_ptr<VoiceIsolation> voice_isolation =
      VoiceIsolation::Create(CreateMockComponent(), params);

  // Mono buses skip the ChannelMixer and go straight to the mock component,
  // which accepts any size, so only VoiceIsolation itself can catch this.
  std::unique_ptr<AudioBus> valid_bus =
      AudioBus::Create(1, kComponentFrameSize);
  std::unique_ptr<AudioBus> wrong_frames_bus_1 =
      AudioBus::Create(1, 2 * kComponentFrameSize);
  std::unique_ptr<AudioBus> wrong_frames_bus_2 =
      AudioBus::Create(1, 2 * kComponentFrameSize);

  // Both buses mismatch `kFrameSize`.
  EXPECT_CHECK_DEATH(
      voice_isolation->ProcessAudio(*wrong_frames_bus_1, *wrong_frames_bus_2));
  // `input_bus` matches `kFrameSize`, but `output_bus` does not.
  EXPECT_CHECK_DEATH(
      voice_isolation->ProcessAudio(*valid_bus, *wrong_frames_bus_1));
}

TEST(VoiceIsolationDeathTest, ProcessAudioDiesOnSameInputAndOutputBus) {
  AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR,
                         ChannelLayoutConfig::Mono(), kSampleRateHz,
                         kComponentFrameSize);
  std::unique_ptr<VoiceIsolation> voice_isolation =
      VoiceIsolation::Create(CreateMockComponent(), params);
  std::unique_ptr<AudioBus> bus = AudioBus::Create(1, kComponentFrameSize);

  EXPECT_CHECK_DEATH(voice_isolation->ProcessAudio(*bus, *bus));
}

TEST(VoiceIsolationTest, AlgorithmicDelay) {
  AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR,
                         ChannelLayoutConfig::Stereo(), kSampleRateHz,
                         kComponentFrameSize);

  // 1. With PassthroughVoiceIsolation (component algorithmic delay is 0).
  auto passthrough = std::make_unique<PassthroughVoiceIsolation>(
      kComponentFrameSize, kComponentFramesPerSecond);
  EXPECT_EQ(passthrough->AlgorithmicDelay(), base::TimeDelta());

  std::unique_ptr<VoiceIsolation> vi_passthrough =
      VoiceIsolation::Create(std::move(passthrough), params);
  ASSERT_NE(vi_passthrough, nullptr);
  EXPECT_EQ(vi_passthrough->AlgorithmicDelay(), base::TimeDelta());

  // 2. With default model (BufferedVoiceIsolation + StftVoiceIsolation +
  // BandSplitVoiceIsolation + TfLiteVoiceIsolation).
  // StftVoiceIsolation adds 10ms algorithmic delay and BufferedVoiceIsolation
  // adds 10ms buffering delay.
  std::unique_ptr<tflite::FlatBufferModel> model =
      LoadVoiceIsolationTestModel();
  std::unique_ptr<VoiceIsolation> vi_model =
      VoiceIsolation::Create(model.get(), params);
  ASSERT_NE(vi_model, nullptr);
  EXPECT_EQ(vi_model->AlgorithmicDelay(), base::Milliseconds(20));
}

// After bias subtraction, the stateless test model scales bins below 7.5 kHz
// uniformly by `0.9 * kStatelessModelDereverbGain + 0.1 *
// kStatelessModelDenoisingOnlyGain` with zero phase shift, so a low-band pulse
// sent through the full VoiceIsolation pipeline comes out as the scaled input
// waveform delayed by the exact number of frames advertised by
// AlgorithmicDelay().
TEST(VoiceIsolationTest,
     OutputIsInputDelayedByAlgorithmicDelayWithStatelessModel) {
  AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR,
                         ChannelLayoutConfig::Mono(), kSampleRateHz,
                         kComponentFrameSize);
  FakeModel fake_model = BuildStatelessTestModel();
  std::unique_ptr<VoiceIsolation> voice_isolation =
      VoiceIsolation::Create(fake_model.model.get(), params);
  ASSERT_NE(voice_isolation, nullptr);

  // Use a smooth 10 ms low-band pulse (1 kHz under a Hann^2 window) rather
  // than a single-sample delta: in BandSplitVoiceIsolation,
  // ComputeHighBandGains() derives both high-band gains of a 20 ms call from
  // its first DFT, so when that first DFT is silent both gains are 0 and a
  // broadband transient loses its high band in the DFT where it starts. A
  // Hann^2-windowed pulse keeps its energy inside the 50 Hz to 7.5 kHz band
  // where the stateless model applies a uniform gain (the test passes for
  // carrier frequencies from ~200 Hz up to ~6.2 kHz and fails outside that
  // range as sidebands reach the bypassed DC bin or the 7.5 kHz crossfade).
  constexpr size_t kNumBlocks = 8;
  constexpr size_t kTotalSamples = kNumBlocks * kComponentFrameSize;
  // 600 makes the pulse straddle the 960-sample boundary between STFT frames
  // without lining up with the 10 ms blocks.
  constexpr size_t kPulseStartIndex = 600;
  constexpr size_t kPulseLength = kSampleRateHz / 100;  // 10 ms.
  constexpr size_t kPulsePeakIndex = kPulseStartIndex + kPulseLength / 2;
  constexpr float kCarrierFrequencyHz = 1000.0f;

  // Convert the advertised algorithmic delay into a sample offset at 48 kHz.
  const size_t delay_frames =
      base::checked_cast<size_t>(AudioTimestampHelper::TimeToFrames(
          voice_isolation->AlgorithmicDelay(), kSampleRateHz));
  ASSERT_GT(delay_frames, 0u);
  ASSERT_LT(kPulseStartIndex + kPulseLength + delay_frames, kTotalSamples);

  const std::vector<float> input = MakeTonePulse(
      kTotalSamples, kPulseStartIndex, kPulseLength, kCarrierFrequencyHz);
  const std::vector<float> output = ProcessInBlocks(*voice_isolation, input);

  // The output peak must appear at `kPulsePeakIndex + delay_frames`.
  const size_t output_peak_index =
      static_cast<size_t>(std::ranges::max_element(output) - output.begin());
  ASSERT_EQ(output_peak_index, kPulsePeakIndex + delay_frames);

  // Every output sample must match the scaled input delayed by `delay_frames`.
  std::vector<float> expected(kTotalSamples, 0.0f);
  for (size_t i = 0; i < kTotalSamples - delay_frames; ++i) {
    expected[i + delay_frames] = kStatelessModelLowBandGain * input[i];
  }
  // 1e-6 covers float rounding through two STFT round trips.
  constexpr float kTolerance = 1e-6f;
  EXPECT_THAT(output,
              testing::Pointwise(testing::FloatNear(kTolerance), expected));
}

}  // namespace media
