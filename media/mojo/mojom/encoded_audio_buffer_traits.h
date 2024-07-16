// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_ENCODED_AUDIO_BUFFER_TRAITS_H_
#define MEDIA_MOJO_MOJOM_ENCODED_AUDIO_BUFFER_TRAITS_H_

#include "media/base/audio_encoder.h"
#include "media/base/ipc/media_param_traits.h"
#include "media/mojo/mojom/audio_encoder.mojom.h"
#include "media/mojo/mojom/media_types.mojom.h"

namespace mojo {

template <>
struct StructTraits<media::mojom::EncodedAudioBufferDataView,
                    media::EncodedAudioBuffer> {
  static const base::span<const uint8_t> data(
      const media::EncodedAudioBuffer& input) {
    if (input.encoded_data.empty()) {
      return base::span<const uint8_t>();
    }

    return input.encoded_data;
  }

  static const base::TimeDelta timestamp(
      const media::EncodedAudioBuffer& input) {
    return input.timestamp - base::TimeTicks();
  }

  static const base::TimeDelta duration(
      const media::EncodedAudioBuffer& input) {
    return input.duration;
  }

  static const media::AudioParameters params(
      const media::EncodedAudioBuffer& input) {
    return input.params;
  }

  static bool Read(media::mojom::EncodedAudioBufferDataView input,
                   media::EncodedAudioBuffer* output);
};

}  // namespace mojo

#endif  // MEDIA_MOJO_MOJOM_ENCODED_AUDIO_BUFFER_TRAITS_H_
