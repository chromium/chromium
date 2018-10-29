// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WEBGRAPHICSCONTEXT3D_PROVIDER_IMPL_H_
#define CONTENT_RENDERER_WEBGRAPHICSCONTEXT3D_PROVIDER_IMPL_H_

#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "components/viz/common/gpu/context_provider.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"

namespace cc {
class ImageDecodeCache;
}  // namespace cc

namespace gpu {
namespace gles2 {
class GLES2Interface;
}  // namespace gles2
}  // namespace gpu

namespace viz {
class GLHelper;
}  // namespace viz

namespace ws {
class ContextProviderCommandBuffer;
}  // namespace ws

namespace content {

class CONTENT_EXPORT WebGraphicsContext3DProviderImpl
    : public blink::WebGraphicsContext3DProvider,
      public viz::ContextLostObserver {
 public:
  WebGraphicsContext3DProviderImpl(
      scoped_refptr<ws::ContextProviderCommandBuffer> provider);
  ~WebGraphicsContext3DProviderImpl() override;

  // WebGraphicsContext3DProvider implementation.
  bool BindToCurrentThread() override;
  gpu::gles2::GLES2Interface* ContextGL() override;
  gpu::webgpu::WebGPUInterface* WebGPUInterface() override;
  GrContext* GetGrContext() override;
  const gpu::Capabilities& GetCapabilities() const override;
  const gpu::GpuFeatureInfo& GetGpuFeatureInfo() const override;
  viz::GLHelper* GetGLHelper() override;
  void SetLostContextCallback(base::RepeatingClosure) override;
  void SetErrorMessageCallback(
      base::RepeatingCallback<void(const char*, int32_t)>) override;
  cc::ImageDecodeCache* ImageDecodeCache(SkColorType) override;

  ws::ContextProviderCommandBuffer* context_provider() const {
    return provider_.get();
  }

 private:
  // viz::ContextLostObserver implementation.
  void OnContextLost() override;

  scoped_refptr<ws::ContextProviderCommandBuffer> provider_;
  std::unique_ptr<viz::GLHelper> gl_helper_;
  base::RepeatingClosure context_lost_callback_;
  base::flat_map<SkColorType, std::unique_ptr<cc::ImageDecodeCache>>
      image_decode_cache_map_;

  DISALLOW_COPY_AND_ASSIGN(WebGraphicsContext3DProviderImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_WEBGRAPHICSCONTEXT3D_PROVIDER_IMPL_H_
