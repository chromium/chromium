// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_FEATURE_INFO_MOJOM_TRAITS_H_
#define GPU_IPC_COMMON_GPU_FEATURE_INFO_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "gpu/config/gpu_blocklist.h"
#include "gpu/config/gpu_driver_bug_list.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/gpu_feature_info.mojom.h"
#include "ui/gfx/mojom/buffer_types_mojom_traits.h"

namespace mojo {

template <>
struct GPU_EXPORT
    EnumTraits<gpu::mojom::GpuFeatureStatus, gpu::GpuFeatureStatus> {
  static gpu::mojom::GpuFeatureStatus ToMojom(gpu::GpuFeatureStatus status) {
    switch (status) {
      case gpu::kGpuFeatureStatusEnabled:
        return gpu::mojom::GpuFeatureStatus::Enabled;
      case gpu::kGpuFeatureStatusBlocklisted:
        return gpu::mojom::GpuFeatureStatus::Blocklisted;
      case gpu::kGpuFeatureStatusDisabled:
        return gpu::mojom::GpuFeatureStatus::Disabled;
      case gpu::kGpuFeatureStatusSoftware:
        return gpu::mojom::GpuFeatureStatus::Software;
      case gpu::kGpuFeatureStatusUndefined:
        return gpu::mojom::GpuFeatureStatus::Undefined;
      case gpu::kGpuFeatureStatusMax:
        return gpu::mojom::GpuFeatureStatus::Max;
    }
    NOTREACHED_IN_MIGRATION();
    return gpu::mojom::GpuFeatureStatus::Max;
  }

  static bool FromMojom(gpu::mojom::GpuFeatureStatus input,
                        gpu::GpuFeatureStatus* out) {
    switch (input) {
      case gpu::mojom::GpuFeatureStatus::Enabled:
        *out = gpu::kGpuFeatureStatusEnabled;
        return true;
      case gpu::mojom::GpuFeatureStatus::Blocklisted:
        *out = gpu::kGpuFeatureStatusBlocklisted;
        return true;
      case gpu::mojom::GpuFeatureStatus::Disabled:
        *out = gpu::kGpuFeatureStatusDisabled;
        return true;
      case gpu::mojom::GpuFeatureStatus::Software:
        *out = gpu::kGpuFeatureStatusSoftware;
        return true;
      case gpu::mojom::GpuFeatureStatus::Undefined:
        *out = gpu::kGpuFeatureStatusUndefined;
        return true;
      case gpu::mojom::GpuFeatureStatus::Max:
        *out = gpu::kGpuFeatureStatusMax;
        return true;
    }
    return false;
  }
};

template <>
struct GPU_EXPORT
    StructTraits<gpu::mojom::GpuFeatureInfoDataView, gpu::GpuFeatureInfo> {
  static bool Read(gpu::mojom::GpuFeatureInfoDataView data,
                   gpu::GpuFeatureInfo* out);

  static const std::array<gpu::GpuFeatureStatus,
                          gpu::NUMBER_OF_GPU_FEATURE_TYPES>&
  status_values(const gpu::GpuFeatureInfo& info) {
    return info.status_values;
  }

  static const std::vector<int32_t>& enabled_gpu_driver_bug_workarounds(
      const gpu::GpuFeatureInfo& info) {
    return info.enabled_gpu_driver_bug_workarounds;
  }

  static const std::string& disabled_extensions(
      const gpu::GpuFeatureInfo& info) {
    return info.disabled_extensions;
  }

  static const std::string& disabled_webgl_extensions(
      const gpu::GpuFeatureInfo& info) {
    return info.disabled_webgl_extensions;
  }

  static const std::vector<uint32_t>& applied_gpu_blocklist_entries(
      const gpu::GpuFeatureInfo& info) {
    return info.applied_gpu_blocklist_entries;
  }

  static const std::vector<uint32_t>& applied_gpu_driver_bug_list_entries(
      const gpu::GpuFeatureInfo& info) {
    return info.applied_gpu_driver_bug_list_entries;
  }

  static std::vector<gfx::BufferFormat>
  supported_buffer_formats_for_allocation_and_texturing(
      const gpu::GpuFeatureInfo& input) {
    return input.supported_buffer_formats_for_allocation_and_texturing;
  }
};

}  // namespace mojo

#endif  // GPU_IPC_COMMON_GPU_FEATURE_INFO_MOJOM_TRAITS_H_
