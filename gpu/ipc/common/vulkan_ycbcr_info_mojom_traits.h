// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_VULKAN_YCBCR_INFO_MOJOM_TRAITS_H_
#define GPU_IPC_COMMON_VULKAN_YCBCR_INFO_MOJOM_TRAITS_H_

#include "gpu/ipc/common/gpu_ipc_common_export.h"
#include "gpu/ipc/common/vulkan_ycbcr_info.mojom-shared.h"
#include "gpu/vulkan/vulkan_ycbcr_info.h"

namespace mojo {

template <>
struct GPU_IPC_COMMON_EXPORT StructTraits<gpu::mojom::VulkanYCbCrInfoDataView,
                                          gpu::VulkanYCbCrInfo> {
  static uint32_t image_format(const gpu::VulkanYCbCrInfo& info) {
    return info.image_format;
  }

  static uint64_t external_format(const gpu::VulkanYCbCrInfo& info) {
    return info.external_format;
  }

  static uint32_t suggested_ycbcr_model(const gpu::VulkanYCbCrInfo& info) {
    return info.suggested_ycbcr_model;
  }

  static uint32_t suggested_ycbcr_range(const gpu::VulkanYCbCrInfo& info) {
    return info.suggested_ycbcr_range;
  }

  static uint32_t suggested_xchroma_offset(const gpu::VulkanYCbCrInfo& info) {
    return info.suggested_xchroma_offset;
  }

  static uint32_t suggested_ychroma_offset(const gpu::VulkanYCbCrInfo& info) {
    return info.suggested_ychroma_offset;
  }

  static uint32_t format_features(const gpu::VulkanYCbCrInfo& info) {
    return info.format_features;
  }

  static bool Read(gpu::mojom::VulkanYCbCrInfoDataView data,
                   gpu::VulkanYCbCrInfo* out) {
    out->image_format = data.image_format();
    out->external_format = data.external_format();
    out->suggested_ycbcr_model = data.suggested_ycbcr_model();
    out->suggested_ycbcr_range = data.suggested_ycbcr_range();
    out->suggested_xchroma_offset = data.suggested_xchroma_offset();
    out->suggested_ychroma_offset = data.suggested_ychroma_offset();
    out->format_features = data.format_features();

    // Values from Vulkan definitions, because we can't easy depend on vulkan
    // here.
    // https://source.chromium.org/chromium/chromium/src/+/main:third_party/vulkan-headers/src/include/vulkan/vulkan_core.h;drc=f6a6f7ab165cedbfa2a7d0c93fe27a2d01ce09c8;l=5438
    const uint32_t VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_2020 = 4;
    const uint32_t VK_CHROMA_LOCATION_MIDPOINT = 1;
    const uint32_t VK_SAMPLER_YCBCR_RANGE_ITU_NARROW = 1;

    if (out->suggested_ycbcr_model >
            VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_2020 ||
        out->suggested_ycbcr_range > VK_SAMPLER_YCBCR_RANGE_ITU_NARROW ||
        out->suggested_xchroma_offset > VK_CHROMA_LOCATION_MIDPOINT ||
        out->suggested_ychroma_offset > VK_CHROMA_LOCATION_MIDPOINT) {
      return false;
    }

    return true;
  }
};

}  // namespace mojo

#endif  // GPU_IPC_COMMON_VULKAN_YCBCR_INFO_MOJOM_TRAITS_H_
