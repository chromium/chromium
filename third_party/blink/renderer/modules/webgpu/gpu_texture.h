// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_TEXTURE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_TEXTURE_H_

#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

class ExceptionState;
class HTMLCanvasElement;
class GPUTextureDescriptor;
class GPUTextureView;
class GPUTextureViewDescriptor;
class StaticBitmapImage;
class WebGPUMailboxTexture;

class GPUTexture : public DawnObject<WGPUTexture> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GPUTexture* Create(GPUDevice* device,
                            const GPUTextureDescriptor* webgpu_desc,
                            ExceptionState& exception_state);
  static GPUTexture* CreateError(GPUDevice* device);
  static GPUTexture* FromCanvas(GPUDevice* device,
                                HTMLCanvasElement* canvas,
                                WGPUTextureUsage usage,
                                ExceptionState& exception_state);

  GPUTexture(GPUDevice* device,
             WGPUTexture texture,
             WGPUTextureDimension dimension,
             WGPUTextureFormat format,
             WGPUTextureUsage usage);
  GPUTexture(GPUDevice* device,
             WGPUTextureFormat format,
             WGPUTextureUsage usage,
             scoped_refptr<WebGPUMailboxTexture> mailbox_texture);

  // gpu_texture.idl
  GPUTextureView* createView(const GPUTextureViewDescriptor* webgpu_desc);
  void destroy();

  WGPUTextureDimension Dimension() { return dimension_; }
  WGPUTextureFormat Format() { return format_; }
  WGPUTextureUsage Usage() { return usage_; }

 private:
  WGPUTextureDimension dimension_;
  WGPUTextureFormat format_;
  WGPUTextureUsage usage_;
  scoped_refptr<WebGPUMailboxTexture> mailbox_texture_;
  DISALLOW_COPY_AND_ASSIGN(GPUTexture);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_TEXTURE_H_
