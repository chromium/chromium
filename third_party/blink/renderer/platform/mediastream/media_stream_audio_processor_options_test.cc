// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_processor_options.h"

#include "base/test/scoped_feature_list.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

using EchoCancellationType = AudioProcessingProperties::EchoCancellationType;
using VoiceIsolationType = AudioProcessingProperties::VoiceIsolationType;

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
      properties.ToAudioProcessingSettings(
          default_settings.multi_channel_capture_processing);
  EXPECT_EQ(default_settings, generated_settings);
}

TEST(AudioProcessingPropertiesToAudioProcessingSettingsTest,
     DisableDefaultProperties) {
  AudioProcessingProperties properties;
  properties.DisableDefaultProperties();
  const media::AudioProcessingSettings settings =
      properties.ToAudioProcessingSettings(
          /*multi_channel_capture_processing=*/true);
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
      properties.ToAudioProcessingSettings(
          /*multi_channel_capture_processing=*/true);
  EXPECT_TRUE(settings.echo_cancellation);
  EXPECT_TRUE(settings.noise_suppression);
  EXPECT_TRUE(settings.automatic_gain_control);
}

TEST(AudioProcessingPropertiesToAudioProcessingSettingsTest,
     SystemAecDisablesBrowserAec) {
  AudioProcessingProperties properties{
      .echo_cancellation_type = EchoCancellationType::kEchoCancellationSystem};
  media::AudioProcessingSettings settings =
      properties.ToAudioProcessingSettings(
          /*multi_channel_capture_processing=*/true);
  EXPECT_FALSE(settings.echo_cancellation);
}

TEST(AudioProcessingPropertiesToAudioProcessingSettingsTest,
     SystemNsDeactivatesBrowserNs) {
  constexpr AudioProcessingProperties kPropertiesWithoutSystemNs{
      .system_noise_suppression_activated = false};
  media::AudioProcessingSettings settings_without_system_ns =
      kPropertiesWithoutSystemNs.ToAudioProcessingSettings(
          /*multi_channel_capture_processing=*/true);
  EXPECT_TRUE(settings_without_system_ns.noise_suppression);

  constexpr AudioProcessingProperties kPropertiesWithSystemNs{
      .system_noise_suppression_activated = true};
  media::AudioProcessingSettings settings_with_system_ns =
      kPropertiesWithSystemNs.ToAudioProcessingSettings(
          /*multi_channel_capture_processing=*/true);
  EXPECT_FALSE(settings_with_system_ns.noise_suppression);
}

#if (BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN))
TEST(AudioProcessingPropertiesToAudioProcessingSettingsTest,
     SystemNsDoesNotDeactivateBrowserNsWhenTandemNsIsAllowed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      media::kEnforceSystemEchoCancellation, {{"allow_ns_in_tandem", "true"}});
  constexpr AudioProcessingProperties kPropertiesWithoutSystemNs{
      .system_noise_suppression_activated = false};
  media::AudioProcessingSettings settings_without_system_ns =
      kPropertiesWithoutSystemNs.ToAudioProcessingSettings(
          /*multi_channel_capture_processing=*/true);
  EXPECT_TRUE(settings_without_system_ns.noise_suppression);

  constexpr AudioProcessingProperties kPropertiesWithSystemNs{
      .system_noise_suppression_activated = true};
  media::AudioProcessingSettings settings_with_system_ns =
      kPropertiesWithSystemNs.ToAudioProcessingSettings(
          /*multi_channel_capture_processing=*/true);
  EXPECT_TRUE(settings_with_system_ns.noise_suppression);
}
#endif

TEST(AudioProcessingPropertiesToAudioProcessingSettingsTest,
     SystemAgcDeactivatesBrowserAgc) {
  constexpr AudioProcessingProperties kPropertiesWithoutSystemAgc{
      .system_gain_control_activated = false};
  media::AudioProcessingSettings settings_without_system_agc =
      kPropertiesWithoutSystemAgc.ToAudioProcessingSettings(
          /*multi_channel_capture_processing=*/true);
  EXPECT_TRUE(settings_without_system_agc.automatic_gain_control);

  constexpr AudioProcessingProperties kPropertiesWithSystemAgc{
      .system_gain_control_activated = true};
  media::AudioProcessingSettings settings_with_system_agc =
      kPropertiesWithSystemAgc.ToAudioProcessingSettings(
          /*multi_channel_capture_processing=*/true);
  EXPECT_FALSE(settings_with_system_agc.automatic_gain_control);
}

#if (BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN))
TEST(AudioProcessingPropertiesToAudioProcessingSettingsTest,
     SystemAgcDoesNotDeactivateBrowserAgcWhenTandemAgcIsAllowed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      media::kEnforceSystemEchoCancellation, {{"allow_agc_in_tandem", "true"}});
  constexpr AudioProcessingProperties kPropertiesWithoutSystemAgc{
      .system_gain_control_activated = false};
  media::AudioProcessingSettings settings_without_system_agc =
      kPropertiesWithoutSystemAgc.ToAudioProcessingSettings(
          /*multi_channel_capture_processing=*/true);
  EXPECT_TRUE(settings_without_system_agc.automatic_gain_control);

  constexpr AudioProcessingProperties kPropertiesWithSystemAgc{
      .system_gain_control_activated = true};
  media::AudioProcessingSettings settings_with_system_agc =
      kPropertiesWithSystemAgc.ToAudioProcessingSettings(
          /*multi_channel_capture_processing=*/true);
  EXPECT_TRUE(settings_with_system_agc.automatic_gain_control);
}
#endif

TEST(AudioProcessingPropertiesTest, VerifyDefaultProcessingState) {
  constexpr AudioProcessingProperties kDefaultProperties;
  EXPECT_EQ(kDefaultProperties.echo_cancellation_type,
            EchoCancellationType::kEchoCancellationAec3);
  EXPECT_FALSE(kDefaultProperties.system_gain_control_activated);
  EXPECT_TRUE(kDefaultProperties.auto_gain_control);
  EXPECT_TRUE(kDefaultProperties.noise_suppression);
  EXPECT_EQ(kDefaultProperties.voice_isolation,
            VoiceIsolationType::kVoiceIsolationDefault);
}

}  // namespace blink
