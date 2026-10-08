// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/voice_isolation/voice_isolation.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <numeric>
#include <utility>

#include "base/test/gmock_expected_support.h"
#include "base/test/gtest_util.h"
#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
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

// TODO(barrerap): Enable once TfLiteVoiceIsolation stops advancing the model
// state when it computes the bias.
TEST(VoiceIsolationTest, DISABLED_VoiceIsolationCanAdaptToAudioParameters) {
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

  // Fill input bus with dummy data.
  std::fill(input_bus->channel(0).begin(), input_bus->channel(0).end(), 42.f);
  std::fill(input_bus->channel(1).begin(), input_bus->channel(1).end(), -1.0f);

  // Clear output bus to verify changes.
  output_bus->Zero();

  // BufferedVoiceIsolation and the STFT introduce a delay of two 10 ms frames.
  // The third frame holds the zero-padded start of the first STFT output,
  // which stays silent only if the model starts from its initial state.
  constexpr int kNumLatencyFrames = 3;
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

// TODO(barrerap): Enable once TfLiteVoiceIsolation::ClearBuffers() resets the
// model resource variables.
TEST(VoiceIsolationTest, DISABLED_ClearBuffersMatchesFreshInstance) {
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

}  // namespace media
