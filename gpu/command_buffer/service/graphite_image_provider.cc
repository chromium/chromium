// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/graphite_image_provider.h"
#include "third_party/skia/include/gpu/graphite/Image.h"

namespace gpu {

sk_sp<SkImage> GraphiteImageProvider::findOrCreate(
    skgpu::graphite::Recorder* recorder,
    const SkImage* image,
    SkImage::RequiredProperties requiredProps) {
  return SkImages::TextureFromImage(recorder, image, requiredProps);
}

}  // namespace gpu
