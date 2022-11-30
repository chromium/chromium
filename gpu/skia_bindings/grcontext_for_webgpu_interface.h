// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_SKIA_BINDINGS_GRCONTEXT_FOR_WEBGPU_INTERFACE_H_
#define GPU_SKIA_BINDINGS_GRCONTEXT_FOR_WEBGPU_INTERFACE_H_

#include "base/memory/raw_ptr.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/gpu/GrContextOptions.h"

class GrDirectContext;

namespace gpu {
struct Capabilities;
class ContextSupport;
namespace webgpu {
class WebGPUInterface;
}
}  // namespace gpu

namespace skia_bindings {

// This class binds an offscreen GrContext to a WebGPU Device. The Device is
// used by the GrContext so must be valid as long as this class is alive.
class GrContextForWebGPUInterface
    : public GrContextOptions::ShaderErrorHandler {
 public:
  GrContextForWebGPUInterface(gpu::webgpu::WebGPUInterface* webgpu,
                              gpu::ContextSupport* context_support,
                              const gpu::Capabilities& capabilities,
                              size_t max_resource_cache_bytes,
                              size_t max_glyph_cache_texture_bytes);

  GrContextForWebGPUInterface(const GrContextForWebGPUInterface&) = delete;
  GrContextForWebGPUInterface& operator=(const GrContextForWebGPUInterface&) =
      delete;

  ~GrContextForWebGPUInterface() override;

  // GrContextOptions::ShaderErrorHandler implementation
  // Handles Skia-reported shader compilation errors.
  void compileError(const char* shader, const char* errors) override;

  GrDirectContext* get();

 private:
  sk_sp<class GrDirectContext> gr_context_;
  raw_ptr<gpu::ContextSupport> context_support_;
};

}  // namespace skia_bindings

#endif  // GPU_SKIA_BINDINGS_GRCONTEXT_FOR_WEBGPU_INTERFACE_H_
