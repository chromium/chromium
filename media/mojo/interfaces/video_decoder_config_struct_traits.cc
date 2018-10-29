// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/interfaces/video_decoder_config_struct_traits.h"

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

  media::VideoPixelFormat format;
  if (!input.ReadFormat(&format))
    return false;

  media::ColorSpace color_space = media::ColorSpace::COLOR_SPACE_UNSPECIFIED;

  media::VideoRotation rotation;
  if (!input.ReadVideoRotation(&rotation))
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

  std::vector<uint8_t> extra_data;
  if (!input.ReadExtraData(&extra_data))
    return false;

  media::EncryptionScheme encryption_scheme;
  if (!input.ReadEncryptionScheme(&encryption_scheme))
    return false;

  media::VideoColorSpace color_space_info;
  if (!input.ReadColorSpaceInfo(&color_space_info))
    return false;

  base::Optional<media::HDRMetadata> hdr_metadata;
  if (!input.ReadHdrMetadata(&hdr_metadata))
    return false;

  output->Initialize(codec, profile, format, color_space, rotation, coded_size,
                     visible_rect, natural_size, extra_data, encryption_scheme);

  output->set_color_space_info(color_space_info);

  if (hdr_metadata)
    output->set_hdr_metadata(hdr_metadata.value());

  if (!output->IsValidConfig())
    return false;

  return true;
}

}  // namespace mojo
