// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_SUPPORTED_VIDEO_DECODER_CONFIG_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_SUPPORTED_VIDEO_DECODER_CONFIG_MOJOM_TRAITS_H_

#include "media/base/ipc/media_param_traits.h"
#include "media/mojo/mojom/media_types.mojom.h"
#include "media/mojo/mojom/video_decoder.mojom.h"
#include "media/video/supported_video_decoder_config.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<media::mojom::SupportedVideoDecoderConfigDataView,
                    media::SupportedVideoDecoderConfig> {
  static media::VideoCodecProfile profile_min(
      const media::SupportedVideoDecoderConfig& input) {
    return input.profile_min;
  }

  static media::VideoCodecProfile profile_max(
      const media::SupportedVideoDecoderConfig& input) {
    return input.profile_max;
  }

  static const gfx::Size& coded_size_min(
      const media::SupportedVideoDecoderConfig& input) {
    return input.coded_size_min;
  }

  static const gfx::Size& coded_size_max(
      const media::SupportedVideoDecoderConfig& input) {
    return input.coded_size_max;
  }

  static bool allow_encrypted(const media::SupportedVideoDecoderConfig& input) {
    return input.allow_encrypted;
  }

  static bool require_encrypted(
      const media::SupportedVideoDecoderConfig& input) {
    return input.require_encrypted;
  }

  static bool Read(media::mojom::SupportedVideoDecoderConfigDataView input,
                   media::SupportedVideoDecoderConfig* output);
};

}  // namespace mojo

#endif  // MEDIA_MOJO_MOJOM_SUPPORTED_VIDEO_DECODER_CONFIG_MOJOM_TRAITS_H_
