// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_VULKAN_YCBCR_INFO_H_
#define GPU_IPC_COMMON_VULKAN_YCBCR_INFO_H_

#include <stdint.h>
#include "gpu/gpu_export.h"

namespace gpu {

// Sampler Ycbcr conversion information.
struct GPU_EXPORT VulkanYCbCrInfo {
  VulkanYCbCrInfo();
  VulkanYCbCrInfo(uint32_t image_format,
                  uint64_t external_format,
                  uint32_t suggested_ycbcr_model,
                  uint32_t suggested_ycbcr_range,
                  uint32_t suggested_xchroma_offset,
                  uint32_t suggested_ychroma_offset,
                  uint32_t format_features);

  // Source image format.
  // Corresponds to vulkan type: VkFormat.
  uint32_t image_format;

  // Implementation-defined external format identifier for use with
  // VkExternalFormatANDROID.
  // This property is driver specific.
  uint64_t external_format;

  // Describes the color matrix for conversion between color models.
  // Corresponds to vulkan type: VkSamplerYcbcrModelConversion.
  uint32_t suggested_ycbcr_model;

  // Describes whether the encoded values have headroom and foot room, or
  // whether the encoding uses the full numerical range.
  // Corresponds to vulkan type: VkSamplerYcbcrRange.
  uint32_t suggested_ycbcr_range;

  // Describes the sample location associated with downsampled chroma channels
  // in the x dimension. It has no effect for formats in which chroma channels
  // are the same resolution as the luma channel.
  // Corresponds to vulkan type: VkChromaLocation.
  uint32_t suggested_xchroma_offset;

  // Describes the sample location associated with downsampled chroma channels
  // in the y dimension. It has no effect for formats in which chroma channels
  // are not downsampled vertically.
  // Corresponds to vulkan type: VkChromaLocation.
  uint32_t suggested_ychroma_offset;

  // Describes the capabilities of the format when used with an image bound to
  // memory imported from buffer. Must be set when for external-format image
  // created from the Android hardware buffer. For regular (not external) images
  // it can be set 0. Corresponds to Vulkan type: VkFormatFeatureFlags.
  uint32_t format_features;
};

}  // namespace gpu

#endif  // GPU_IPC_COMMON_VULKAN_YCBCR_INFO_H_
