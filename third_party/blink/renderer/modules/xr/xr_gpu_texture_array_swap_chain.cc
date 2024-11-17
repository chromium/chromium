// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_gpu_texture_array_swap_chain.h"

#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_cpp.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"

namespace blink {

XRGPUTextureArraySwapChain::XRGPUTextureArraySwapChain(
    GPUDevice* device,
    XRGPUSwapChain* wrapped_swap_chain,
    uint32_t layers)
    : XRGPUSwapChain(device), wrapped_swap_chain_(wrapped_swap_chain) {
  CHECK(wrapped_swap_chain_);

  // Copy the wrapped swap chain's descriptor and divide its width by the
  // number of requested layers.
  CHECK_EQ(descriptor_.size.width % layers, 0ul);
  descriptor_ = wrapped_swap_chain->descriptor();
  descriptor_.label = "XRGPUTextureArraySwapChain";
  descriptor_.size = {descriptor_.size.width / layers, descriptor_.size.height,
                      layers};

  texture_internal_usage_ = {{
      .internalUsage =
          wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc,
  }};
  descriptor_.nextInChain = &texture_internal_usage_;
}

GPUTexture* XRGPUTextureArraySwapChain::ProduceTexture() {
  return GPUTexture::Create(device(), &descriptor_);
}

void XRGPUTextureArraySwapChain::SetLayer(XRCompositionLayer* layer) {
  XRGPUSwapChain::SetLayer(layer);
  wrapped_swap_chain_->SetLayer(layer);
}

void XRGPUTextureArraySwapChain::OnFrameStart() {
  wrapped_swap_chain_->OnFrameStart();
}

void XRGPUTextureArraySwapChain::OnFrameEnd() {
  if (!texture_was_queried()) {
    wrapped_swap_chain_->OnFrameEnd();
    return;
  }

  // Copy the texture layers into the wrapped swap chain
  GPUTexture* source_texture = GetCurrentTexture();
  GPUTexture* wrapped_texture = wrapped_swap_chain_->GetCurrentTexture();

  wgpu::DawnEncoderInternalUsageDescriptor internal_usage_desc = {{
      .useInternalUsages = true,
  }};
  wgpu::CommandEncoderDescriptor command_encoder_desc = {
      .nextInChain = &internal_usage_desc,
      .label = "XRGPUTextureArraySwapChain Copy",
  };
  wgpu::CommandEncoder command_encoder =
      device()->GetHandle().CreateCommandEncoder(&command_encoder_desc);

  wgpu::ImageCopyTexture source = {
      .texture = source_texture->GetHandle(),
      .aspect = wgpu::TextureAspect::All,
  };
  wgpu::ImageCopyTexture destination = {
      .texture = wrapped_texture->GetHandle(),
      .aspect = wgpu::TextureAspect::All,
  };
  wgpu::Extent3D copy_size = {
      .width = source_texture->width(),
      .height = source_texture->height(),
      .depthOrArrayLayers = 1,
  };

  for (uint32_t i = 0; i < source_texture->depthOrArrayLayers(); ++i) {
    source.origin.z = i;
    destination.origin.x = source_texture->width() * i;
    command_encoder.CopyTextureToTexture(&source, &destination, &copy_size);
  }

  ClearCurrentTexture(command_encoder);

  wgpu::CommandBuffer command_buffer = command_encoder.Finish();
  command_encoder = nullptr;

  device()->GetHandle().GetQueue().Submit(1u, &command_buffer);
  command_buffer = nullptr;

  wrapped_swap_chain_->OnFrameEnd();

  // Intentionally not calling ResetCurrentTexture() here to keep the previously
  // produced texture for the next frame.
}

void XRGPUTextureArraySwapChain::Trace(Visitor* visitor) const {
  visitor->Trace(wrapped_swap_chain_);
  XRGPUSwapChain::Trace(visitor);
}

}  // namespace blink
