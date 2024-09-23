// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_DEVICE_PERF_INFO_H_
#define GPU_CONFIG_DEVICE_PERF_INFO_H_

#include <cstdint>
#include <optional>

#include "build/build_config.h"
#include "gpu/gpu_export.h"

#if BUILDFLAG(IS_WIN)
#include <d3dcommon.h>
#endif

namespace gpu {

// These values are persistent to logs. Entries should not be renumbered and
// numeric values should never be reused.
// This should match enum IntelGpuGeneration in
//  \tools\metrics\histograms\metadata\gpu\enums.xml
enum class IntelGpuGeneration {
  kNonIntel = 0,
  kUnknownIntel = 1,  // Intel GPU, but not one of the following generations.
  kGen4 = 4,
  kGen5 = 5,
  kGen6 = 6,
  kGen7 = 7,
  kGen8 = 8,
  kGen9 = 9,
  kGen10 = 10,
  kGen11 = 11,
  kGen12 = 12,
  kGen13 = 13,
  kMaxValue = kGen13,
};

enum class HasDiscreteGpu {
  kNo = 0,
  kYes = 1,
  kUnknown = 2,
  kMaxValue = kUnknown,
};

struct GPU_EXPORT DevicePerfInfo {
  uint32_t total_physical_memory_mb = 0u;
  uint32_t total_disk_space_mb = 0u;
  uint32_t hardware_concurrency = 0u;
#if BUILDFLAG(IS_WIN)
  // system commit limit (n pages) x page size.
  uint32_t system_commit_limit_mb = 0u;
  // If multiple GPUs are detected, this holds the highest feature level.
  D3D_FEATURE_LEVEL d3d11_feature_level = D3D_FEATURE_LEVEL_1_0_CORE;
  HasDiscreteGpu has_discrete_gpu = HasDiscreteGpu::kUnknown;
#endif

  // The following fields are only filled on the browser side. They don't
  // need to be part of mojom.
  IntelGpuGeneration intel_gpu_generation = IntelGpuGeneration::kNonIntel;
  bool software_rendering = false;
};

// Thread-safe getter and setter of global instance of DevicePerfInfo.
GPU_EXPORT std::optional<DevicePerfInfo> GetDevicePerfInfo();
GPU_EXPORT void SetDevicePerfInfo(const DevicePerfInfo& device_perf_info);

}  // namespace gpu

#endif  // GPU_CONFIG_DEVICE_PERF_INFO_H_
