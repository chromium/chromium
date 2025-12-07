// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/loopback_mixin.h"

#include <memory>

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_switches.h"
#include "services/audio/loopback_coordinator.h"
#include "services/audio/loopback_signal_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AtLeast;
using testing::Return;
using testing::WithArgs;

namespace audio {

namespace {

constexpr int kSampleRate = 48000;
constexpr int kFramesPerBuffer = 480;

// A mock for LoopbackSignalProvider to control its behavior in tests.
// This requires Start() and PullLoopbackData() to be virtual.
class MockLoopbackSignalProvider : public LoopbackSignalProviderInterface {
 public:
  MockLoopbackSignalProvider() {}
  ~MockLoopbackSignalProvider() override = default;

  MOCK_METHOD(void, Start, (), (override));
  MOCK_METHOD(base::TimeTicks,
              PullLoopbackData,
              (media::AudioBus * audio_bus,
               base::TimeTicks capture_time,
               double volume),
              (override));
};

// Test class to access the protected constructor of LoopbackMixin.
class LoopbackMixinUnderTest : public LoopbackMixin {
 public:
  LoopbackMixinUnderTest(
      std::unique_ptr<LoopbackSignalProviderInterface> signal_provider,
      const media::AudioParameters& params,
      OnDataCallback on_data_callback)
      : LoopbackMixin(std::move(signal_provider),
                      params,
                      std::move(on_data_callback)) {}
};

}  // namespace

class LoopbackMixinTest : public testing::Test {
 public:
  LoopbackMixinTest()
      : params_(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                media::ChannelLayoutConfig::Stereo(),
                kSampleRate,
                kFramesPerBuffer) {}

 protected:
  // Helper to verify the contents of an AudioBus.
  void VerifyAudioBus(const media::AudioBus* bus, float expected_value) {
    ASSERT_TRUE(bus);
    for (int i = 0; i < bus->channels(); ++i) {
      auto channel_data(bus->channel_span(i));
      for (float sample : channel_data) {
        EXPECT_FLOAT_EQ(sample, expected_value);
      }
    }
  }

  base::test::ScopedFeatureList feature_list_;
  const media::AudioParameters params_;
  LoopbackCoordinator coordinator_;
  base::MockCallback<LoopbackMixin::OnDataCallback> on_data_callback_;
};

// --- Factory Function Tests ---

TEST_F(LoopbackMixinTest, MaybeCreate_FailsWithWrongDeviceId) {
  feature_list_.InitAndEnableFeature(kRestrictOwnAudioAddChromiumBack);
  auto mixin = LoopbackMixin::MaybeCreateRestrictOwnAudioLoopbackMixin(
      &coordinator_, base::UnguessableToken::Create(), "some_other_device_id",
      params_, on_data_callback_.Get());

  EXPECT_EQ(mixin, nullptr);
}

TEST_F(LoopbackMixinTest, MaybeCreate_FailsWhenFeatureIsDisabled) {
  feature_list_.InitAndDisableFeature(kRestrictOwnAudioAddChromiumBack);
  auto mixin = LoopbackMixin::MaybeCreateRestrictOwnAudioLoopbackMixin(
      &coordinator_, base::UnguessableToken::Create(),
      media::AudioDeviceDescription::kLoopbackWithoutChromeId, params_,
      on_data_callback_.Get());

  EXPECT_EQ(mixin, nullptr);
}

TEST_F(LoopbackMixinTest, MaybeCreate_SucceedsWithCorrectIdAndFeature) {
  feature_list_.InitAndEnableFeature(kRestrictOwnAudioAddChromiumBack);
  auto mixin = LoopbackMixin::MaybeCreateRestrictOwnAudioLoopbackMixin(
      &coordinator_, base::UnguessableToken::Create(),
      media::AudioDeviceDescription::kLoopbackWithoutChromeId, params_,
      on_data_callback_.Get());

  if (media::IsRestrictOwnAudioSupported()) {
    EXPECT_NE(mixin, nullptr);
  } else {
    EXPECT_EQ(mixin, nullptr);
  }
}

// --- Behavior Tests ---

TEST_F(LoopbackMixinTest, Start_CallsProviderStart) {
  auto mock_provider = std::make_unique<MockLoopbackSignalProvider>();
  // Keep a raw pointer for setting expectations before moving ownership.
  MockLoopbackSignalProvider* mock_provider_ptr = mock_provider.get();

  LoopbackMixinUnderTest mixin(std::move(mock_provider), params_,
                               on_data_callback_.Get());

  EXPECT_CALL(*mock_provider_ptr, Start()).Times(1);
  mixin.Start();
}

TEST_F(LoopbackMixinTest, OnData_MixesAudioAndForwardsToCallback) {
  const float kSourceValue = 0.2f;
  const float kLoopbackValue = 0.3f;
  const float kExpectedMixedValue = kSourceValue + kLoopbackValue;

  auto mock_provider = std::make_unique<MockLoopbackSignalProvider>();
  MockLoopbackSignalProvider* mock_provider_ptr = mock_provider.get();

  // Prepare the loopback audio data that the mock provider will "provide".
  auto loopback_audio = media::AudioBus::Create(params_);
  std::ranges::for_each(loopback_audio->AllChannels(),
                        [kLoopbackValue](const auto& channel) {
                          std::ranges::fill(channel, kLoopbackValue);
                        });

  // Prepare the primary source audio.
  auto source_audio = media::AudioBus::Create(params_);
  std::ranges::for_each(source_audio->AllChannels(),
                        [kSourceValue](const auto& channel) {
                          std::ranges::fill(channel, kSourceValue);
                        });

  const base::TimeTicks now = base::TimeTicks::Now();
  const double volume = 0.75;
  const media::AudioGlitchInfo glitch_info{.duration = base::Seconds(1)};

  // When PullLoopbackData is called, copy the prepared audio into the output.
  EXPECT_CALL(*mock_provider_ptr, PullLoopbackData(_, now, _))
      .WillOnce([&](media::AudioBus* audio_bus,
                    base::TimeTicks /*capture_time*/, double /*volume*/) {
        loopback_audio->CopyTo(audio_bus);
        return base::TimeTicks::Now();
      });

  LoopbackMixinUnderTest mixin(std::move(mock_provider), params_,
                               on_data_callback_.Get());

  // Set expectation on the final callback.
  EXPECT_CALL(on_data_callback_, Run(_, now, volume, glitch_info))
      .WillOnce(WithArgs<0>([&](const media::AudioBus* mixed_bus) {
        // Verify the audio data was mixed correctly.
        VerifyAudioBus(mixed_bus, kExpectedMixedValue);
      }));

  // Trigger the mixing process.
  mixin.OnData(source_audio.get(), now, volume, glitch_info);
}

}  // namespace audio
