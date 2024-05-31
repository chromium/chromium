// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_SUPPORTED_AUDIO_DECODER_CONFIG_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_SUPPORTED_AUDIO_DECODER_CONFIG_MOJOM_TRAITS_H_

#include "media/base/supported_audio_decoder_config.h"
#include "media/mojo/mojom/audio_decoder.mojom.h"

namespace mojo {

template <>
struct StructTraits<media::mojom::SupportedAudioDecoderConfigDataView,
                    media::SupportedAudioDecoderConfig> {
  static media::AudioCodec codec(
      const media::SupportedAudioDecoderConfig& input) {
    return input.codec;
  }
  static media::AudioCodecProfile profile(
      const media::SupportedAudioDecoderConfig& input) {
    return input.profile;
  }
  static bool Read(media::mojom::SupportedAudioDecoderConfigDataView input,
                   media::SupportedAudioDecoderConfig* output);
};

}  // namespace mojo

#endif  // MEDIA_MOJO_MOJOM_SUPPORTED_AUDIO_DECODER_CONFIG_MOJOM_TRAITS_H_
