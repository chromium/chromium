// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_swap_chain.h"

#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_canvas_context.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_swap_chain_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"

namespace blink {

// static
GPUSwapChain* GPUSwapChain::Create(GPUCanvasContext* context,
                                   const GPUSwapChainDescriptor* descriptor) {
  return MakeGarbageCollected<GPUSwapChain>(context, descriptor);
}

GPUSwapChain::GPUSwapChain(GPUCanvasContext* context,
                           const GPUSwapChainDescriptor* descriptor)
    : DawnObjectBase(descriptor->device()->GetDawnControlClient()),
      device_(descriptor->device()),
      context_(context),
      usage_(AsDawnEnum<WGPUTextureUsage>(descriptor->usage())) {
  // TODO: Use label from GPUObjectDescriptorBase.
  swap_buffers_ = base::AdoptRef(new WebGPUSwapBufferProvider(
      this, GetDawnControlClient(), usage_,
      AsDawnEnum<WGPUTextureFormat>(descriptor->format())));
}

GPUSwapChain::~GPUSwapChain() {
  Neuter();
}

void GPUSwapChain::Trace(blink::Visitor* visitor) {
  visitor->Trace(device_);
  visitor->Trace(context_);
  visitor->Trace(texture_);
  ScriptWrappable::Trace(visitor);
}

void GPUSwapChain::Neuter() {
  texture_ = nullptr;

  DCHECK(swap_buffers_);
  if (!swap_buffers_) {
    swap_buffers_->Neuter();
    swap_buffers_ = nullptr;
  }
}

cc::Layer* GPUSwapChain::CcLayer() {
  DCHECK(swap_buffers_);
  return swap_buffers_->CcLayer();
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

  WGPUTexture dawn_client_texture = swap_buffers_->GetNewTexture(
      device_->GetHandle(), context_->CanvasSize());
  DCHECK(dawn_client_texture);
  texture_ = MakeGarbageCollected<GPUTexture>(device_, dawn_client_texture);
  return texture_;
}

// WebGPUSwapBufferProvider::Client implementation
void GPUSwapChain::OnTextureTransferred() {
  DCHECK(texture_);
  texture_ = nullptr;
}

}  // namespace blink
