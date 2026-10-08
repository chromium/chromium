// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_INIT_OZONE_H_
#define GPU_IPC_SERVICE_GPU_INIT_OZONE_H_

#include "base/memory/stack_allocated.h"
#include "gpu/vulkan/buildflags.h"
#include "skia/buildflags.h"

namespace gpu {

class DawnContextProvider;
struct GpuFeatureInfo;
class VulkanImplementation;

namespace gpu_init_internal {

void InitializePlatformForGpu(bool enable_native_gpu_memory_buffers,
                              bool single_process);

void MaybeDisableWebGPUOnVulkanViaGLInterop(GpuFeatureInfo& gpu_feature_info);

void QueryNativePixmapSupport(GpuFeatureInfo& gpu_feature_info);

struct SetDrmModifiersFilterParams {
  STACK_ALLOCATED();

 public:
#if BUILDFLAG(ENABLE_VULKAN)
  VulkanImplementation* vulkan_implementation = nullptr;
#endif  // BUILDFLAG(ENABLE_VULKAN)

#if BUILDFLAG(SKIA_USE_DAWN)
  DawnContextProvider* dawn_context_provider = nullptr;
#endif  // BUILDFLAG(SKIA_USE_DAWN)
};

void AfterSandboxEntryAndSetDrmModifiersFilter(
    GpuFeatureInfo& gpu_feature_info,
    const SetDrmModifiersFilterParams& params);

}  // namespace gpu_init_internal

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_INIT_OZONE_H_
