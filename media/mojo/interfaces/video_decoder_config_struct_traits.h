// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_INTERFACES_VIDEO_DECODER_CONFIG_STRUCT_TRAITS_H_
#define MEDIA_MOJO_INTERFACES_VIDEO_DECODER_CONFIG_STRUCT_TRAITS_H_

#include "media/base/ipc/media_param_traits.h"
#include "media/base/video_decoder_config.h"
#include "media/mojo/interfaces/encryption_scheme_struct_traits.h"
#include "media/mojo/interfaces/hdr_metadata_struct_traits.h"
#include "media/mojo/interfaces/media_types.mojom.h"
#include "media/mojo/interfaces/video_color_space_struct_traits.h"
#include "ui/gfx/geometry/mojo/geometry_struct_traits.h"

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

  static media::VideoPixelFormat format(
      const media::VideoDecoderConfig& input) {
    return input.format();
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

  static const media::EncryptionScheme& encryption_scheme(
      const media::VideoDecoderConfig& input) {
    return input.encryption_scheme();
  }

  static const media::VideoColorSpace& color_space_info(
      const media::VideoDecoderConfig& input) {
    return input.color_space_info();
  }

  static media::VideoRotation video_rotation(
      const media::VideoDecoderConfig& input) {
    return input.video_rotation();
  }

  static const base::Optional<media::HDRMetadata>& hdr_metadata(
      const media::VideoDecoderConfig& input) {
    return input.hdr_metadata();
  }

  static bool Read(media::mojom::VideoDecoderConfigDataView input,
                   media::VideoDecoderConfig* output);
};

}  // namespace mojo

#endif  // MEDIA_MOJO_INTERFACES_VIDEO_DECODER_CONFIG_STRUCT_TRAITS_H_
