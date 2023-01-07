// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_VIDEO_TRANSFORMATION_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_VIDEO_TRANSFORMATION_MOJOM_TRAITS_H_

#include "media/base/ipc/media_param_traits.h"
#include "media/base/video_transformation.h"
#include "media/mojo/mojom/media_types.mojom-shared.h"
#include "media/mojo/mojom/media_types_enum_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<media::mojom::VideoTransformationDataView,
                    media::VideoTransformation> {
  static media::VideoRotation rotation(
      const media::VideoTransformation& input) {
    return input.rotation;
  }

  static bool mirrored(const media::VideoTransformation& input) {
    return input.mirrored;
  }

  static bool Read(media::mojom::VideoTransformationDataView input,
                   media::VideoTransformation* output);
};

}  // namespace mojo

#endif  // MEDIA_MOJO_MOJOM_VIDEO_TRANSFORMATION_MOJOM_TRAITS_H_
