// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_TEXTURE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_TEXTURE_H_

#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

class ExceptionState;
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
  static GPUTexture* CreateError(GPUDevice* device,
                                 const WGPUTextureDescriptor* desc);

  GPUTexture(GPUDevice* device, WGPUTexture texture);
  GPUTexture(GPUDevice* device,
             WGPUTextureFormat format,
             WGPUTextureUsage usage,
             scoped_refptr<WebGPUMailboxTexture> mailbox_texture);

  ~GPUTexture() override;

  GPUTexture(const GPUTexture&) = delete;
  GPUTexture& operator=(const GPUTexture&) = delete;

  // gpu_texture.idl
  GPUTextureView* createView(const GPUTextureViewDescriptor* webgpu_desc,
                             ExceptionState& exception_state);
  void destroy();
  uint32_t width() const;
  uint32_t height() const;
  uint32_t depthOrArrayLayers() const;
  uint32_t mipLevelCount() const;
  uint32_t sampleCount() const;
  String dimension() const;
  String format() const;
  uint32_t usage() const;

  WGPUTextureDimension Dimension() { return dimension_; }
  WGPUTextureFormat Format() { return format_; }
  WGPUTextureUsageFlags Usage() { return usage_; }

  void DissociateMailbox();

 private:
  void setLabelImpl(const String& value) override {
    std::string utf8_label = value.Utf8();
    GetProcs().textureSetLabel(GetHandle(), utf8_label.c_str());
  }

  WGPUTextureDimension dimension_;
  WGPUTextureFormat format_;
  WGPUTextureUsageFlags usage_;
  scoped_refptr<WebGPUMailboxTexture> mailbox_texture_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_TEXTURE_H_
