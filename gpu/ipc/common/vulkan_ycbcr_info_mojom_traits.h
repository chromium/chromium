// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_VULKAN_YCBCR_INFO_MOJOM_TRAITS_H_
#define GPU_IPC_COMMON_VULKAN_YCBCR_INFO_MOJOM_TRAITS_H_

#include "gpu/ipc/common/vulkan_ycbcr_info.h"
#include "gpu/ipc/common/vulkan_ycbcr_info.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<gpu::mojom::VulkanYCbCrInfoDataView, gpu::VulkanYCbCrInfo> {
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
    return true;
  }
};

}  // namespace mojo

#endif  // GPU_IPC_COMMON_VULKAN_YCBCR_INFO_MOJOM_TRAITS_H_
