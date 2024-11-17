// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_GPU_SWAP_CHAIN_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_GPU_SWAP_CHAIN_H_

#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_swap_buffer_provider.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class GPUDevice;
class GPUTexture;
class XRCompositionLayer;

class XRGPUSwapChain : public GarbageCollected<XRGPUSwapChain> {
 public:
  explicit XRGPUSwapChain(GPUDevice*);
  virtual ~XRGPUSwapChain() = default;

  GPUTexture* GetCurrentTexture();
  virtual void OnFrameStart();
  virtual void OnFrameEnd();

  virtual const wgpu::TextureDescriptor& descriptor() const = 0;

  GPUDevice* device() { return device_.Get(); }

  virtual void SetLayer(XRCompositionLayer* layer) { layer_ = layer; }
  XRCompositionLayer* layer() { return layer_.Get(); }

  bool texture_was_queried() const { return texture_queried_; }

  virtual void Trace(Visitor* visitor) const;

 protected:
  virtual GPUTexture* ProduceTexture() = 0;

  GPUTexture* ResetCurrentTexture();
  void ClearCurrentTexture(wgpu::CommandEncoder);

 private:
  Member<GPUDevice> device_;
  Member<GPUTexture> current_texture_;
  Member<XRCompositionLayer> layer_;
  bool texture_queried_;
};

// A texture swap chain that is not communicated back to the compositor, used
// for things like depth/stencil attachments that don't assist reprojection.
class XRGPUStaticSwapChain final : public XRGPUSwapChain {
 public:
  XRGPUStaticSwapChain(GPUDevice*, const wgpu::TextureDescriptor&);

  GPUTexture* ProduceTexture() override;

  void OnFrameEnd() override;

  const wgpu::TextureDescriptor& descriptor() const override {
    return descriptor_;
  }

 private:
  wgpu::TextureDescriptor descriptor_;
};

// A swap chain backed by Mailboxes
class XRGPUMailboxSwapChain final : public XRGPUSwapChain {
 public:
  XRGPUMailboxSwapChain(GPUDevice* device, const wgpu::TextureDescriptor& desc);
  ~XRGPUMailboxSwapChain() override = default;

  GPUTexture* ProduceTexture() override;

  void OnFrameEnd() override;

  const wgpu::TextureDescriptor& descriptor() const override {
    return descriptor_;
  }

 private:
  wgpu::TextureDescriptor descriptor_;
  wgpu::DawnTextureInternalUsageDescriptor texture_internal_usage_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_GPU_SWAP_CHAIN_H_
