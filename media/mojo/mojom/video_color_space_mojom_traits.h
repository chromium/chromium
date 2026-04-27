// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_VIDEO_COLOR_SPACE_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_VIDEO_COLOR_SPACE_MOJOM_TRAITS_H_

#include "media/base/video_color_space.h"
#include "media/mojo/mojom/media_types.mojom.h"

namespace mojo {

template <>
struct StructTraits<media::mojom::VideoColorSpaceDataView,
                    media::VideoColorSpace> {
  static media::VideoColorSpace::PrimaryID primaries(
      const media::VideoColorSpace& input) {
    return input.primaries;
  }
  static media::VideoColorSpace::TransferID transfer(
      const media::VideoColorSpace& input) {
    return input.transfer;
  }
  static media::VideoColorSpace::MatrixID matrix(
      const media::VideoColorSpace& input) {
    return input.matrix;
  }
  static gfx::ColorSpace::RangeID range(const media::VideoColorSpace& input) {
    return input.range;
  }

  static bool Read(media::mojom::VideoColorSpaceDataView data,
                   media::VideoColorSpace* output) {
    output->primaries =
        static_cast<media::VideoColorSpace::PrimaryID>(data.primaries());
    output->transfer =
        static_cast<media::VideoColorSpace::TransferID>(data.transfer());
    output->matrix =
        static_cast<media::VideoColorSpace::MatrixID>(data.matrix());
    output->range = static_cast<gfx::ColorSpace::RangeID>(data.range());
    return true;
  }
};

}  // namespace mojo

#endif  // MEDIA_MOJO_MOJOM_VIDEO_COLOR_SPACE_MOJOM_TRAITS_H_
