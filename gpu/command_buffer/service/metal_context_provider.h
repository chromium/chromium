// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_METAL_CONTEXT_PROVIDER_H_
#define GPU_COMMAND_BUFFER_SERVICE_METAL_CONTEXT_PROVIDER_H_

#include <memory>

#include "gpu/gpu_gles2_export.h"
#include "third_party/skia/include/gpu/graphite/ContextOptions.h"
#include "third_party/skia/include/gpu/graphite/mtl/MtlGraphiteTypes.h"

#if __OBJC__
@protocol MTLDevice;
#endif  // __OBJC__

namespace gpu {
class GraphiteSharedContext;
class GpuProcessShmCount;
}  // namespace gpu

namespace viz {

// The MetalContextProvider provides a Metal-backed GraphiteSharedContext.
class GPU_GLES2_EXPORT MetalContextProvider {
 public:
  // Create and return a MetalContextProvider if possible. May return nullptr
  // if no Metal devices exist.
  static std::unique_ptr<MetalContextProvider> Create();

  MetalContextProvider(const MetalContextProvider&) = delete;
  MetalContextProvider& operator=(const MetalContextProvider&) = delete;
  ~MetalContextProvider();

  bool InitializeGraphiteContext(
      const skgpu::graphite::ContextOptions& options,
      gpu::GpuProcessShmCount* use_shader_cache_shm_count);

  int32_t GetMaxTextureSize() const;

  gpu::GraphiteSharedContext* GetGraphiteSharedContext() const;

#if __OBJC__
  id<MTLDevice> GetMTLDevice() const;
#endif  // __OBJC__

 private:
#if __OBJC__
  explicit MetalContextProvider(id<MTLDevice> device);
#endif  // __OBJC__

  struct ObjCStorage;
  std::unique_ptr<ObjCStorage> objc_storage_;
};

}  // namespace viz

#endif  // GPU_COMMAND_BUFFER_SERVICE_METAL_CONTEXT_PROVIDER_H_
