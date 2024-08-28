// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/video_decoder_config_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<media::mojom::VideoDecoderConfigDataView,
                  media::VideoDecoderConfig>::
    Read(media::mojom::VideoDecoderConfigDataView input,
         media::VideoDecoderConfig* output) {
  media::VideoCodec codec;
  if (!input.ReadCodec(&codec))
    return false;

  media::VideoCodecProfile profile;
  if (!input.ReadProfile(&profile))
    return false;

  media::VideoTransformation transformation;
  if (!input.ReadTransformation(&transformation))
    return false;

  gfx::Size coded_size;
  if (!input.ReadCodedSize(&coded_size))
    return false;

  gfx::Rect visible_rect;
  if (!input.ReadVisibleRect(&visible_rect))
    return false;

  gfx::Size natural_size;
  if (!input.ReadNaturalSize(&natural_size))
    return false;

  media::VideoAspectRatio aspect_ratio;
  if (!input.ReadAspectRatio(&aspect_ratio)) {
    return false;
  }

  std::vector<uint8_t> extra_data;
  if (!input.ReadExtraData(&extra_data))
    return false;

  media::EncryptionScheme encryption_scheme;
  if (!input.ReadEncryptionScheme(&encryption_scheme))
    return false;

  media::VideoColorSpace color_space;
  if (!input.ReadColorSpaceInfo(&color_space))
    return false;

  std::optional<gfx::HDRMetadata> hdr_metadata;
  if (!input.ReadHdrMetadata(&hdr_metadata))
    return false;

  output->Initialize(codec, profile,
                     input.has_alpha()
                         ? media::VideoDecoderConfig::AlphaMode::kHasAlpha
                         : media::VideoDecoderConfig::AlphaMode::kIsOpaque,
                     color_space, transformation, coded_size, visible_rect,
                     natural_size, extra_data, encryption_scheme);

  output->set_level(input.level());

  output->set_aspect_ratio(aspect_ratio);

  if (hdr_metadata)
    output->set_hdr_metadata(hdr_metadata.value());

  if (!output->IsValidConfig())
    return false;

  return true;
}

}  // namespace mojo
