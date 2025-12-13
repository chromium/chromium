// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_gpu_swap_chain.h"

#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/xr/xr_layer_shared_image_manager.h"

namespace blink {

bool IsDepthFormat(wgpu::TextureFormat format) {
  switch (format) {
    case wgpu::TextureFormat::Depth24Plus:
    case wgpu::TextureFormat::Depth16Unorm:
    case wgpu::TextureFormat::Depth24PlusStencil8:
    case wgpu::TextureFormat::Depth32Float:
    case wgpu::TextureFormat::Depth32FloatStencil8:
      return true;
    default:
      return false;
  }
}

bool IsStencilFormat(wgpu::TextureFormat format) {
  switch (format) {
    case wgpu::TextureFormat::Stencil8:
    case wgpu::TextureFormat::Depth24PlusStencil8:
    case wgpu::TextureFormat::Depth32FloatStencil8:
      return true;
    default:
      return false;
  }
}

XRGPUSwapChain::XRGPUSwapChain(GPUDevice* device) : device_(device) {
  CHECK(device);
}

// Clears the contents of the current texture to transparent black or 0 (for
// depth/stencil textures).
void XRGPUSwapChain::ClearCurrentTexture(wgpu::CommandEncoder command_encoder) {
  GPUTexture* texture = current_texture();
  if (!texture) {
    return;
  }

  bool hasDepth = IsDepthFormat(texture->Format());
  bool hasStencil = IsStencilFormat(texture->Format());

  // Clear each level of the texture array.
  for (uint32_t i = 0; i < texture->depthOrArrayLayers(); ++i) {
    wgpu::TextureViewDescriptor view_desc = {
        .dimension = wgpu::TextureViewDimension::e2D,
        .baseMipLevel = 0,
        .mipLevelCount = 1,
        .baseArrayLayer = i,
        .arrayLayerCount = 1,
    };

    wgpu::TextureView view = texture->GetHandle().CreateView(&view_desc);

    wgpu::RenderPassEncoder render_pass;
    if (hasDepth || hasStencil) {
      wgpu::RenderPassDepthStencilAttachment depth_stencil_attachment = {
          .view = view,
      };

      if (hasDepth) {
        depth_stencil_attachment.depthLoadOp = wgpu::LoadOp::Clear;
        depth_stencil_attachment.depthStoreOp = wgpu::StoreOp::Store;
        depth_stencil_attachment.depthClearValue = 0;
      }

      if (hasStencil) {
        depth_stencil_attachment.stencilLoadOp = wgpu::LoadOp::Clear;
        depth_stencil_attachment.stencilStoreOp = wgpu::StoreOp::Store;
        depth_stencil_attachment.stencilClearValue = 0;
      }

      wgpu::RenderPassDescriptor render_pass_desc = {
          .depthStencilAttachment = &depth_stencil_attachment,
      };

      render_pass = command_encoder.BeginRenderPass(&render_pass_desc);
    } else {
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

      render_pass = command_encoder.BeginRenderPass(&render_pass_desc);
    }

    // Immediately end the render pass to clear the texture.
    render_pass.End();
  }
}

void XRGPUSwapChain::Trace(Visitor* visitor) const {
  visitor->Trace(device_);
  XRSwapChain::Trace(visitor);
}

XRGPUStaticSwapChain::XRGPUStaticSwapChain(GPUDevice* device,
                                           const wgpu::TextureDescriptor& desc)
    : XRGPUSwapChain(device) {
  descriptor_ = desc;
}

GPUTexture* XRGPUStaticSwapChain::ProduceTexture() {
  return GPUTexture::Create(device(), &descriptor_);
}

void XRGPUStaticSwapChain::OnFrameEnd() {
  // TODO(crbug.com/5818595): Prior to shipping the spec needs to determine
  // if texture re-use is appropriate or not. If re-use is not specified then
  // it should at the very least be detached from the JavaScript wrapper and
  // reattached to a new one here. In both cases the texture should be
  // cleared.

  wgpu::DawnEncoderInternalUsageDescriptor internal_usage_desc = {{
      .useInternalUsages = true,
  }};
  wgpu::CommandEncoderDescriptor command_encoder_desc = {
      .nextInChain = &internal_usage_desc,
      .label = "XRGPUStaticSwapChain Clear",
  };
  wgpu::CommandEncoder command_encoder =
      device()->GetHandle().CreateCommandEncoder(&command_encoder_desc);

  ClearCurrentTexture(command_encoder);

  wgpu::CommandBuffer command_buffer = command_encoder.Finish();
  command_encoder = nullptr;

  device()->GetHandle().GetQueue().Submit(1u, &command_buffer);
  command_buffer = nullptr;

  // Intentionally not calling ResetCurrentTexture() here to keep the previously
  // produced texture for the next frame.
}

XRGPUMailboxSwapChain::XRGPUMailboxSwapChain(
    GPUDevice* device,
    const wgpu::TextureDescriptor& desc)
    : XRGPUSwapChain(device) {
  descriptor_ = desc;

  // TODO(crbug.com/359418629): Internal Usage will not be necessary once we can
  // use texture array mailboxes directly.
  wgpu::TextureUsage internal_usage = wgpu::TextureUsage::CopyDst;
  texture_internal_usage_ = {{
      .internalUsage = internal_usage,
  }};
  descriptor_.nextInChain = &texture_internal_usage_;
}

GPUTexture* XRGPUMailboxSwapChain::ProduceTexture() {
  const XRSharedImageData& content_image_data = layer()->SharedImage();

  // TODO(crbug.com/359418629): Allow for other mailboxes as well?
  CHECK(content_image_data.shared_image);

  scoped_refptr<WebGPUMailboxTexture> mailbox_texture =
      WebGPUMailboxTexture::FromExistingSharedImage(
          device()->GetDawnControlClient(), device()->GetHandle(), descriptor_,
          content_image_data.shared_image, content_image_data.sync_token);

  return MakeGarbageCollected<GPUTexture>(
      device(), descriptor_.format, descriptor_.usage,
      std::move(mailbox_texture), "WebXR Mailbox Swap Chain");
}

void XRGPUMailboxSwapChain::OnFrameEnd() {
  GPUTexture* texture = ResetCurrentTexture();
  if (texture) {
    texture->DissociateMailbox();
  }
}

}  // namespace blink
