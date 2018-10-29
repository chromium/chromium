// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_TEST_FAKE_WEB_GRAPHICS_CONTEXT_3D_PROVIDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_TEST_FAKE_WEB_GRAPHICS_CONTEXT_3D_PROVIDER_H_

#include "cc/test/stub_decode_cache.h"
#include "cc/tiles/image_decode_cache.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/config/gpu_feature_info.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"

namespace blink {

class FakeWebGraphicsContext3DProvider : public WebGraphicsContext3DProvider {
 public:
  FakeWebGraphicsContext3DProvider(gpu::gles2::GLES2Interface* gl,
                                   cc::ImageDecodeCache* cache = nullptr)
      : gl_(gl),
        image_decode_cache_(cache ? cache : &stub_image_decode_cache_) {
    sk_sp<const GrGLInterface> gl_interface(GrGLCreateNullInterface());
    gr_context_ = GrContext::MakeGL(std::move(gl_interface));
    // enable all gpu features.
    for (unsigned feature = 0; feature < gpu::NUMBER_OF_GPU_FEATURE_TYPES;
         ++feature) {
      gpu_feature_info_.status_values[feature] = gpu::kGpuFeatureStatusEnabled;
    }
  }
  ~FakeWebGraphicsContext3DProvider() override = default;

  GrContext* GetGrContext() override { return gr_context_.get(); }

  const gpu::Capabilities& GetCapabilities() const override {
    return capabilities_;
  }

  const gpu::GpuFeatureInfo& GetGpuFeatureInfo() const override {
    return gpu_feature_info_;
  }

  viz::GLHelper* GetGLHelper() override { return nullptr; }

  gpu::gles2::GLES2Interface* ContextGL() override { return gl_; }
  gpu::webgpu::WebGPUInterface* WebGPUInterface() override { return nullptr; }

  bool BindToCurrentThread() override { return false; }
  void SetLostContextCallback(base::Closure) override {}
  void SetErrorMessageCallback(
      base::RepeatingCallback<void(const char*, int32_t id)>) override {}
  cc::ImageDecodeCache* ImageDecodeCache(SkColorType) override {
    return image_decode_cache_;
  }

 private:
  cc::StubDecodeCache stub_image_decode_cache_;

  gpu::gles2::GLES2Interface* gl_;
  sk_sp<GrContext> gr_context_;
  gpu::Capabilities capabilities_;
  gpu::GpuFeatureInfo gpu_feature_info_;
  cc::ImageDecodeCache* image_decode_cache_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_TEST_FAKE_WEB_GRAPHICS_CONTEXT_3D_PROVIDER_H_
