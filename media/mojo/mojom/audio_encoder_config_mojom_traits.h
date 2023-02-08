// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_AUDIO_ENCODER_CONFIG_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_AUDIO_ENCODER_CONFIG_MOJOM_TRAITS_H_

#include "media/base/audio_encoder.h"
#include "media/base/ipc/media_param_traits.h"
#include "media/mojo/mojom/audio_encoder.mojom-shared.h"
#include "media/mojo/mojom/media_types.mojom.h"

namespace mojo {

template <>
struct EnumTraits<media::mojom::AacOutputFormat,
                  media::AudioEncoder::AacOutputFormat> {
  static media::mojom::AacOutputFormat ToMojom(
      media::AudioEncoder::AacOutputFormat input);

  static bool FromMojom(media::mojom::AacOutputFormat,
                        media::AudioEncoder::AacOutputFormat* output);
};

template <>
struct StructTraits<media::mojom::AacAudioEncoderConfigDataView,
                    media::AudioEncoder::AacOptions> {
  static media::AudioEncoder::AacOutputFormat format(
      media::AudioEncoder::AacOptions& input) {
    return input.format;
  }

  static bool Read(media::mojom::AacAudioEncoderConfigDataView input,
                   media::AudioEncoder::AacOptions* output);
};

template <>
struct StructTraits<media::mojom::AudioEncoderConfigDataView,
                    media::AudioEncoderConfig> {
  static media::AudioCodec codec(const media::AudioEncoderConfig& input) {
    return input.codec;
  }

  static uint8_t channel_count(const media::AudioEncoderConfig& input) {
    return input.channels;
  }

  static uint32_t sample_rate(const media::AudioEncoderConfig& input) {
    return input.sample_rate;
  }

  static uint32_t bitrate(const media::AudioEncoderConfig& input) {
    return input.bitrate.value_or(0);
  }

  static media::AudioEncoder::AacOptions aac(
      const media::AudioEncoderConfig& input) {
    return input.aac.value_or(media::AudioEncoder::AacOptions());
  }

  static bool Read(media::mojom::AudioEncoderConfigDataView input,
                   media::AudioEncoderConfig* output);
};

}  // namespace mojo

#endif  // MEDIA_MOJO_MOJOM_AUDIO_ENCODER_CONFIG_MOJOM_TRAITS_H_
