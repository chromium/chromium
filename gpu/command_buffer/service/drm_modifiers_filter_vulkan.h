// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_DRM_MODIFIERS_FILTER_VULKAN_H_
#define GPU_COMMAND_BUFFER_SERVICE_DRM_MODIFIERS_FILTER_VULKAN_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/gpu_gles2_export.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "ui/ozone/public/drm_modifiers_filter.h"

namespace gpu {

// Populate the map of fourcc formats to the list of modifiers supported by that
// format on Vulkan.
GPU_GLES2_EXPORT void PopulateVkDrmFormatsAndModifiers(
    VulkanDeviceQueue* device_queue,
    base::flat_map<uint32_t, std::vector<uint64_t>>& fourcc_modifier_map);

// DRM modifiers filter object that lets clients filter out modifiers that are
// not supported for Vulkan import. The list of modifiers that the Vulkan
// implementation can import is a subset of all modifiers supported by the
// hardware and can be queried through the VK_EXT_image_drm_format_modifier
// extension.
class GPU_GLES2_EXPORT DrmModifiersFilterVulkan
    : public ui::DrmModifiersFilter {
 public:
  explicit DrmModifiersFilterVulkan(
      raw_ptr<gpu::VulkanImplementation> vulkan_implementation);

  ~DrmModifiersFilterVulkan() override;

  std::vector<uint64_t> Filter(viz::SharedImageFormat format,
                               const std::vector<uint64_t>& modifiers) override;

 private:
  raw_ptr<gpu::VulkanImplementation> vulkan_implementation_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_DRM_MODIFIERS_FILTER_VULKAN_H_
