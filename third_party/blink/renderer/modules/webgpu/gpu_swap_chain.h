// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_SWAP_CHAIN_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_SWAP_CHAIN_H_

#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_swap_buffer_provider.h"

namespace cc {
class Layer;
}

namespace blink {

class GPUCanvasContext;
class GPUDevice;
class GPUSwapChainDescriptor;
class GPUTexture;

class GPUSwapChain : public ScriptWrappable,
                     public DawnObjectBase,
                     public WebGPUSwapBufferProvider::Client {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GPUSwapChain* Create(GPUCanvasContext* context,
                              const GPUSwapChainDescriptor* descriptor);
  explicit GPUSwapChain(GPUCanvasContext* context,
                        const GPUSwapChainDescriptor* descriptor);
  ~GPUSwapChain() override;

  void Trace(blink::Visitor* visitor) override;

  void Neuter();
  cc::Layer* CcLayer();

  // gpu_swap_chain.idl
  GPUTexture* getCurrentTexture();

  // WebGPUSwapBufferProvider::Client implementation
  void OnTextureTransferred() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(GPUSwapChain);

  scoped_refptr<WebGPUSwapBufferProvider> swap_buffers_;

  Member<GPUDevice> device_;
  Member<GPUCanvasContext> context_;
  WGPUTextureUsage usage_;

  Member<GPUTexture> texture_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_SWAP_CHAIN_H_
