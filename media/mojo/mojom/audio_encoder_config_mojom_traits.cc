// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/audio_encoder_config_mojom_traits.h"

#include "base/cxx17_backports.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/limits.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<media::mojom::AudioEncoderConfigDataView,
                  media::AudioEncoderConfig>::
    Read(media::mojom::AudioEncoderConfigDataView input,
         media::AudioEncoderConfig* output) {
  media::AudioCodec codec;
  if (!input.ReadCodec(&codec))
    return false;
  output->codec = codec;

  if (input.sample_rate() < media::limits::kMinSampleRate ||
      input.sample_rate() > media::limits::kMaxSampleRate)
    return false;
  output->sample_rate = input.sample_rate();

  if (input.bitrate() > 0)
    output->bitrate = base::saturated_cast<int>(input.bitrate());

  if (input.channel_count() > media::limits::kMaxChannels)
    return false;
  output->channels = input.channel_count();

  return true;
}

}  // namespace mojo
