// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_SHARED_IMAGE_CAPABILITIES_MOJOM_TRAITS_H_
#define GPU_IPC_COMMON_SHARED_IMAGE_CAPABILITIES_MOJOM_TRAITS_H_

#include "build/build_config.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/shared_image_capabilities.mojom.h"

namespace mojo {

template <>
struct GPU_EXPORT StructTraits<gpu::mojom::SharedImageCapabilitiesDataView,
                               gpu::SharedImageCapabilities> {
  static bool Read(gpu::mojom::SharedImageCapabilitiesDataView data,
                   gpu::SharedImageCapabilities* out);

  static bool supports_scanout_shared_images(
      const gpu::SharedImageCapabilities& input) {
    return input.supports_scanout_shared_images;
  }

#if BUILDFLAG(IS_WIN)
  static bool supports_scanout_shared_images_for_software_video_frames(
      const gpu::SharedImageCapabilities& input) {
    return input.supports_scanout_shared_images_for_software_video_frames;
  }
#endif

  static bool supports_luminance_shared_images(
      const gpu::SharedImageCapabilities& input) {
    return input.supports_luminance_shared_images;
  }

  static bool supports_r16_shared_images(
      const gpu::SharedImageCapabilities& input) {
    return input.supports_r16_shared_images;
  }

  static bool supports_native_nv12_mappable_shared_images(
      const gpu::SharedImageCapabilities& input) {
    return input.supports_native_nv12_mappable_shared_images;
  }

  static bool is_r16f_supported(const gpu::SharedImageCapabilities& input) {
    return input.is_r16f_supported;
  }

  static bool disable_r8_shared_images(
      const gpu::SharedImageCapabilities& input) {
    return input.disable_r8_shared_images;
  }

  static bool disable_webgpu_shared_images(
      const gpu::SharedImageCapabilities& input) {
    return input.disable_webgpu_shared_images;
  }

  static bool shared_image_d3d(const gpu::SharedImageCapabilities& input) {
    return input.shared_image_d3d;
  }

  static bool shared_image_swap_chain(
      const gpu::SharedImageCapabilities& input) {
    return input.shared_image_swap_chain;
  }

#if BUILDFLAG(IS_MAC)
  static uint32_t texture_target_for_io_surfaces(
      const gpu::SharedImageCapabilities& input) {
    return input.texture_target_for_io_surfaces;
  }
#endif
};

}  // namespace mojo
#endif  // GPU_IPC_COMMON_SHARED_IMAGE_CAPABILITIES_MOJOM_TRAITS_H_
