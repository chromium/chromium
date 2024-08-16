// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_GPU_SUB_IMAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_GPU_SUB_IMAGE_H_

#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_texture_view_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"
#include "third_party/blink/renderer/modules/xr/xr_sub_image.h"

namespace blink {

class XRGPUSubImage final : public XRSubImage {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit XRGPUSubImage(const gfx::Rect& viewport) : XRSubImage(viewport) {}

  GPUTexture* colorTexture() const { return color_texture_.Get(); }
  GPUTexture* depthStencilTexture() const {
    return depth_stencil_texture_.Get();
  }
  GPUTexture* motionVectorTexture() const {
    return motion_vector_texture_.Get();
  }

  const GPUTextureViewDescriptor* getViewDescriptor() const {
    GPUTextureViewDescriptor* view_descriptor =
        GPUTextureViewDescriptor::Create();
    view_descriptor->setDimension("2D");
    view_descriptor->setMipLevelCount(1);
    view_descriptor->setArrayLayerCount(1);
    return view_descriptor;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(color_texture_);
    visitor->Trace(depth_stencil_texture_);
    visitor->Trace(motion_vector_texture_);
    XRSubImage::Trace(visitor);
  }

 private:
  Member<GPUTexture> color_texture_{nullptr};
  Member<GPUTexture> depth_stencil_texture_{nullptr};
  Member<GPUTexture> motion_vector_texture_{nullptr};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_GPU_SUB_IMAGE_H_
