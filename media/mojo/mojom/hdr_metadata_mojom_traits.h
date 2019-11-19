// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_HDR_METADATA_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_HDR_METADATA_MOJOM_TRAITS_H_

#include "media/base/hdr_metadata.h"
#include "media/mojo/mojom/media_types.mojom.h"

namespace mojo {

template <>
struct StructTraits<media::mojom::MasteringMetadataDataView,
                    media::MasteringMetadata> {
  static gfx::PointF primary_r(const media::MasteringMetadata& input) {
    return input.primary_r;
  }
  static gfx::PointF primary_g(const media::MasteringMetadata& input) {
    return input.primary_g;
  }
  static gfx::PointF primary_b(const media::MasteringMetadata& input) {
    return input.primary_b;
  }
  static gfx::PointF white_point(const media::MasteringMetadata& input) {
    return input.white_point;
  }
  static float luminance_max(const media::MasteringMetadata& input) {
    return input.luminance_max;
  }
  static float luminance_min(const media::MasteringMetadata& input) {
    return input.luminance_min;
  }

  static bool Read(media::mojom::MasteringMetadataDataView data,
                   media::MasteringMetadata* output) {
    output->luminance_max = data.luminance_max();
    output->luminance_min = data.luminance_min();
    if (!data.ReadPrimaryR(&output->primary_r))
      return false;
    if (!data.ReadPrimaryG(&output->primary_g))
      return false;
    if (!data.ReadPrimaryB(&output->primary_b))
      return false;
    if (!data.ReadWhitePoint(&output->white_point))
      return false;
    return true;
  }
};

template <>
struct StructTraits<media::mojom::HDRMetadataDataView, media::HDRMetadata> {
  static unsigned max_content_light_level(const media::HDRMetadata& input) {
    return input.max_content_light_level;
  }
  static unsigned max_frame_average_light_level(
      const media::HDRMetadata& input) {
    return input.max_frame_average_light_level;
  }
  static media::MasteringMetadata mastering_metadata(
      const media::HDRMetadata& input) {
    return input.mastering_metadata;
  }

  static bool Read(media::mojom::HDRMetadataDataView data,
                   media::HDRMetadata* output) {
    output->max_content_light_level = data.max_content_light_level();
    output->max_frame_average_light_level =
        data.max_frame_average_light_level();
    if (!data.ReadMasteringMetadata(&output->mastering_metadata))
      return false;
    return true;
  }
};

}  // namespace mojo

#endif  // MEDIA_MOJO_MOJOM_HDR_METADATA_MOJOM_TRAITS_H_
