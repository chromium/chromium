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
    : device_(device), wrapped_swap_chain_(wrapped_swap_chain) {
  CHECK(device_);
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

  texture_ = GPUTexture::Create(device, &descriptor_);
}

GPUTexture* XRGPUTextureArraySwapChain::GetCurrentTexture() {
  return texture_;
}

void XRGPUTextureArraySwapChain::SetLayer(XRCompositionLayer* layer) {
  XRGPUSwapChain::SetLayer(layer);
  wrapped_swap_chain_->SetLayer(layer);
}

void XRGPUTextureArraySwapChain::OnFrameStart() {
  wrapped_swap_chain_->OnFrameStart();
}

void XRGPUTextureArraySwapChain::OnFrameEnd() {
  // Copy the texture layers into the wrapped swap chain
  GPUTexture* wrapped_texture = wrapped_swap_chain_->GetCurrentTexture();

  wgpu::DawnEncoderInternalUsageDescriptor internal_usage_desc = {{
      .useInternalUsages = true,
  }};
  wgpu::CommandEncoderDescriptor command_encoder_desc = {
      .nextInChain = &internal_usage_desc,
      .label = "XRGPUTextureArraySwapChain Copy",
  };
  wgpu::CommandEncoder command_encoder =
      device_->GetHandle().CreateCommandEncoder(&command_encoder_desc);

  wgpu::ImageCopyTexture source = {
      .texture = texture_->GetHandle(),
      .aspect = wgpu::TextureAspect::All,
  };
  wgpu::ImageCopyTexture destination = {
      .texture = wrapped_texture->GetHandle(),
      .aspect = wgpu::TextureAspect::All,
  };
  wgpu::Extent3D copy_size = {
      .width = texture_->width(),
      .height = texture_->height(),
      .depthOrArrayLayers = 1,
  };

  const uint32_t layers = descriptor_.size.depthOrArrayLayers;
  for (uint32_t i = 0; i < layers; ++i) {
    source.origin.z = i;
    destination.origin.x = texture_->width() * i;
    command_encoder.CopyTextureToTexture(&source, &destination, &copy_size);
  }

  // Clear the texture array.
  for (uint32_t i = 0; i < layers; ++i) {
    wgpu::TextureViewDescriptor view_desc = {
        .dimension = wgpu::TextureViewDimension::e2D,
        .baseArrayLayer = i,
        .arrayLayerCount = 1,
    };

    wgpu::TextureView view = texture_->GetHandle().CreateView(&view_desc);

    wgpu::RenderPassColorAttachment color_attachment = {
        .view = view,
        .loadOp = wgpu::LoadOp::Clear,
        .storeOp = wgpu::StoreOp::Store,
        .clearValue = {0, 0, 0, 0},
    };

    wgpu::RenderPassDescriptor render_pass_desc = {
        .colorAttachmentCount = 1,
        .colorAttachments = &color_attachment,
    };

    // Begin and immediately end the render pass to clear the texture.
    wgpu::RenderPassEncoder render_pass =
        command_encoder.BeginRenderPass(&render_pass_desc);
    render_pass.End();
  }

  wgpu::CommandBuffer command_buffer = command_encoder.Finish();
  command_encoder = nullptr;

  device_->GetHandle().GetQueue().Submit(1u, &command_buffer);
  command_buffer = nullptr;

  wrapped_swap_chain_->OnFrameEnd();
}

void XRGPUTextureArraySwapChain::Trace(Visitor* visitor) const {
  visitor->Trace(device_);
  visitor->Trace(texture_);
  visitor->Trace(wrapped_swap_chain_);
  XRGPUSwapChain::Trace(visitor);
}

}  // namespace blink
