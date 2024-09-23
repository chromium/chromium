// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/audio_encoder_config_mojom_traits.h"

#include <algorithm>

#include "base/numerics/safe_conversions.h"
#include "media/base/limits.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"

namespace mojo {

// static
media::mojom::AacOutputFormat EnumTraits<media::mojom::AacOutputFormat,
                                         media::AudioEncoder::AacOutputFormat>::
    ToMojom(media::AudioEncoder::AacOutputFormat input) {
  switch (input) {
    case media::AudioEncoder::AacOutputFormat::ADTS:
      return media::mojom::AacOutputFormat::kADTS;
    case media::AudioEncoder::AacOutputFormat::AAC:
      return media::mojom::AacOutputFormat::kAAC;
  }
  NOTREACHED();
}

// static
bool EnumTraits<media::mojom::AacOutputFormat,
                media::AudioEncoder::AacOutputFormat>::
    FromMojom(media::mojom::AacOutputFormat format,
              media::AudioEncoder::AacOutputFormat* output) {
  switch (format) {
    case media::mojom::AacOutputFormat::kADTS:
      *output = media::AudioEncoder::AacOutputFormat::ADTS;
      return true;
    case media::mojom::AacOutputFormat::kAAC:
      *output = media::AudioEncoder::AacOutputFormat::AAC;
      return true;
  }
  NOTREACHED();
}

// static
bool StructTraits<media::mojom::AacAudioEncoderConfigDataView,
                  media::AudioEncoder::AacOptions>::
    Read(media::mojom::AacAudioEncoderConfigDataView input,
         media::AudioEncoder::AacOptions* output) {
  media::AudioEncoder::AacOutputFormat format;
  if (!input.ReadFormat(&format)) {
    return false;
  }

  output->format = format;
  return true;
}

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

  media::AudioEncoder::AacOptions aac;
  if (!input.ReadAac(&aac)) {
    return false;
  }
  output->aac = aac;

  return true;
}

}  // namespace mojo
