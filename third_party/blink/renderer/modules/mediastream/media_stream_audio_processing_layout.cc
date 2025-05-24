// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_audio_processing_layout.h"

#include "build/build_config.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_switches.h"

namespace blink {

namespace {
// Returns whether system noise suppression is allowed to be used regardless of
// whether the noise suppression constraint is set, or whether a browser-based
// AEC is active. This is currently the default on at least MacOS but is not
// allowed for ChromeOS or Windows setups. On Windows, the system effects AEC,
// NS and AGC always come as a "package" and it it not possible to enable or
// disable the system NS independently. TODO(crbug.com/417413190): delete if not
// relevant any more.
constexpr bool IsIndependentSystemNsAllowed() {
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
  return false;
#else
  return true;
#endif
}

int ApplyPropertiesToEffects(const AudioProcessingProperties& properties,
                             int available_platfrom_effects) {
  int enabled_platform_effects = available_platfrom_effects;

  if (properties.echo_cancellation_type ==
      AudioProcessingProperties::EchoCancellationType::
          kEchoCancellationSystem) {
    // On Windows we can only disable platform NS and AGC effects if platform
    // AEC effect is disabled.
#if !BUILDFLAG(IS_WIN)
    // Platform echo cancellation is requested.
    // TODO(crbug.com/405165917): CHECK(platform_effects &
    // media::AudioParameters::ECHO_CANCELLER);

    // Disable platform NS effect if it's not requested.
    if (!properties.noise_suppression) {
      if (!IsIndependentSystemNsAllowed()) {
        // Special case for NS. TODO(crbug.com/417413190): Rethink.
        enabled_platform_effects &= ~media::AudioParameters::NOISE_SUPPRESSION;
      }
    }

    // Disable platform AGC effect if not requested.
    if (!properties.auto_gain_control) {
      enabled_platform_effects &=
          ~media::AudioParameters::AUTOMATIC_GAIN_CONTROL;
    }
#endif
  } else {
    // No platform processing if platform AEC is not requested.
    enabled_platform_effects &= ~media::AudioParameters::ECHO_CANCELLER;
    enabled_platform_effects &= ~media::AudioParameters::AUTOMATIC_GAIN_CONTROL;
    if (!IsIndependentSystemNsAllowed()) {
      // Special case for NS. TODO(crbug.com/417413190): Rethink.
      enabled_platform_effects &= ~media::AudioParameters::NOISE_SUPPRESSION;
    }
  }
#if BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(media::kCrOSSystemVoiceIsolationOption) &&
      enabled_platform_effects &
          media::AudioParameters::VOICE_ISOLATION_SUPPORTED) {
    if (properties.echo_cancellation_type ==
            AudioProcessingProperties::EchoCancellationType::
                kEchoCancellationAec3 ||
        properties.voice_isolation ==
            AudioProcessingProperties::VoiceIsolationType::
                kVoiceIsolationDisabled) {
      // Force voice isolation effect to be disabled if disabled in the
      // properties, or if browser-based AEC is enabled (platform voice
      // isolation would break browser-based AEC).
      enabled_platform_effects |=
          media::AudioParameters::CLIENT_CONTROLLED_VOICE_ISOLATION;
      enabled_platform_effects &= ~media::AudioParameters::VOICE_ISOLATION;
    } else if (properties.voice_isolation ==
               AudioProcessingProperties::VoiceIsolationType::
                   kVoiceIsolationEnabled) {
      // No browser-based AEC involved; voice isolation is enabled in the
      // properties: force voice isolation to be enabled in the effects.
      enabled_platform_effects |=
          media::AudioParameters::CLIENT_CONTROLLED_VOICE_ISOLATION;

      enabled_platform_effects |= media::AudioParameters::VOICE_ISOLATION;
    } else {
      // Turn off voice isolation control.
      enabled_platform_effects &=
          ~media::AudioParameters::CLIENT_CONTROLLED_VOICE_ISOLATION;
    }
  }

  if (base::FeatureList::IsEnabled(media::kIgnoreUiGains)) {
    // Ignore UI Gains if AGC is running in either browser or system
    if (properties.auto_gain_control) {
      enabled_platform_effects |= media::AudioParameters::IGNORE_UI_GAINS;
    }
  }
#endif

  return enabled_platform_effects;
}

media::AudioProcessingSettings ComputeWebrtcProcessingSettings(
    const AudioProcessingProperties& properties,
    int enabled_platform_effects,
    bool multichannel_processing) {
  media::AudioProcessingSettings out;

  out.echo_cancellation =
      properties.echo_cancellation_type ==
      AudioProcessingProperties::EchoCancellationType::kEchoCancellationAec3;

  out.noise_suppression =
      properties.noise_suppression &&
      (media::IsSystemEchoCancellationEnforcedAndAllowNsInTandem() ||
       !(enabled_platform_effects & media::AudioParameters::NOISE_SUPPRESSION));

  out.automatic_gain_control =
      properties.auto_gain_control &&
      (media::IsSystemEchoCancellationEnforcedAndAllowAgcInTandem() ||
       !(enabled_platform_effects &
         media::AudioParameters::AUTOMATIC_GAIN_CONTROL));

  out.multi_channel_capture_processing = multichannel_processing;
  return out;
}

}  // namespace

// static
bool MediaStreamAudioProcessingLayout::IsIndependentSystemNsAllowedForTests() {
  return IsIndependentSystemNsAllowed();
}

// static
media::AudioProcessingSettings
MediaStreamAudioProcessingLayout::ComputeWebrtcProcessingSettingsForTests(
    const AudioProcessingProperties& properties,
    int enabled_platform_effects,
    bool multichannel_processing) {
  return ComputeWebrtcProcessingSettings(properties, enabled_platform_effects,
                                         multichannel_processing);
}

MediaStreamAudioProcessingLayout::MediaStreamAudioProcessingLayout(
    const AudioProcessingProperties& properties,
    int available_platform_effects,
    int channels)
    : properties_(properties),
      platform_effects_(
          ApplyPropertiesToEffects(properties_, available_platform_effects)),
      webrtc_processing_settings_(
          ComputeWebrtcProcessingSettings(properties_,
                                          platform_effects_,
                                          channels > 1)) {}

bool MediaStreamAudioProcessingLayout::NeedWebrtcAudioProcessing() const {
  if (webrtc_processing_settings_.NeedWebrtcAudioProcessing()) {
    return true;
  }

// TODO(crbug.com/40205004): reconsider the logic below; see also
// AudioProcessingSettings::NeedWebrtcAudioProcessing().
#if !BUILDFLAG(IS_IOS)
  if (properties_.auto_gain_control) {
    return true;
  }
#endif

  if (properties_.noise_suppression) {
    return true;
  }

  return false;
}

bool MediaStreamAudioProcessingLayout::NoiseSuppressionInTandem() const {
  return (platform_effects_ & media::AudioParameters::NOISE_SUPPRESSION) &&
         webrtc_processing_settings_.noise_suppression;
}

bool MediaStreamAudioProcessingLayout::AutomaticGainControlInTandem() const {
  return (platform_effects_ & media::AudioParameters::AUTOMATIC_GAIN_CONTROL) &&
         webrtc_processing_settings_.automatic_gain_control;
}

}  // namespace blink
