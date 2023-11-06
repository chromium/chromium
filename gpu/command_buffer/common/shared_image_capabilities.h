// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_SHARED_IMAGE_CAPABILITIES_H_
#define GPU_COMMAND_BUFFER_COMMON_SHARED_IMAGE_CAPABILITIES_H_

#include "gpu/gpu_export.h"

namespace gpu {

struct GPU_EXPORT SharedImageCapabilities {
  bool supports_scanout_shared_images = false;
  bool supports_luminance_shared_images = false;
  bool supports_r16_shared_images = false;
  bool disable_r8_shared_images = false;

  bool shared_image_d3d = false;
  bool shared_image_swap_chain = false;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_SHARED_IMAGE_CAPABILITIES_H_
