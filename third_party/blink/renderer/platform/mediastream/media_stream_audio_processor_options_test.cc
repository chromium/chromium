// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_processor_options.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

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

  EXPECT_EQ(
      properties.voice_isolation,
      AudioProcessingProperties::VoiceIsolationType::kVoiceIsolationDefault);
}

TEST(AudioProcessingPropertiesToAudioProcessingSettingsTest,
     AllBrowserPropertiesEnabled) {
  const AudioProcessingProperties properties{
      .echo_cancellation_type = AudioProcessingProperties::
          EchoCancellationType::kEchoCancellationAec3,
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
      .echo_cancellation_type = AudioProcessingProperties::
          EchoCancellationType::kEchoCancellationSystem};
  media::AudioProcessingSettings settings =
      properties.ToAudioProcessingSettings(
          /*multi_channel_capture_processing=*/true);
  EXPECT_FALSE(settings.echo_cancellation);
}

TEST(AudioProcessingPropertiesToAudioProcessingSettingsTest,
     SystemNsDeactivatesBrowserNs) {
  // Verify that noise suppression is by default enabled, since otherwise this
  // test does not work.
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

TEST(AudioProcessingPropertiesToAudioProcessingSettingsTest,
     SystemAgcDeactivatesBrowserAgc) {
  // Verify that gain control is by default enabled, since otherwise this test
  // does not work.
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

TEST(AudioProcessingPropertiesTest,
     GainControlEnabledReturnsTrueIfBrowserAgcEnabled) {
  constexpr AudioProcessingProperties kPropertiesWithBrowserAgc{
      .auto_gain_control = true};
  EXPECT_TRUE(kPropertiesWithBrowserAgc.GainControlEnabled());
}

TEST(AudioProcessingPropertiesTest,
     GainControlEnabledReturnsTrueIfSystemAgcEnabled) {
  constexpr AudioProcessingProperties kPropertiesWithBrowserAgc{
      .system_gain_control_activated = true,
      .auto_gain_control = true,
  };
  EXPECT_TRUE(kPropertiesWithBrowserAgc.GainControlEnabled());
}

TEST(AudioProcessingPropertiesTest,
     GainControlEnabledReturnsFalseIfAgcDisabled) {
  constexpr AudioProcessingProperties kPropertiesWithBrowserAgc{
      .auto_gain_control = false};
  EXPECT_FALSE(kPropertiesWithBrowserAgc.GainControlEnabled());
}

}  // namespace blink
