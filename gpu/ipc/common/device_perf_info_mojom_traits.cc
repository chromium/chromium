// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/device_perf_info_mojom_traits.h"

#include "build/build_config.h"

namespace mojo {

#if BUILDFLAG(IS_WIN)
// static
gpu::mojom::Direct3DFeatureLevel
EnumTraits<gpu::mojom::Direct3DFeatureLevel, D3D_FEATURE_LEVEL>::ToMojom(
    D3D_FEATURE_LEVEL d3d_feature_level) {
  switch (d3d_feature_level) {
    case D3D_FEATURE_LEVEL_1_0_CORE:
      return gpu::mojom::Direct3DFeatureLevel::k1_0_Core;
    case D3D_FEATURE_LEVEL_9_1:
      return gpu::mojom::Direct3DFeatureLevel::k9_1;
    case D3D_FEATURE_LEVEL_9_2:
      return gpu::mojom::Direct3DFeatureLevel::k9_2;
    case D3D_FEATURE_LEVEL_9_3:
      return gpu::mojom::Direct3DFeatureLevel::k9_3;
    case D3D_FEATURE_LEVEL_10_0:
      return gpu::mojom::Direct3DFeatureLevel::k10_0;
    case D3D_FEATURE_LEVEL_10_1:
      return gpu::mojom::Direct3DFeatureLevel::k10_1;
    case D3D_FEATURE_LEVEL_11_0:
      return gpu::mojom::Direct3DFeatureLevel::k11_0;
    case D3D_FEATURE_LEVEL_11_1:
      return gpu::mojom::Direct3DFeatureLevel::k11_1;
    case D3D_FEATURE_LEVEL_12_0:
      return gpu::mojom::Direct3DFeatureLevel::k12_0;
    case D3D_FEATURE_LEVEL_12_1:
      return gpu::mojom::Direct3DFeatureLevel::k12_1;
    case D3D_FEATURE_LEVEL_12_2:
      return gpu::mojom::Direct3DFeatureLevel::k12_2;
  }
  NOTREACHED_IN_MIGRATION()
      << "Invalid D3D_FEATURE_LEVEL:" << d3d_feature_level;
  return gpu::mojom::Direct3DFeatureLevel::k1_0_Core;
}

// static
bool EnumTraits<gpu::mojom::Direct3DFeatureLevel, D3D_FEATURE_LEVEL>::FromMojom(
    gpu::mojom::Direct3DFeatureLevel input,
    D3D_FEATURE_LEVEL* out) {
  switch (input) {
    case gpu::mojom::Direct3DFeatureLevel::k1_0_Core:
      *out = D3D_FEATURE_LEVEL_1_0_CORE;
      return true;
    case gpu::mojom::Direct3DFeatureLevel::k9_1:
      *out = D3D_FEATURE_LEVEL_9_1;
      return true;
    case gpu::mojom::Direct3DFeatureLevel::k9_2:
      *out = D3D_FEATURE_LEVEL_9_2;
      return true;
    case gpu::mojom::Direct3DFeatureLevel::k9_3:
      *out = D3D_FEATURE_LEVEL_9_3;
      return true;
    case gpu::mojom::Direct3DFeatureLevel::k10_0:
      *out = D3D_FEATURE_LEVEL_10_0;
      return true;
    case gpu::mojom::Direct3DFeatureLevel::k10_1:
      *out = D3D_FEATURE_LEVEL_10_1;
      return true;
    case gpu::mojom::Direct3DFeatureLevel::k11_0:
      *out = D3D_FEATURE_LEVEL_11_0;
      return true;
    case gpu::mojom::Direct3DFeatureLevel::k11_1:
      *out = D3D_FEATURE_LEVEL_11_1;
      return true;
    case gpu::mojom::Direct3DFeatureLevel::k12_0:
      *out = D3D_FEATURE_LEVEL_12_0;
      return true;
    case gpu::mojom::Direct3DFeatureLevel::k12_1:
      *out = D3D_FEATURE_LEVEL_12_1;
      return true;
    case gpu::mojom::Direct3DFeatureLevel::k12_2:
      *out = D3D_FEATURE_LEVEL_12_2;
      return true;
  }
  NOTREACHED_IN_MIGRATION() << "Invalid D3D_FEATURE_LEVEL: " << input;
  return false;
}
#endif  // BUILDFLAG(IS_WIN)

gpu::mojom::HasDiscreteGpu
EnumTraits<gpu::mojom::HasDiscreteGpu, gpu::HasDiscreteGpu>::ToMojom(
    gpu::HasDiscreteGpu has_discrete_gpu) {
  switch (has_discrete_gpu) {
    case gpu::HasDiscreteGpu::kUnknown:
      return gpu::mojom::HasDiscreteGpu::kUnknown;
    case gpu::HasDiscreteGpu::kNo:
      return gpu::mojom::HasDiscreteGpu::kNo;
    case gpu::HasDiscreteGpu::kYes:
      return gpu::mojom::HasDiscreteGpu::kYes;
  }
  NOTREACHED_IN_MIGRATION()
      << "Invalid gpu::HasDiscreteGpu: " << static_cast<int>(has_discrete_gpu);
  return gpu::mojom::HasDiscreteGpu::kUnknown;
}

// static
bool EnumTraits<gpu::mojom::HasDiscreteGpu, gpu::HasDiscreteGpu>::FromMojom(
    gpu::mojom::HasDiscreteGpu input,
    gpu::HasDiscreteGpu* out) {
  switch (input) {
    case gpu::mojom::HasDiscreteGpu::kUnknown:
      *out = gpu::HasDiscreteGpu::kUnknown;
      return true;
    case gpu::mojom::HasDiscreteGpu::kNo:
      *out = gpu::HasDiscreteGpu::kNo;
      return true;
    case gpu::mojom::HasDiscreteGpu::kYes:
      *out = gpu::HasDiscreteGpu::kYes;
      return true;
  }
  NOTREACHED_IN_MIGRATION() << "Invalid gpu::mojom::HasDiscreteGpu: " << input;
  return false;
}

// static
bool StructTraits<gpu::mojom::DevicePerfInfoDataView, gpu::DevicePerfInfo>::
    Read(gpu::mojom::DevicePerfInfoDataView data, gpu::DevicePerfInfo* out) {
  out->total_physical_memory_mb = data.total_physical_memory_mb();
  out->total_disk_space_mb = data.total_disk_space_mb();
  out->hardware_concurrency = data.hardware_concurrency();
  bool rt = true;
#if BUILDFLAG(IS_WIN)
  out->system_commit_limit_mb = data.system_commit_limit_mb();
  rt &= data.ReadD3d11FeatureLevel(&out->d3d11_feature_level);
  rt &= data.ReadHasDiscreteGpu(&out->has_discrete_gpu);
#endif  // BUILDFLAG(IS_WIN)
  return rt;
}

}  // namespace mojo
