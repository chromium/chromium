// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/loopback_signal_provider.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "base/containers/unique_ptr_adapters.h"
#include "base/logging.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_timestamp_helper.h"
#include "services/audio/loopback_coordinator.h"
#include "services/audio/test/fake_consumer.h"
#include "services/audio/test/fake_loopback_group_member.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace audio {
namespace {

// Default volume for sources.
constexpr double kDefaultSourceVolume = 0.5;

// Default volume for the loopback signal provider pull.
constexpr double kDefaultLoopbackVolume = 0.8;

// Piano key frequencies.
constexpr double kMiddleAFreq = 440;
constexpr double kMiddleCFreq = 261.626;

// Audio buffer duration for sources.
constexpr base::TimeDelta kSourceBufferDuration = base::Milliseconds(10);

// How far in the future sources render their audio.
constexpr base::TimeDelta kPlayoutDelay = base::Milliseconds(20);

// The amount of audio signal to record in the main test loop.
constexpr base::TimeDelta kTestRecordingDuration = base::Milliseconds(250);

const media::AudioParameters& GetOutputParams() {
  // 48 kHz, 2-channel audio, with 10 ms buffers.
  static const media::AudioParameters params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Stereo(), 48000, 480);
  return params;
}

class LoopbackSignalProviderTest : public testing::Test {
 public:
  LoopbackSignalProviderTest()
      : group_id_(base::UnguessableToken::Create()),
        output_params_(GetOutputParams()),
        destination_bus_(media::AudioBus::Create(output_params_)),
        consumer_(output_params_.channels(), output_params_.sample_rate()) {}

  ~LoopbackSignalProviderTest() override = default;

  void TearDown() override {
    provider_.reset();
    for (const auto& source : sources_) {
      coordinator_.RemoveMember(source.get());
    }
    sources_.clear();
    task_environment_.FastForwardUntilNoTasksRemain();
  }

 protected:
  FakeLoopbackGroupMember* AddSource(int channels, int sample_rate) {
    auto source =
        std::make_unique<FakeLoopbackGroupMember>(media::AudioParameters(
            media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
            media::ChannelLayoutConfig::Guess(channels), sample_rate,
            (sample_rate * kSourceBufferDuration).InSeconds()));
    auto* source_ptr = source.get();
    sources_.push_back(std::move(source));
    coordinator_.AddMember(group_id_, source_ptr);
    return source_ptr;
  }

  void RemoveSource(FakeLoopbackGroupMember* source) {
    const auto it =
        std::ranges::find_if(sources_, base::MatchesUniquePtr(source));
    if (it != sources_.end()) {
      coordinator_.RemoveMember(source);
      sources_.erase(it);
    }
  }

  void CreateSignalProvider() {
    CHECK(!provider_);
    provider_ = std::make_unique<LoopbackSignalProvider>(
        output_params_, LoopbackGroupObserver::CreateMatchingGroupObserver(
                            &coordinator_, group_id_));
  }

  // Main test loop: Simulates audio sources playing and the signal provider
  // pulling and mixing the data. The resulting mixed audio is recorded in
  // `consumer_`.
  void PumpAndPullAudio(double volume = kDefaultLoopbackVolume) {
    consumer_.Clear();

    const base::TimeDelta buffer_duration =
        media::AudioTimestampHelper::FramesToTime(
            output_params_.frames_per_buffer(), output_params_.sample_rate());
    const int min_frames_to_record = media::AudioTimestampHelper::TimeToFrames(
        kTestRecordingDuration, output_params_.sample_rate());

    while (consumer_.GetRecordedFrameCount() < min_frames_to_record) {
      // 1. Sources "play" audio that will be output in the near future. This
      //    triggers OnData() on the provider's SnooperNodes.
      const base::TimeTicks playout_time =
          task_environment_.NowTicks() + kPlayoutDelay;
      for (const auto& source : sources_) {
        source->RenderMoreAudio(playout_time);
      }

      // 2. The provider pulls and mixes the audio for the current time.
      provider_->PullLoopbackData(destination_bus_.get(),
                                  task_environment_.NowTicks(), volume);

      // 3. "Record" the output for analysis.
      consumer_.Consume(*destination_bus_);

      // 4. Advance time.
      task_environment_.FastForwardBy(buffer_duration);
    }
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  LoopbackCoordinator coordinator_;
  const base::UnguessableToken group_id_;
  std::vector<std::unique_ptr<FakeLoopbackGroupMember>> sources_;

  const media::AudioParameters output_params_;
  std::unique_ptr<media::AudioBus> destination_bus_;
  FakeConsumer consumer_;

  std::unique_ptr<LoopbackSignalProvider> provider_;
};

// Syntactic sugar to confirm a tone exists and its amplitude matches
// expectations. A slightly larger tolerance is used to account for resampling.
#define EXPECT_TONE(ch, frequency, expected_amplitude)                     \
  {                                                                        \
    SCOPED_TRACE(testing::Message() << "ch=" << ch);                       \
    const double amplitude = consumer_.ComputeAmplitudeAt(                 \
        ch, frequency, consumer_.GetRecordedFrameCount());                 \
    VLOG(1) << "For ch=" << ch << ", amplitude at frequency=" << frequency \
            << " is " << amplitude;                                        \
    EXPECT_NEAR(expected_amplitude, amplitude, 0.015);                     \
  }

TEST_F(LoopbackSignalProviderTest, CreateAndDestroy) {
  CreateSignalProvider();
  provider_->Start();
}

TEST_F(LoopbackSignalProviderTest, ProducesSilenceWhenNoSourcesArePresent) {
  CreateSignalProvider();
  provider_->Start();
  PumpAndPullAudio();

  for (int ch = 0; ch < output_params_.channels(); ++ch) {
    SCOPED_TRACE(testing::Message() << "ch=" << ch);
    EXPECT_TRUE(consumer_.IsSilent(ch));
  }
}

TEST_F(LoopbackSignalProviderTest, ProducesAudioFromSingleSource) {
  // Add a source before the provider is started.
  FakeLoopbackGroupMember* const source =
      AddSource(1, output_params_.sample_rate());  // Monaural
  source->SetChannelTone(0, kMiddleAFreq);
  source->SetVolume(kDefaultSourceVolume);

  CreateSignalProvider();
  provider_->Start();
  PumpAndPullAudio();

  // The mono source should be up-mixed to all stereo channels.
  for (int ch = 0; ch < output_params_.channels(); ++ch) {
    EXPECT_TONE(ch, kMiddleAFreq,
                kDefaultSourceVolume * kDefaultLoopbackVolume);
  }
}

TEST_F(LoopbackSignalProviderTest, ProducesAudioFromTwoSources) {
  const int channels = output_params_.channels();
  FakeLoopbackGroupMember* const source1 = AddSource(channels, 48000);
  source1->SetChannelTone(0, kMiddleAFreq);  // Tone on channel 0
  source1->SetVolume(kDefaultSourceVolume);

  FakeLoopbackGroupMember* const source2 = AddSource(channels, 44100);
  source2->SetChannelTone(1, kMiddleCFreq);  // Tone on channel 1
  source2->SetVolume(kDefaultSourceVolume);

  CreateSignalProvider();
  provider_->Start();
  PumpAndPullAudio();

  // The final output should have both tones mixed.
  const double expected_amplitude =
      kDefaultSourceVolume * kDefaultLoopbackVolume;
  EXPECT_TONE(0, kMiddleAFreq, expected_amplitude);
  EXPECT_TONE(1, kMiddleCFreq, expected_amplitude);
}

TEST_F(LoopbackSignalProviderTest, SourceAddedDynamically) {
  // Start with one source.
  FakeLoopbackGroupMember* const source1 =
      AddSource(output_params_.channels(), output_params_.sample_rate());
  source1->SetChannelTone(0, kMiddleAFreq);
  source1->SetVolume(kDefaultSourceVolume);

  CreateSignalProvider();
  provider_->Start();

  // Check that only the first source's audio is present.
  PumpAndPullAudio();
  const double expected_amplitude =
      kDefaultSourceVolume * kDefaultLoopbackVolume;
  EXPECT_TONE(0, kMiddleAFreq, expected_amplitude);
  EXPECT_TONE(1, kMiddleAFreq, 0.0);

  // Add a second source while running.
  FakeLoopbackGroupMember* const source2 =
      AddSource(output_params_.channels(), output_params_.sample_rate());
  source2->SetChannelTone(1, kMiddleCFreq);
  source2->SetVolume(kDefaultSourceVolume);

  // Check that both sources are now mixed.
  PumpAndPullAudio();
  EXPECT_TONE(0, kMiddleAFreq, expected_amplitude);
  EXPECT_TONE(1, kMiddleCFreq, expected_amplitude);
}

TEST_F(LoopbackSignalProviderTest, SourceRemovedDynamically) {
  // Start with two sources.
  FakeLoopbackGroupMember* const source1 =
      AddSource(output_params_.channels(), output_params_.sample_rate());
  source1->SetChannelTone(0, kMiddleAFreq);
  source1->SetVolume(kDefaultSourceVolume);
  FakeLoopbackGroupMember* const source2 =
      AddSource(output_params_.channels(), output_params_.sample_rate());
  source2->SetChannelTone(1, kMiddleCFreq);
  source2->SetVolume(kDefaultSourceVolume);

  CreateSignalProvider();
  provider_->Start();

  // Check that both are mixed.
  PumpAndPullAudio();
  const double expected_amplitude =
      kDefaultSourceVolume * kDefaultLoopbackVolume;
  EXPECT_TONE(0, kMiddleAFreq, expected_amplitude);
  EXPECT_TONE(1, kMiddleCFreq, expected_amplitude);

  // Remove the second source.
  RemoveSource(source2);

  // Check that only the first source's audio is present.
  PumpAndPullAudio();
  EXPECT_TONE(0, kMiddleAFreq, expected_amplitude);
  EXPECT_TONE(1, kMiddleCFreq, 0.0);  // Should be silent now.
}

TEST_F(LoopbackSignalProviderTest, AudioChangesWithPullVolume) {
  FakeLoopbackGroupMember* const source =
      AddSource(1, output_params_.sample_rate());
  source->SetChannelTone(0, kMiddleAFreq);
  source->SetVolume(kDefaultSourceVolume);

  CreateSignalProvider();
  provider_->Start();

  // Pull with default volume.
  PumpAndPullAudio(kDefaultLoopbackVolume);
  double expected_amplitude = kDefaultSourceVolume * kDefaultLoopbackVolume;
  EXPECT_TONE(0, kMiddleAFreq, expected_amplitude);

  // Pull with half volume.
  PumpAndPullAudio(kDefaultLoopbackVolume / 2.0);
  expected_amplitude /= 2.0;
  EXPECT_TONE(0, kMiddleAFreq, expected_amplitude);

  // Pull with zero volume.
  PumpAndPullAudio(0.0);
  EXPECT_TONE(0, kMiddleAFreq, 0.0);
}

TEST_F(LoopbackSignalProviderTest, HandlesDifferentSampleRates) {
  // Source 1 at 48 kHz.
  FakeLoopbackGroupMember* const source1 =
      AddSource(output_params_.channels(), 48000);
  source1->SetChannelTone(0, kMiddleAFreq);
  source1->SetVolume(kDefaultSourceVolume);

  // Source 2 at 22.05 kHz. SnooperNode will have to resample this up to 48 kHz.
  FakeLoopbackGroupMember* const source2 =
      AddSource(output_params_.channels(), 22050);
  source2->SetChannelTone(1, kMiddleCFreq);
  source2->SetVolume(kDefaultSourceVolume);

  CreateSignalProvider();
  provider_->Start();
  PumpAndPullAudio();

  // Check that both tones are present at the correct amplitude.
  const double expected_amplitude =
      kDefaultSourceVolume * kDefaultLoopbackVolume;
  EXPECT_TONE(0, kMiddleAFreq, expected_amplitude);
  EXPECT_TONE(1, kMiddleCFreq, expected_amplitude);
}

TEST_F(LoopbackSignalProviderTest,
       HandlesSourceStartingAfterProviderIsStarted) {
  CreateSignalProvider();
  provider_->Start();

  // The provider is running, but there are no sources. Should be silent.
  PumpAndPullAudio();
  EXPECT_TRUE(consumer_.IsSilent(0));
  EXPECT_TRUE(consumer_.IsSilent(1));

  // Now, add a source.
  FakeLoopbackGroupMember* const source =
      AddSource(output_params_.channels(), output_params_.sample_rate());
  source->SetChannelTone(0, kMiddleAFreq);
  source->SetVolume(kDefaultSourceVolume);

  // The provider should now be capturing the source's audio.
  PumpAndPullAudio();
  EXPECT_TONE(0, kMiddleAFreq, kDefaultSourceVolume * kDefaultLoopbackVolume);
}

TEST_F(LoopbackSignalProviderTest, SurvivesSourceBeingRemovedThenReAdded) {
  // Start with one source.
  FakeLoopbackGroupMember* source =
      AddSource(output_params_.channels(), output_params_.sample_rate());
  source->SetChannelTone(0, kMiddleAFreq);
  source->SetVolume(kDefaultSourceVolume);

  CreateSignalProvider();
  provider_->Start();

  // Check that audio is present.
  PumpAndPullAudio();
  const double expected_amplitude =
      kDefaultSourceVolume * kDefaultLoopbackVolume;
  EXPECT_TONE(0, kMiddleAFreq, expected_amplitude);

  // Remove the source from the coordinator. The test fixture still owns it.
  coordinator_.RemoveMember(source);

  // Check for silence.
  PumpAndPullAudio();
  EXPECT_TONE(0, kMiddleAFreq, 0.0);

  // Add the same source back to the coordinator.
  coordinator_.AddMember(group_id_, source);

  // Check that audio is back.
  PumpAndPullAudio();
  EXPECT_TONE(0, kMiddleAFreq, expected_amplitude);
}

}  // namespace
}  // namespace audio
