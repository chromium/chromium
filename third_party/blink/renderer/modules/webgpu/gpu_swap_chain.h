// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_SWAP_CHAIN_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_SWAP_CHAIN_H_

#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_canvas_compositing_alpha_mode.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_swap_buffer_provider.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace cc {
class Layer;
}

namespace blink {

class CanvasResource;
class GPUCanvasContext;
class GPUDevice;
class GPUTexture;
class StaticBitmapImage;

class GPUSwapChain final : public GarbageCollected<GPUSwapChain>,
                           public DawnObjectBase,
                           public WebGPUSwapBufferProvider::Client {
 public:
  explicit GPUSwapChain(GPUCanvasContext*,
                        GPUDevice*,
                        WGPUTextureUsage,
                        WGPUTextureFormat,
                        cc::PaintFlags::FilterQuality,
                        V8GPUCanvasCompositingAlphaMode::Enum,
                        gfx::Size);

  GPUSwapChain(const GPUSwapChain&) = delete;
  GPUSwapChain& operator=(const GPUSwapChain&) = delete;

  virtual ~GPUSwapChain();

  void Trace(Visitor* visitor) const;

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
  bool CopyToResourceProvider(CanvasResourceProvider*) const;

  // Produces a snapshot of the current contents of the swap chain if possible.
  // If that texture has already been sent to the compositor, will produce a
  // snapshot of the just released texture associated to this gpu context.
  // todo(crbug/1267243) Make snapshot always return the current frame.
  scoped_refptr<StaticBitmapImage> Snapshot() const;

  GPUTexture* getCurrentTexture();

  // WebGPUSwapBufferProvider::Client implementation
  void OnTextureTransferred() override;

 private:
  scoped_refptr<WebGPUSwapBufferProvider> swap_buffers_;

  Member<GPUDevice> device_;
  Member<GPUCanvasContext> context_;
  const WGPUTextureUsage usage_;
  const WGPUTextureFormat format_;
  const V8GPUCanvasCompositingAlphaMode::Enum compositing_alpha_mode_;
  const gfx::Size size_;

  Member<GPUTexture> texture_;
  WGPURenderPipeline alpha_to_one_pipeline_ = nullptr;

  scoped_refptr<StaticBitmapImage> SnapshotInternal(
      const WGPUTexture& texture,
      const gfx::Size& size) const;
  bool CopyTextureToResourceProvider(
      const WGPUTexture& texture,
      const gfx::Size& size,
      CanvasResourceProvider* resource_provider) const;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_SWAP_CHAIN_H_
