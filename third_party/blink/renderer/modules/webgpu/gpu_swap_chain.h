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

class CanvasResource;
class GPUCanvasContext;
class GPUDevice;
class GPUTexture;
class StaticBitmapImage;

class GPUSwapChain : public DawnObjectImpl,
                     public WebGPUSwapBufferProvider::Client {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit GPUSwapChain(GPUCanvasContext*,
                        GPUDevice*,
                        WGPUTextureUsage,
                        WGPUTextureFormat,
                        cc::PaintFlags::FilterQuality,
                        IntSize);
  ~GPUSwapChain() override;

  void Trace(Visitor* visitor) const override;

  void Neuter();
  cc::Layer* CcLayer();
  void SetFilterQuality(cc::PaintFlags::FilterQuality);

  const gfx::Size& Size() const { return swap_buffers_->Size(); }

  viz::ResourceFormat Format() const { return swap_buffers_->Format(); }

  // Returns a StaticBitmapImage backed by a texture containing the current
  // contents of the front buffer. This is done without any pixel copies. The
  // texture in the ImageBitmap is from the active ContextProvider on the
  // WebGPUSwapBufferProvider.
  scoped_refptr<StaticBitmapImage> TransferToStaticBitmapImage();

  // Returns a CanvasResource of type ExternalCanvasResource that will
  // encapsulate an external mailbox, synctoken and release callback.
  scoped_refptr<CanvasResource> ExportCanvasResource();

  // Copies the back buffer to given shared image resource provider which must
  // be webgpu compatible. Returns true on success.
  bool CopyToResourceProvider(CanvasResourceProvider*);

  // gpu_swap_chain.idl
  GPUTexture* getCurrentTexture();

  // WebGPUSwapBufferProvider::Client implementation
  void OnTextureTransferred() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(GPUSwapChain);

  scoped_refptr<WebGPUSwapBufferProvider> swap_buffers_;

  Member<GPUCanvasContext> context_;
  WGPUTextureUsage usage_;
  WGPUTextureFormat format_;
  const IntSize size_;

  Member<GPUTexture> texture_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_SWAP_CHAIN_H_
