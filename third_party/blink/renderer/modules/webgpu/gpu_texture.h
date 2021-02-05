// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_TEXTURE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_TEXTURE_H_

#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

class ExceptionState;
class HTMLVideoElement;
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
  static GPUTexture* FromVideo(GPUDevice* device,
                               HTMLVideoElement* video,
                               WGPUTextureUsage usage,
                               ExceptionState& exception_state);

  GPUTexture(GPUDevice* device, WGPUTexture texture, WGPUTextureFormat format);
  GPUTexture(GPUDevice* device,
             WGPUTextureFormat format,
             scoped_refptr<WebGPUMailboxTexture> mailbox_texture);

  // gpu_texture.idl
  GPUTextureView* createView(const GPUTextureViewDescriptor* webgpu_desc);
  void destroy();

  WGPUTextureFormat Format() { return format_; }

 private:
  WGPUTextureFormat format_;
  scoped_refptr<WebGPUMailboxTexture> mailbox_texture_;
  DISALLOW_COPY_AND_ASSIGN(GPUTexture);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_TEXTURE_H_
