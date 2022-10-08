// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/gpu_disk_cache_type.h"

#include "base/notreached.h"

namespace gpu {

std::ostream& operator<<(std::ostream& s, const GpuDiskCacheType& type) {
  switch (type) {
    case GpuDiskCacheType::kGlShaders:
      s << "gpu::GpuDiskCacheType::kGlShaders";
      break;
    case GpuDiskCacheType::kDawnWebGPU:
      s << "gpu::GpuDiskCacheType::kDawnWebGPU";
      break;
  }
  return s;
}

std::ostream& operator<<(std::ostream& s, const GpuDiskCacheHandle& handle) {
  switch (GetHandleType(handle)) {
    case GpuDiskCacheType::kGlShaders:
      s << "GlShaderHandle(" << GetHandleValue(handle) << ")";
      break;
    case GpuDiskCacheType::kDawnWebGPU:
      s << "DawnWebGPUHandle(" << GetHandleValue(handle) << ")";
      break;
  }
  return s;
}

base::FilePath::StringType GetGpuDiskCacheSubdir(GpuDiskCacheType type) {
  switch (type) {
    case GpuDiskCacheType::kGlShaders:
      return FILE_PATH_LITERAL("GPUCache");
    case GpuDiskCacheType::kDawnWebGPU:
      return FILE_PATH_LITERAL("DawnCache");
  }
  NOTREACHED();
  return FILE_PATH_LITERAL("");
}

GpuDiskCacheType GetHandleType(const GpuDiskCacheHandle& handle) {
  if (absl::holds_alternative<gpu::GpuDiskCacheGlShaderHandle>(handle))
    return GpuDiskCacheType::kGlShaders;
  DCHECK(absl::holds_alternative<gpu::GpuDiskCacheDawnWebGPUHandle>(handle));
  return GpuDiskCacheType::kDawnWebGPU;
}

int32_t GetHandleValue(const GpuDiskCacheHandle& handle) {
  if (absl::holds_alternative<gpu::GpuDiskCacheGlShaderHandle>(handle))
    return absl::get<gpu::GpuDiskCacheGlShaderHandle>(handle).value();
  DCHECK(absl::holds_alternative<gpu::GpuDiskCacheDawnWebGPUHandle>(handle));
  return absl::get<gpu::GpuDiskCacheDawnWebGPUHandle>(handle).value();
}

bool IsReservedGpuDiskCacheHandle(const GpuDiskCacheHandle& handle) {
  if (absl::holds_alternative<gpu::GpuDiskCacheGlShaderHandle>(handle)) {
    const auto& gl_shader_handle =
        absl::get<gpu::GpuDiskCacheGlShaderHandle>(handle);
    return gl_shader_handle == kDisplayCompositorGpuDiskCacheHandle ||
           gl_shader_handle == kGrShaderGpuDiskCacheHandle;
  }
  return false;
}

}  // namespace gpu
