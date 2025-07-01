// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_audio_processing_layout.h"

#include "base/test/scoped_feature_list.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_processor_options.h"

namespace blink {

using EchoCancellationType = AudioProcessingProperties::EchoCancellationType;
using VoiceIsolationType = AudioProcessingProperties::VoiceIsolationType;
using PlatformEffectsMask = media::AudioParameters::PlatformEffectsMask;

TEST(AudioProcessingPropertiesToAudioProcessingSettingsTest,
     VerifyDefaultSettingsState) {
  const media::AudioProcessingSettings default_settings;
  EXPECT_TRUE(default_settings.echo_cancellation);
  EXPECT_TRUE(default_settings.noise_suppression);
  EXPECT_TRUE(default_settings.automatic_gain_control);
  EXPECT_TRUE(default_settings.multi_channel_capture_processing);
}

TEST(AudioProcessingPropertiesToAudioProcessingSettingsTest,
     DefaultPropertiesAndSettingsMatch) {
  const media::AudioProcessingSettings default_settings;
  AudioProcessingProperties properties;
  const media::AudioProcessingSettings generated_settings =
      MediaStreamAudioProcessingLayout::ComputeWebrtcProcessingSettingsForTests(
          properties,
          /*enabled_platform_effects=*/0,
          default_settings.multi_channel_capture_processing);
  EXPECT_EQ(default_settings, generated_settings);
}

TEST(AudioProcessingPropertiesToAudioProcessingSettingsTest,
     DisableDefaultProperties) {
  AudioProcessingProperties properties;
  properties.DisableDefaultProperties();
  const media::AudioProcessingSettings settings =
      MediaStreamAudioProcessingLayout::ComputeWebrtcProcessingSettingsForTests(
          properties,
          /*enabled_platform_effects=*/0,
          /*multichannel_processing=*/true);
  EXPECT_FALSE(settings.echo_cancellation);
  EXPECT_FALSE(settings.noise_suppression);
  EXPECT_FALSE(settings.automatic_gain_control);

  EXPECT_EQ(properties.voice_isolation,
            VoiceIsolationType::kVoiceIsolationDefault);
}

TEST(AudioProcessingPropertiesToAudioProcessingSettingsTest,
     AllBrowserPropertiesEnabled) {
  const AudioProcessingProperties properties{
      .echo_cancellation_type = EchoCancellationType::kEchoCancellationAec3,
      .auto_gain_control = true,
      .noise_suppression = true};
  const media::AudioProcessingSettings settings =
      MediaStreamAudioProcessingLayout::ComputeWebrtcProcessingSettingsForTests(
          properties,
          /*enabled_platform_effects=*/0,
          /*multichannel_processing=*/true);
  EXPECT_TRUE(settings.echo_cancellation);
  EXPECT_TRUE(settings.noise_suppression);
  EXPECT_TRUE(settings.automatic_gain_control);
}

TEST(AudioProcessingPropertiesToAudioProcessingSettingsTest,
     SystemAecDisablesBrowserAec) {
  AudioProcessingProperties properties{
      .echo_cancellation_type = EchoCancellationType::kEchoCancellationSystem};
  media::AudioProcessingSettings settings =
      MediaStreamAudioProcessingLayout::ComputeWebrtcProcessingSettingsForTests(
          properties,
          /*enabled_platform_effects=*/PlatformEffectsMask::ECHO_CANCELLER,
          /*multichannel_processing=*/true);
  EXPECT_FALSE(settings.echo_cancellation);
}

TEST(AudioProcessingPropertiesToAudioProcessingSettingsTest,
     SystemNsDeactivatesBrowserNs) {
  constexpr AudioProcessingProperties properties;
  media::AudioProcessingSettings settings_without_system_ns =
      MediaStreamAudioProcessingLayout::ComputeWebrtcProcessingSettingsForTests(
          properties,
          /*enabled_platform_effects=*/0,
          /*multichannel_processing=*/true);

  EXPECT_TRUE(settings_without_system_ns.noise_suppression);

  media::AudioProcessingSettings settings_with_system_ns =
      MediaStreamAudioProcessingLayout::ComputeWebrtcProcessingSettingsForTests(
          properties,
          /*enabled_platform_effects=*/PlatformEffectsMask::NOISE_SUPPRESSION,
          /*multichannel_processing=*/true);

  EXPECT_FALSE(settings_with_system_ns.noise_suppression);
}

#if (BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN))
TEST(AudioProcessingPropertiesToAudioProcessingSettingsTest,
     SystemNsDoesNotDeactivateBrowserNsWhenTandemNsIsAllowed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      media::kEnforceSystemEchoCancellation, {{"allow_ns_in_tandem", "true"}});

  constexpr AudioProcessingProperties properties{
      .echo_cancellation_type = EchoCancellationType::kEchoCancellationSystem};
  media::AudioProcessingSettings settings_without_system_ns =
      MediaStreamAudioProcessingLayout::ComputeWebrtcProcessingSettingsForTests(
          properties,
          /*enabled_platform_effects=*/0,
          /*multichannel_processing=*/true);

  EXPECT_TRUE(settings_without_system_ns.noise_suppression);

  media::AudioProcessingSettings settings_with_system_ns =
      MediaStreamAudioProcessingLayout::ComputeWebrtcProcessingSettingsForTests(
          properties,
          /*enabled_platform_effects=*/PlatformEffectsMask::NOISE_SUPPRESSION,
          /*multichannel_processing=*/true);

  EXPECT_TRUE(settings_with_system_ns.noise_suppression);

  MediaStreamAudioProcessingLayout processing_layout(
      properties, PlatformEffectsMask::NOISE_SUPPRESSION, /*channels=*/1);
  EXPECT_TRUE(processing_layout.NoiseSuppressionInTandem());
}
#endif

TEST(AudioProcessingPropertiesToAudioProcessingSettingsTest,
     SystemAgcDeactivatesBrowserAgc) {
  constexpr AudioProcessingProperties properties;
  media::AudioProcessingSettings settings_without_system_agc =
      MediaStreamAudioProcessingLayout::ComputeWebrtcProcessingSettingsForTests(
          properties,
          /*enabled_platform_effects=*/0,
          /*multichannel_processing=*/true);

  EXPECT_TRUE(settings_without_system_agc.automatic_gain_control);

  media::AudioProcessingSettings settings_with_system_agc =
      MediaStreamAudioProcessingLayout::ComputeWebrtcProcessingSettingsForTests(
          properties,
          /*enabled_platform_effects=*/
          PlatformEffectsMask::AUTOMATIC_GAIN_CONTROL,
          /*multichannel_processing=*/true);

  EXPECT_FALSE(settings_with_system_agc.automatic_gain_control);
}

#if (BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN))
TEST(AudioProcessingPropertiesToAudioProcessingSettingsTest,
     SystemAgcDoesNotDeactivateBrowserAgcWhenTandemAgcIsAllowed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      media::kEnforceSystemEchoCancellation, {{"allow_agc_in_tandem", "true"}});

  constexpr AudioProcessingProperties properties{
      .echo_cancellation_type = EchoCancellationType::kEchoCancellationSystem};
  media::AudioProcessingSettings settings_without_system_agc =
      MediaStreamAudioProcessingLayout::ComputeWebrtcProcessingSettingsForTests(
          properties,
          /*enabled_platform_effects=*/0,
          /*multichannel_processing=*/true);

  EXPECT_TRUE(settings_without_system_agc.automatic_gain_control);

  media::AudioProcessingSettings settings_with_system_agc =
      MediaStreamAudioProcessingLayout::ComputeWebrtcProcessingSettingsForTests(
          properties,
          /*enabled_platform_effects=*/
          PlatformEffectsMask::AUTOMATIC_GAIN_CONTROL,
          /*multichannel_processing=*/true);

  EXPECT_TRUE(settings_with_system_agc.automatic_gain_control);

  MediaStreamAudioProcessingLayout processing_layout(
      properties, PlatformEffectsMask::AUTOMATIC_GAIN_CONTROL, /*channels=*/1);
  EXPECT_TRUE(processing_layout.AutomaticGainControlInTandem());
}
#endif

TEST(AudioProcessingPropertiesTest, VerifyDefaultProcessingState) {
  constexpr AudioProcessingProperties kDefaultProperties;
  EXPECT_EQ(kDefaultProperties.echo_cancellation_type,
            EchoCancellationType::kEchoCancellationAec3);
  EXPECT_TRUE(kDefaultProperties.auto_gain_control);
  EXPECT_TRUE(kDefaultProperties.noise_suppression);
  EXPECT_EQ(kDefaultProperties.voice_isolation,
            VoiceIsolationType::kVoiceIsolationDefault);
}

class MediaStreamAudioProcessingLayoutTest
    : public testing::TestWithParam<
          testing::tuple<AudioProcessingProperties::EchoCancellationType,
                         bool,
                         bool>> {};

TEST_P(MediaStreamAudioProcessingLayoutTest,
       PlatformAecNsAgcCorrectIfAvailale) {
  AudioProcessingProperties properties;
  properties.echo_cancellation_type = std::get<0>(GetParam());
  properties.noise_suppression = std::get<1>(GetParam());
  properties.auto_gain_control = std::get<2>(GetParam());

  int available_platform_effects =
      media::AudioParameters::ECHO_CANCELLER |
      media::AudioParameters::NOISE_SUPPRESSION |
      media::AudioParameters::AUTOMATIC_GAIN_CONTROL;

  MediaStreamAudioProcessingLayout processing_layout(
      properties, available_platform_effects, /*channels=*/1);

  int expected_effects = available_platform_effects;

  EchoCanceller echo_canceller = EchoCanceller::From(properties);

  if (echo_canceller.IsPlatformProvided()) {
#if (!BUILDFLAG(IS_WIN))
    // Disable AGC and NS if not requested.
    if (!properties.auto_gain_control) {
      expected_effects &= ~media::AudioParameters::AUTOMATIC_GAIN_CONTROL;
    }
    if (!properties.noise_suppression &&
        !MediaStreamAudioProcessingLayout::
            IsIndependentSystemNsAllowedForTests()) {
      // TODO(crbug.com/417413190): It's weird that we keep NS enabled in this
      // case if IsIndependentSystemNsAllowed() returns true, but this is how
      // the code works now.
      expected_effects &= ~media::AudioParameters::NOISE_SUPPRESSION;
    }
#endif
  } else {
    // No platform processing if platform AEC is not requested.
    expected_effects &= ~media::AudioParameters::ECHO_CANCELLER;
    expected_effects &= ~media::AudioParameters::AUTOMATIC_GAIN_CONTROL;
    if (!MediaStreamAudioProcessingLayout::
            IsIndependentSystemNsAllowedForTests()) {
      // Special case for NS.
      expected_effects &= ~media::AudioParameters::NOISE_SUPPRESSION;
    }
  }

  EXPECT_EQ(expected_effects,
            processing_layout.platform_effects() &
                (media::AudioParameters::ECHO_CANCELLER |
                 media::AudioParameters::NOISE_SUPPRESSION |
                 media::AudioParameters::AUTOMATIC_GAIN_CONTROL))
      << "\nproperties: " << properties.ToString()
      << "\necho_canceller: " << echo_canceller.ToString() << "\nexpected: "
      << media::AudioParameters::EffectsMaskToString(expected_effects)
      << "\n  result: "
      << media::AudioParameters::EffectsMaskToString(
             processing_layout.platform_effects());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    MediaStreamAudioProcessingLayoutTest,
    ::testing::Combine(
        ::testing::ValuesIn({AudioProcessingProperties::EchoCancellationType::
                                 kEchoCancellationDisabled,
                             AudioProcessingProperties::EchoCancellationType::
                                 kEchoCancellationSystem,
                             AudioProcessingProperties::EchoCancellationType::
                                 kEchoCancellationAec3}),
        // ACG and NS on/off.
        ::testing::Bool(),
        ::testing::Bool()));

#if BUILDFLAG(SYSTEM_LOOPBACK_AS_AEC_REFERENCE)
class MediaStreamAudioProcessingLayoutLoopbackTest
    : public testing::TestWithParam<testing::tuple<bool, bool>> {};

TEST_P(MediaStreamAudioProcessingLayoutLoopbackTest, LoopbackAec) {
  base::test::ScopedFeatureList scoped_feature_list;
  bool loopback_aec_enforced = std::get<0>(GetParam());
  bool aec_enabled = std::get<1>(GetParam());
  if (loopback_aec_enforced) {
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        media::kSystemLoopbackAsAecReference, {{"forced_on", "true"}});
  } else {
    scoped_feature_list.InitAndDisableFeature(
        media::kSystemLoopbackAsAecReference);
  }

  AudioProcessingProperties properties;
  properties.echo_cancellation_type =
      aec_enabled ? AudioProcessingProperties::EchoCancellationType::
                        kEchoCancellationAec3
                  : AudioProcessingProperties::EchoCancellationType::
                        kEchoCancellationDisabled;

  MediaStreamAudioProcessingLayout processing_layout(
      properties, /*available_platform_effects=*/0, /*channels=*/1);

  EXPECT_TRUE(processing_layout.NeedApmInAudioService());
  EXPECT_EQ(
      processing_layout.webrtc_processing_settings().use_loopback_aec_reference,
      aec_enabled && loopback_aec_enforced);
}

INSTANTIATE_TEST_SUITE_P(All,
                         MediaStreamAudioProcessingLayoutLoopbackTest,
                         ::testing::Combine(
                             // AEC on/off.
                             ::testing::Bool(),
                             // Lopoback AEC enforced on/off.
                             ::testing::Bool()));

#endif

}  // namespace blink
