// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_VIDEO_DECODER_CONFIG_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_VIDEO_DECODER_CONFIG_MOJOM_TRAITS_H_

#include "media/base/ipc/media_param_traits.h"
#include "media/base/video_decoder_config.h"
#include "media/mojo/mojom/hdr_metadata_mojom_traits.h"
#include "media/mojo/mojom/media_types.mojom.h"
#include "media/mojo/mojom/video_color_space_mojom_traits.h"
#include "media/mojo/mojom/video_transformation_mojom_traits.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<media::mojom::VideoDecoderConfigDataView,
                    media::VideoDecoderConfig> {
  static media::VideoCodec codec(const media::VideoDecoderConfig& input) {
    return input.codec();
  }

  static media::VideoCodecProfile profile(
      const media::VideoDecoderConfig& input) {
    return input.profile();
  }

  static bool has_alpha(const media::VideoDecoderConfig& input) {
    return input.alpha_mode() ==
           media::VideoDecoderConfig::AlphaMode::kHasAlpha;
  }

  static const gfx::Size& coded_size(const media::VideoDecoderConfig& input) {
    return input.coded_size();
  }

  static const gfx::Rect& visible_rect(const media::VideoDecoderConfig& input) {
    return input.visible_rect();
  }

  static const gfx::Size& natural_size(const media::VideoDecoderConfig& input) {
    return input.natural_size();
  }

  static const std::vector<uint8_t>& extra_data(
      const media::VideoDecoderConfig& input) {
    return input.extra_data();
  }

  static media::EncryptionScheme encryption_scheme(
      const media::VideoDecoderConfig& input) {
    return input.encryption_scheme();
  }

  static const media::VideoColorSpace& color_space_info(
      const media::VideoDecoderConfig& input) {
    return input.color_space_info();
  }

  static media::VideoTransformation transformation(
      const media::VideoDecoderConfig& input) {
    return input.video_transformation();
  }

  static const base::Optional<media::HDRMetadata>& hdr_metadata(
      const media::VideoDecoderConfig& input) {
    return input.hdr_metadata();
  }

  static bool Read(media::mojom::VideoDecoderConfigDataView input,
                   media::VideoDecoderConfig* output);
};

}  // namespace mojo

#endif  // MEDIA_MOJO_MOJOM_VIDEO_DECODER_CONFIG_MOJOM_TRAITS_H_
