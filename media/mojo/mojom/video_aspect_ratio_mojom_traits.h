// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_VIDEO_ASPECT_RATIO_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_VIDEO_ASPECT_RATIO_MOJOM_TRAITS_H_

#include "media/base/video_aspect_ratio.h"
#include "media/mojo/mojom/media_types.mojom.h"

namespace mojo {

template <>
struct StructTraits<media::mojom::VideoAspectRatioDataView,
                    media::VideoAspectRatio> {
  static media::mojom::VideoAspectRatio_Type type(
      const media::VideoAspectRatio& input) {
    switch (input.type_) {
      case media::VideoAspectRatio::Type::kDisplay:
        return media::mojom::VideoAspectRatio_Type::kDisplay;
      case media::VideoAspectRatio::Type::kPixel:
        return media::mojom::VideoAspectRatio_Type::kPixel;
    }
  }

  static double value(const media::VideoAspectRatio& input) {
    return input.aspect_ratio_;
  }

  static bool Read(media::mojom::VideoAspectRatioDataView data,
                   media::VideoAspectRatio* output) {
    switch (data.type()) {
      case media::mojom::VideoAspectRatio_Type::kDisplay:
        output->type_ = media::VideoAspectRatio::Type::kDisplay;
        break;
      case media::mojom::VideoAspectRatio_Type::kPixel:
        output->type_ = media::VideoAspectRatio::Type::kPixel;
        break;
    }
    output->aspect_ratio_ = data.value();
    return true;
  }
};

}  // namespace mojo

#endif  // MEDIA_MOJO_MOJOM_VIDEO_ASPECT_RATIO_MOJOM_TRAITS_H_
