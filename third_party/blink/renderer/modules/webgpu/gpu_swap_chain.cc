// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_swap_chain.h"

#include "third_party/blink/renderer/modules/webgpu/gpu_canvas_context.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_mailbox_texture.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

GPUSwapChain::GPUSwapChain(GPUCanvasContext* context,
                           GPUDevice* device,
                           WGPUTextureUsage usage,
                           WGPUTextureFormat format,
                           SkFilterQuality filter_quality)
    : DawnObjectImpl(device),
      context_(context),
      usage_(usage),
      format_(format) {
  // TODO: Use label from GPUObjectDescriptorBase.
  swap_buffers_ = base::AdoptRef(new WebGPUSwapBufferProvider(
      this, GetDawnControlClient(), device->GetHandle(), usage_, format));
  swap_buffers_->SetFilterQuality(filter_quality);
}

GPUSwapChain::~GPUSwapChain() {
  Neuter();
}

void GPUSwapChain::Trace(Visitor* visitor) const {
  visitor->Trace(context_);
  visitor->Trace(texture_);
  DawnObjectImpl::Trace(visitor);
}

void GPUSwapChain::Neuter() {
  texture_ = nullptr;
  if (swap_buffers_) {
    swap_buffers_->Neuter();
    swap_buffers_ = nullptr;
  }
}

cc::Layer* GPUSwapChain::CcLayer() {
  DCHECK(swap_buffers_);
  return swap_buffers_->CcLayer();
}

void GPUSwapChain::SetFilterQuality(SkFilterQuality filter_quality) {
  DCHECK(swap_buffers_);
  if (swap_buffers_) {
    swap_buffers_->SetFilterQuality(filter_quality);
  }
}

// gpu_swap_chain.idl
GPUTexture* GPUSwapChain::getCurrentTexture() {
  if (!swap_buffers_) {
    // TODO(cwallez@chromium.org) return an error texture.
    return nullptr;
  }

  // Calling getCurrentTexture returns a texture that is valid until the
  // animation frame it gets presented. If getCurrenTexture is called multiple
  // time, the same texture should be returned. |texture_| is set to null when
  // presented so that we know we should create a new one.
  if (texture_) {
    return texture_;
  }

  WGPUTexture dawn_client_texture =
      swap_buffers_->GetNewTexture(context_->CanvasSize());
  DCHECK(dawn_client_texture);
  texture_ =
      MakeGarbageCollected<GPUTexture>(device_, dawn_client_texture, format_);
  return texture_;
}

// WebGPUSwapBufferProvider::Client implementation
void GPUSwapChain::OnTextureTransferred() {
  DCHECK(texture_);
  texture_ = nullptr;
}

}  // namespace blink
