// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_SHARED_IMAGE_CAPABILITIES_H_
#define GPU_COMMAND_BUFFER_COMMON_SHARED_IMAGE_CAPABILITIES_H_

#include <stdint.h>

#include "build/build_config.h"
#include "gpu/gpu_export.h"

namespace gpu {

struct GPU_EXPORT SharedImageCapabilities {
  SharedImageCapabilities();
  SharedImageCapabilities(const SharedImageCapabilities& other);
  ~SharedImageCapabilities();

  bool supports_scanout_shared_images = false;

#if BUILDFLAG(IS_WIN)
  // On Windows, overlays are in general not supported. However, in some cases
  // they are supported for the software video frame use case in particular.
  // This cap details whether that support is present.
  bool supports_scanout_shared_images_for_software_video_frames = false;
#endif

  bool supports_luminance_shared_images = false;
  bool supports_r16_shared_images = false;
  bool supports_native_nv12_mappable_shared_images = false;
  bool is_r16f_supported = false;
  bool disable_r8_shared_images = false;
  bool disable_webgpu_shared_images = false;

  bool shared_image_d3d = false;
  bool shared_image_swap_chain = false;

#if BUILDFLAG(IS_MAC)
  uint32_t texture_target_for_io_surfaces = 0;
#endif
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_SHARED_IMAGE_CAPABILITIES_H_
