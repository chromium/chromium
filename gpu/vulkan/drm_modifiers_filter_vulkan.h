// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_DRM_MODIFIERS_FILTER_VULKAN_H_
#define GPU_VULKAN_DRM_MODIFIERS_FILTER_VULKAN_H_

#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "ui/gfx/buffer_types.h"
#include "ui/ozone/public/drm_modifiers_filter.h"

namespace gpu {

// DRM modifiers filter object that lets clients filter out modifiers that are
// not supported for Vulkan import. The list of modifiers that the Vulkan
// implementation can import is a subset of all modifiers supported by the
// hardware and can be queried through the VK_EXT_image_drm_format_modifier
// extension.
class COMPONENT_EXPORT(VULKAN) DrmModifiersFilterVulkan
    : public ui::DrmModifiersFilter {
 public:
  explicit DrmModifiersFilterVulkan(
      raw_ptr<gpu::VulkanImplementation> vulkan_implementation);

  ~DrmModifiersFilterVulkan() override;

  std::vector<uint64_t> Filter(gfx::BufferFormat format,
                               const std::vector<uint64_t>& modifiers) override;

 private:
  raw_ptr<gpu::VulkanImplementation> vulkan_implementation_;
};

}  // namespace gpu

#endif  // GPU_VULKAN_DRM_MODIFIERS_FILTER_VULKAN_H_
