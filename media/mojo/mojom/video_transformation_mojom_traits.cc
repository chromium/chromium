// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/video_transformation_mojom_traits.h"

#include "media/mojo/mojom/media_types.mojom.h"

namespace mojo {

// static
bool StructTraits<media::mojom::VideoTransformationDataView,
                  media::VideoTransformation>::
    Read(media::mojom::VideoTransformationDataView input,
         media::VideoTransformation* output) {
  if (!input.ReadRotation(&output->rotation))
    return false;

  output->mirrored = input.mirrored();

  return true;
}

}  // namespace mojo
