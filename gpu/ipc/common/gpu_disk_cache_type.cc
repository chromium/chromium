// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/gpu_disk_cache_type.h"

#include <variant>

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
    case GpuDiskCacheType::kDawnGraphite:
      s << "gpu::GpuDiskCacheType::kDawnGraphite";
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
    case GpuDiskCacheType::kDawnGraphite:
      s << "DawnGraphiteHandle(" << GetHandleValue(handle) << ")";
      break;
  }
  return s;
}

base::FilePath::StringType GetGpuDiskCacheSubdir(GpuDiskCacheType type) {
  switch (type) {
    case GpuDiskCacheType::kGlShaders:
      return FILE_PATH_LITERAL("GPUCache");
    case GpuDiskCacheType::kDawnWebGPU:
      return FILE_PATH_LITERAL("DawnWebGPUCache");
    case GpuDiskCacheType::kDawnGraphite:
      return FILE_PATH_LITERAL("DawnGraphiteCache");
  }
  NOTREACHED();
}

GpuDiskCacheType GetHandleType(const GpuDiskCacheHandle& handle) {
  if (std::holds_alternative<gpu::GpuDiskCacheGlShaderHandle>(handle)) {
    return GpuDiskCacheType::kGlShaders;
  }
  if (std::holds_alternative<gpu::GpuDiskCacheDawnWebGPUHandle>(handle)) {
    return GpuDiskCacheType::kDawnWebGPU;
  }
  DCHECK(std::holds_alternative<gpu::GpuDiskCacheDawnGraphiteHandle>(handle));
  return GpuDiskCacheType::kDawnGraphite;
}

int32_t GetHandleValue(const GpuDiskCacheHandle& handle) {
  if (std::holds_alternative<gpu::GpuDiskCacheGlShaderHandle>(handle)) {
    return std::get<gpu::GpuDiskCacheGlShaderHandle>(handle).value();
  }
  if (std::holds_alternative<gpu::GpuDiskCacheDawnWebGPUHandle>(handle)) {
    return std::get<gpu::GpuDiskCacheDawnWebGPUHandle>(handle).value();
  }
  DCHECK(std::holds_alternative<gpu::GpuDiskCacheDawnGraphiteHandle>(handle));
  return std::get<gpu::GpuDiskCacheDawnGraphiteHandle>(handle).value();
}

bool IsReservedGpuDiskCacheHandle(const GpuDiskCacheHandle& handle) {
  if (std::holds_alternative<gpu::GpuDiskCacheGlShaderHandle>(handle)) {
    const auto& gl_shader_handle =
        std::get<gpu::GpuDiskCacheGlShaderHandle>(handle);
    return gl_shader_handle == kDisplayCompositorGpuDiskCacheHandle ||
           gl_shader_handle == kGrShaderGpuDiskCacheHandle;
  }
  if (std::holds_alternative<gpu::GpuDiskCacheDawnGraphiteHandle>(handle)) {
    const auto& dawn_graphite_handle =
        std::get<gpu::GpuDiskCacheDawnGraphiteHandle>(handle);
    return dawn_graphite_handle == kGraphiteDawnGpuDiskCacheHandle;
  }
  return false;
}

}  // namespace gpu
