// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_DRM_MODIFIERS_FILTER_DAWN_H_
#define GPU_COMMAND_BUFFER_SERVICE_DRM_MODIFIERS_FILTER_DAWN_H_

#include "base/containers/flat_map.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gfx/buffer_types.h"
#include "ui/ozone/public/drm_modifiers_filter.h"

namespace wgpu {
class Adapter;
}

namespace gpu {

// Populate the map of fourcc formats to the list of modifiers supported by that
// format on Dawn.
GPU_GLES2_EXPORT void PopulateDawnDrmFormatsAndModifiers(
    wgpu::Adapter adapter,
    base::flat_map<uint32_t, std::vector<uint64_t>>& fourcc_modifier_map);

// DRM modifiers filter object that lets clients filter out modifiers that are
// not supported for Dawn import. The list of modifiers that the Dawn
// implementation can import is a subset of all modifiers supported by the
// hardware and can be queried through wgpu::Adapter::GetFormatCapabilities()
// which will query this API when Dawn is using the Vulkan backend.
// https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkGetPhysicalDeviceFormatProperties2.html
class GPU_GLES2_EXPORT DrmModifiersFilterDawn : public ui::DrmModifiersFilter {
 public:
  explicit DrmModifiersFilterDawn(wgpu::Adapter adapter);

  ~DrmModifiersFilterDawn() override;

  std::vector<uint64_t> Filter(gfx::BufferFormat format,
                               const std::vector<uint64_t>& modifiers) override;

 private:
  // Map from all BufferFormats to a set of modifiers supported by that format.
  base::flat_map<gfx::BufferFormat, std::vector<uint64_t>> modifier_map_;
};

}  //  namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_DRM_MODIFIERS_FILTER_DAWN_H_
