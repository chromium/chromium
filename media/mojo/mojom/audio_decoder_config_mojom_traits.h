// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_AUDIO_DECODER_CONFIG_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_AUDIO_DECODER_CONFIG_MOJOM_TRAITS_H_

#include "media/base/audio_decoder_config.h"
#include "media/base/ipc/media_param_traits.h"
#include "media/mojo/mojom/media_types.mojom.h"

namespace mojo {

template <>
struct StructTraits<media::mojom::AudioDecoderConfigDataView,
                    media::AudioDecoderConfig> {
  static media::AudioCodec codec(const media::AudioDecoderConfig& input) {
    return input.codec();
  }

  static media::SampleFormat sample_format(
      const media::AudioDecoderConfig& input) {
    return input.sample_format();
  }

  static media::ChannelLayout channel_layout(
      const media::AudioDecoderConfig& input) {
    return input.channel_layout();
  }

  static int samples_per_second(const media::AudioDecoderConfig& input) {
    return input.samples_per_second();
  }

  static const std::vector<uint8_t>& extra_data(
      const media::AudioDecoderConfig& input) {
    return input.extra_data();
  }

  static base::TimeDelta seek_preroll(const media::AudioDecoderConfig& input) {
    return input.seek_preroll();
  }

  static int codec_delay(const media::AudioDecoderConfig& input) {
    return input.codec_delay();
  }

  static media::EncryptionScheme encryption_scheme(
      const media::AudioDecoderConfig& input) {
    return input.encryption_scheme();
  }

  static bool Read(media::mojom::AudioDecoderConfigDataView input,
                   media::AudioDecoderConfig* output);
};

}  // namespace mojo

#endif  // MEDIA_MOJO_MOJOM_AUDIO_DECODER_CONFIG_MOJOM_TRAITS_H_
