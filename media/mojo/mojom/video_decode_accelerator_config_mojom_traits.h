// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_VIDEO_DECODE_ACCELERATOR_CONFIG_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_VIDEO_DECODE_ACCELERATOR_CONFIG_MOJOM_TRAITS_H_

#include "media/base/video_decoder_config.h"
#include "media/mojo/mojom/gpu_accelerated_video_decoder.mojom.h"
#include "media/mojo/mojom/media_types.mojom.h"
#include "media/video/video_decode_accelerator.h"

namespace mojo {

template <>
struct StructTraits<media::mojom::VideoDecodeAcceleratorConfigDataView,
                    media::VideoDecodeAccelerator::Config> {
  static media::VideoCodecProfile profile(
      const media::VideoDecodeAccelerator::Config& input) {
    return input.profile;
  }

  static media::EncryptionScheme encryption_scheme(
      const media::VideoDecodeAccelerator::Config& input) {
    return input.encryption_scheme;
  }

  static absl::optional<base::UnguessableToken> cdm_id(
      const media::VideoDecodeAccelerator::Config& input) {
    return input.cdm_id;
  }

  static bool is_deferred_initialization_allowed(
      const media::VideoDecodeAccelerator::Config& input) {
    return input.is_deferred_initialization_allowed;
  }

  static media::OverlayInfo overlay_info(
      const media::VideoDecodeAccelerator::Config& input) {
    return input.overlay_info;
  }

  static gfx::Size initial_expected_coded_size(
      const media::VideoDecodeAccelerator::Config& input) {
    return input.initial_expected_coded_size;
  }

  static std::vector<media::VideoPixelFormat> supported_output_formats(
      const media::VideoDecodeAccelerator::Config& input) {
    return input.supported_output_formats;
  }

  static std::vector<uint8_t> sps(
      const media::VideoDecodeAccelerator::Config& input) {
    return input.sps;
  }

  static std::vector<uint8_t> pps(
      const media::VideoDecodeAccelerator::Config& input) {
    return input.pps;
  }

  static media::VideoColorSpace container_color_space(
      const media::VideoDecodeAccelerator::Config& input) {
    return input.container_color_space;
  }

  static gfx::ColorSpace target_color_space(
      const media::VideoDecodeAccelerator::Config& input) {
    return input.target_color_space;
  }

  static absl::optional<gfx::HDRMetadata> hdr_metadata(
      const media::VideoDecodeAccelerator::Config& input) {
    return input.hdr_metadata;
  }

  static bool Read(media::mojom::VideoDecodeAcceleratorConfigDataView input,
                   media::VideoDecodeAccelerator::Config* output);
};

}  // namespace mojo

#endif  // MEDIA_MOJO_MOJOM_VIDEO_DECODE_ACCELERATOR_CONFIG_MOJOM_TRAITS_H_
