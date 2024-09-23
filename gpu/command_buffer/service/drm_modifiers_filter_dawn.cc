// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/drm_modifiers_filter_dawn.h"

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/flat_map.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "third_party/dawn/include/dawn/webgpu_cpp.h"
#include "third_party/libdrm/src/include/drm/drm_fourcc.h"
#include "ui/gfx/linux/drm_util_linux.h"

namespace gpu {

namespace {

// This should contain all the multiple-memory-plane modifiers because Dawn does
// not yet support multiple memory plane formats.
constexpr auto kModifierBlocklist = base::MakeFixedFlatSet<uint64_t>({
    I915_FORMAT_MOD_Y_TILED_CCS,
    I915_FORMAT_MOD_Yf_TILED_CCS,
    I915_FORMAT_MOD_Y_TILED_GEN12_RC_CCS,
    I915_FORMAT_MOD_Y_TILED_GEN12_MC_CCS,
    I915_FORMAT_MOD_Y_TILED_GEN12_RC_CCS_CC,
    I915_FORMAT_MOD_4_TILED_DG2_RC_CCS,
    I915_FORMAT_MOD_4_TILED_DG2_MC_CCS,
    I915_FORMAT_MOD_4_TILED_DG2_RC_CCS_CC,
});

base::flat_map<gfx::BufferFormat, std::vector<uint64_t>> GetDawnModifierMap(
    wgpu::Adapter adapter) {
  base::flat_map<gfx::BufferFormat, std::vector<uint64_t>> modifier_map;

  for (int i = 0; i <= static_cast<int>(gfx::BufferFormat::LAST); i++) {
    gfx::BufferFormat buffer_format = static_cast<gfx::BufferFormat>(i);
    auto si_format = viz::GetSharedImageFormat(buffer_format);
    auto wgpu_format = ToDawnFormat(si_format);
    if (wgpu_format == wgpu::TextureFormat::Undefined) {
      modifier_map.emplace(buffer_format, std::vector<uint64_t>());
      continue;
    }

    wgpu::DrmFormatCapabilities drmCapabilities;
    wgpu::FormatCapabilities formatCapabilities;
    formatCapabilities.nextInChain = &drmCapabilities;
    adapter.GetFormatCapabilities(wgpu_format, &formatCapabilities);

    if (!drmCapabilities.properties || !drmCapabilities.propertiesCount) {
      modifier_map.emplace(buffer_format, std::vector<uint64_t>());
      continue;
    }

    std::vector<uint64_t> modifiers;
    modifiers.reserve(drmCapabilities.propertiesCount);
    for (size_t j = 0; j < drmCapabilities.propertiesCount; j++) {
      if (!base::Contains(kModifierBlocklist,
                          drmCapabilities.properties[j].modifier)) {
        modifiers.push_back(drmCapabilities.properties[j].modifier);
      }
    }
    modifier_map.emplace(buffer_format, std::move(modifiers));
  }

  return modifier_map;
}

}  // namespace

void PopulateDawnDrmFormatsAndModifiers(
    wgpu::Adapter adapter,
    base::flat_map<uint32_t, std::vector<uint64_t>>& fourcc_modifier_map) {
  auto buffer_format_map = GetDawnModifierMap(adapter);
  for (auto& entry : buffer_format_map) {
    int fourcc_format = ui::GetFourCCFormatFromBufferFormat(entry.first);
    if (fourcc_format == DRM_FORMAT_INVALID) {
      continue;
    }
    fourcc_modifier_map.emplace(fourcc_format, entry.second);
  }
}

DrmModifiersFilterDawn::DrmModifiersFilterDawn(wgpu::Adapter adapter) {
  modifier_map_ = GetDawnModifierMap(adapter);
}

DrmModifiersFilterDawn::~DrmModifiersFilterDawn() = default;

std::vector<uint64_t> DrmModifiersFilterDawn::Filter(
    gfx::BufferFormat format,
    const std::vector<uint64_t>& modifiers) {
  const auto& modifier_set = modifier_map_.at(format);

  std::vector<uint64_t> intersection;
  for (const auto& modifier : modifiers) {
    if (base::Contains(modifier_set, modifier)) {
      intersection.push_back(modifier);
    }
  }
  return intersection;
}

}  // namespace gpu
