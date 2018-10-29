// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_SHARED_IMAGE_USAGE_H_
#define GPU_COMMAND_BUFFER_COMMON_SHARED_IMAGE_USAGE_H_

#include <stdint.h>

namespace gpu {

enum SharedImageUsage : uint32_t {
  // Image will be used in GLES2Interface
  SHARED_IMAGE_USAGE_GLES2 = 1 << 0,
  // Image will be used as a framebuffer (hint)
  SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT = 1 << 1,
  // Image will be used in RasterInterface
  SHARED_IMAGE_USAGE_RASTER = 1 << 2,
  // Image will be used in Display
  SHARED_IMAGE_USAGE_DISPLAY = 1 << 3,
  // Image will be used as a scanout buffer (overlay)
  SHARED_IMAGE_USAGE_SCANOUT = 1 << 4,
  // Image will be used in OOP rasterization
  // TODO(backer): Fold back into SHARED_IMAGE_USAGE_RASTER once RasterInterface
  // can CPU raster (CopySubImage?) to SkImage.
  SHARED_IMAGE_USAGE_OOP_RASTERIZATION = 1 << 5,
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_SHARED_IMAGE_USAGE_H_
