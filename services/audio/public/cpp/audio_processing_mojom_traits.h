// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_PUBLIC_CPP_AUDIO_PROCESSING_MOJOM_TRAITS_H_
#define SERVICES_AUDIO_PUBLIC_CPP_AUDIO_PROCESSING_MOJOM_TRAITS_H_

#include "media/base/audio_processing.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/audio/public/mojom/audio_processing.mojom.h"

namespace mojo {

template <>
struct EnumTraits<audio::mojom::AutomaticGainControlType,
                  media::AutomaticGainControlType> {
  static audio::mojom::AutomaticGainControlType ToMojom(
      media::AutomaticGainControlType type) {
    switch (type) {
      case media::AutomaticGainControlType::kDisabled:
        return audio::mojom::AutomaticGainControlType::kDisabled;
      case media::AutomaticGainControlType::kDefault:
        return audio::mojom::AutomaticGainControlType::kDefault;
      case media::AutomaticGainControlType::kExperimental:
        return audio::mojom::AutomaticGainControlType::kExperimental;
      case media::AutomaticGainControlType::kHybridExperimental:
        return audio::mojom::AutomaticGainControlType::kHybridExperimental;
    }
    NOTREACHED();
    return audio::mojom::AutomaticGainControlType::kDisabled;
  }

  static bool FromMojom(audio::mojom::AutomaticGainControlType input,
                        media::AutomaticGainControlType* out) {
    switch (input) {
      case audio::mojom::AutomaticGainControlType::kDisabled:
        *out = media::AutomaticGainControlType::kDisabled;
        return true;
      case audio::mojom::AutomaticGainControlType::kDefault:
        *out = media::AutomaticGainControlType::kDefault;
        return true;
      case audio::mojom::AutomaticGainControlType::kExperimental:
        *out = media::AutomaticGainControlType::kExperimental;
        return true;
      case audio::mojom::AutomaticGainControlType::kHybridExperimental:
        *out = media::AutomaticGainControlType::kHybridExperimental;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

template <>
struct EnumTraits<audio::mojom::EchoCancellationType,
                  media::EchoCancellationType> {
  static audio::mojom::EchoCancellationType ToMojom(
      media::EchoCancellationType type) {
    switch (type) {
      case media::EchoCancellationType::kDisabled:
        return audio::mojom::EchoCancellationType::kDisabled;
      case media::EchoCancellationType::kAec3:
        return audio::mojom::EchoCancellationType::kAec3;
      case media::EchoCancellationType::kSystemAec:
        return audio::mojom::EchoCancellationType::kSystemAec;
    }
    NOTREACHED();
    return audio::mojom::EchoCancellationType::kDisabled;
  }

  static bool FromMojom(audio::mojom::EchoCancellationType input,
                        media::EchoCancellationType* out) {
    switch (input) {
      case audio::mojom::EchoCancellationType::kDisabled:
        *out = media::EchoCancellationType::kDisabled;
        return true;
      case audio::mojom::EchoCancellationType::kAec3:
        *out = media::EchoCancellationType::kAec3;
        return true;
      case audio::mojom::EchoCancellationType::kSystemAec:
        *out = media::EchoCancellationType::kSystemAec;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

template <>
struct EnumTraits<audio::mojom::NoiseSuppressionType,
                  media::NoiseSuppressionType> {
  static audio::mojom::NoiseSuppressionType ToMojom(
      media::NoiseSuppressionType type) {
    switch (type) {
      case media::NoiseSuppressionType::kDisabled:
        return audio::mojom::NoiseSuppressionType::kDisabled;
      case media::NoiseSuppressionType::kDefault:
        return audio::mojom::NoiseSuppressionType::kDefault;
      case media::NoiseSuppressionType::kExperimental:
        return audio::mojom::NoiseSuppressionType::kExperimental;
    }
    NOTREACHED();
    return audio::mojom::NoiseSuppressionType::kDisabled;
  }

  static bool FromMojom(audio::mojom::NoiseSuppressionType input,
                        media::NoiseSuppressionType* out) {
    switch (input) {
      case audio::mojom::NoiseSuppressionType::kDisabled:
        *out = media::NoiseSuppressionType::kDisabled;
        return true;
      case audio::mojom::NoiseSuppressionType::kDefault:
        *out = media::NoiseSuppressionType::kDefault;
        return true;
      case audio::mojom::NoiseSuppressionType::kExperimental:
        *out = media::NoiseSuppressionType::kExperimental;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

template <>
class StructTraits<audio::mojom::AudioProcessingSettingsDataView,
                   media::AudioProcessingSettings> {
 public:
  static media::EchoCancellationType echo_cancellation(
      const media::AudioProcessingSettings& s) {
    return s.echo_cancellation;
  }
  static media::NoiseSuppressionType noise_suppression(
      const media::AudioProcessingSettings& s) {
    return s.noise_suppression;
  }
  static media::AutomaticGainControlType automatic_gain_control(
      const media::AudioProcessingSettings& s) {
    return s.automatic_gain_control;
  }
  static bool high_pass_filter(const media::AudioProcessingSettings& s) {
    return s.high_pass_filter;
  }
  static bool typing_detection(const media::AudioProcessingSettings& s) {
    return s.typing_detection;
  }
  static bool stereo_mirroring(const media::AudioProcessingSettings& s) {
    return s.stereo_mirroring;
  }

  static bool Read(audio::mojom::AudioProcessingSettingsDataView data,
                   media::AudioProcessingSettings* out_settings);
};

template <>
class StructTraits<audio::mojom::AudioProcessingStatsDataView,
                   webrtc::AudioProcessorInterface::AudioProcessorStatistics> {
 public:
  static bool typing_noise_detected(
      const webrtc::AudioProcessorInterface::AudioProcessorStatistics& s) {
    return s.typing_noise_detected;
  }

  static bool has_echo_return_loss(
      const webrtc::AudioProcessorInterface::AudioProcessorStatistics& s) {
    return s.apm_statistics.echo_return_loss.has_value();
  }
  static double echo_return_loss(
      const webrtc::AudioProcessorInterface::AudioProcessorStatistics& s) {
    return s.apm_statistics.echo_return_loss.value_or(0);
  }

  static bool has_echo_return_loss_enhancement(
      const webrtc::AudioProcessorInterface::AudioProcessorStatistics& s) {
    return s.apm_statistics.echo_return_loss_enhancement.has_value();
  }
  static double echo_return_loss_enhancement(
      const webrtc::AudioProcessorInterface::AudioProcessorStatistics& s) {
    return s.apm_statistics.echo_return_loss_enhancement.value_or(0);
  }

  static bool has_divergent_filter_fraction(
      const webrtc::AudioProcessorInterface::AudioProcessorStatistics& s) {
    return s.apm_statistics.divergent_filter_fraction.has_value();
  }
  static double divergent_filter_fraction(
      const webrtc::AudioProcessorInterface::AudioProcessorStatistics& s) {
    return s.apm_statistics.divergent_filter_fraction.value_or(0);
  }

  static bool has_delay_median_ms(
      const webrtc::AudioProcessorInterface::AudioProcessorStatistics& s) {
    return s.apm_statistics.delay_median_ms.has_value();
  }
  static int32_t delay_median_ms(
      const webrtc::AudioProcessorInterface::AudioProcessorStatistics& s) {
    return s.apm_statistics.delay_median_ms.value_or(0);
  }

  static bool has_delay_standard_deviation_ms(
      const webrtc::AudioProcessorInterface::AudioProcessorStatistics& s) {
    return s.apm_statistics.delay_standard_deviation_ms.has_value();
  }
  static int32_t delay_standard_deviation_ms(
      const webrtc::AudioProcessorInterface::AudioProcessorStatistics& s) {
    return s.apm_statistics.delay_standard_deviation_ms.value_or(0);
  }

  static bool has_residual_echo_likelihood(
      const webrtc::AudioProcessorInterface::AudioProcessorStatistics& s) {
    return s.apm_statistics.residual_echo_likelihood.has_value();
  }
  static double residual_echo_likelihood(
      const webrtc::AudioProcessorInterface::AudioProcessorStatistics& s) {
    return s.apm_statistics.residual_echo_likelihood.value_or(0);
  }

  static bool has_residual_echo_likelihood_recent_max(
      const webrtc::AudioProcessorInterface::AudioProcessorStatistics& s) {
    return s.apm_statistics.residual_echo_likelihood_recent_max.has_value();
  }
  static double residual_echo_likelihood_recent_max(
      const webrtc::AudioProcessorInterface::AudioProcessorStatistics& s) {
    return s.apm_statistics.residual_echo_likelihood_recent_max.value_or(0);
  }

  static bool has_delay_ms(
      const webrtc::AudioProcessorInterface::AudioProcessorStatistics& s) {
    return s.apm_statistics.delay_ms.has_value();
  }
  static int32_t delay_ms(
      const webrtc::AudioProcessorInterface::AudioProcessorStatistics& s) {
    return s.apm_statistics.delay_ms.value_or(0);
  }

  static bool Read(
      audio::mojom::AudioProcessingStatsDataView data,
      webrtc::AudioProcessorInterface::AudioProcessorStatistics* out_stats);
};

}  // namespace mojo

#endif  // SERVICES_AUDIO_PUBLIC_CPP_AUDIO_PROCESSING_MOJOM_TRAITS_H_
