// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_AUDIO_PARAMETERS_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_AUDIO_PARAMETERS_MOJOM_TRAITS_H_

#include <optional>

#include "media/base/audio_latency.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_point.h"
#include "media/mojo/mojom/audio_parameters.mojom-shared.h"
#include "media/mojo/mojom/channel_layout_mojom_traits.h"
#include "media/mojo/mojom/media_types.mojom-shared.h"
#include "media/mojo/mojom/media_types_enum_mojom_traits.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"

namespace mojo {

template <>
struct EnumTraits<media::mojom::AudioParametersFormat,
                  media::AudioParameters::Format> {
  static media::mojom::AudioParametersFormat ToMojom(
      media::AudioParameters::Format input);
  static media::AudioParameters::Format FromMojom(
      media::mojom::AudioParametersFormat input);
};

template <>
struct EnumTraits<media::mojom::AudioLatencyType, media::AudioLatency::Type> {
  static media::mojom::AudioLatencyType ToMojom(
      media::AudioLatency::Type input);
  static media::AudioLatency::Type FromMojom(
      media::mojom::AudioLatencyType input);
};

template <>
struct StructTraits<media::mojom::AudioParametersHardwareCapabilitiesDataView,
                    media::AudioParameters::HardwareCapabilities> {
  static int32_t min_frames_per_buffer(
      const media::AudioParameters::HardwareCapabilities& input) {
    return input.min_frames_per_buffer;
  }
  static int32_t max_frames_per_buffer(
      const media::AudioParameters::HardwareCapabilities& input) {
    return input.max_frames_per_buffer;
  }
  static int32_t default_frames_per_buffer(
      const media::AudioParameters::HardwareCapabilities& input) {
    return input.default_frames_per_buffer;
  }
  static int32_t bitstream_formats(
      const media::AudioParameters::HardwareCapabilities& input) {
    return input.bitstream_formats;
  }
  static bool require_encapsulation(
      const media::AudioParameters::HardwareCapabilities& input) {
    return input.require_encapsulation;
  }
  static bool require_audio_offload(
      const media::AudioParameters::HardwareCapabilities& input) {
    return input.require_audio_offload;
  }

  static bool Read(
      media::mojom::AudioParametersHardwareCapabilitiesDataView input,
      media::AudioParameters::HardwareCapabilities* output);
};

template <>
struct StructTraits<media::mojom::AudioParametersDataView,
                    media::AudioParameters> {
  static media::AudioParameters::Format format(
      const media::AudioParameters& input) {
    return input.format();
  }
  static const media::ChannelLayoutConfig& channel_layout_config(
      const media::AudioParameters& input) {
    return input.channel_layout_config();
  }
  static int32_t sample_rate(const media::AudioParameters& input) {
    return input.sample_rate();
  }
  static int32_t frames_per_buffer(const media::AudioParameters& input) {
    return input.frames_per_buffer();
  }
  static uint32_t effects(const media::AudioParameters& input) {
    return input.effects();
  }
  static const std::vector<media::Point>& mic_positions(
      const media::AudioParameters& input) {
    return input.mic_positions();
  }
  static media::AudioLatency::Type latency_tag(
      const media::AudioParameters& input) {
    return input.latency_tag();
  }
  static std::optional<media::AudioParameters::HardwareCapabilities>
  hardware_capabilities(const media::AudioParameters& input) {
    return input.hardware_capabilities();
  }

  static bool Read(media::mojom::AudioParametersDataView input,
                   media::AudioParameters* output);
};

}  // namespace mojo

#endif  // MEDIA_MOJO_MOJOM_AUDIO_PARAMETERS_MOJOM_TRAITS_H_
