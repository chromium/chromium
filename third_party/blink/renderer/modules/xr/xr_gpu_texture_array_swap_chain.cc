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
    wgpu::TextureFormat format,
    uint32_t layers)
    : XRGPUSwapChain(device), wrapped_swap_chain_(wrapped_swap_chain) {
  CHECK(wrapped_swap_chain_);

  // Copy the wrapped swap chain's descriptor and divide its width by the
  // number of requested layers.
  CHECK_EQ(descriptor_.size.width % layers, 0ul);
  descriptor_ = wrapped_swap_chain->descriptor();
  descriptor_.label = "XRGPUTextureArraySwapChain";
  descriptor_.format = format;
  descriptor_.size = {descriptor_.size.width / layers, descriptor_.size.height,
                      layers};
  descriptor_.usage |= wgpu::TextureUsage::TextureBinding;

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

  GPUTexture* source_texture = GetCurrentTexture();
  GPUTexture* wrapped_texture = wrapped_swap_chain_->GetCurrentTexture();

  wgpu::DawnEncoderInternalUsageDescriptor internal_usage_desc = {{
      .useInternalUsages = true,
  }};
  wgpu::CommandEncoderDescriptor command_encoder_desc = {
      .nextInChain = &internal_usage_desc,
      .label = "XRGPUTextureArraySwapChain Direct Copy",
  };
  wgpu::CommandEncoder command_encoder =
      device()->GetHandle().CreateCommandEncoder(&command_encoder_desc);

  if (source_texture->format() == wrapped_texture->format()) {
    DirectCopy(command_encoder, source_texture, wrapped_texture);
  } else {
    RenderCopy(command_encoder, source_texture, wrapped_texture);
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

// Copy the texture layers into the wrapped swapchain with CopyTextureToTexture.
// This is the most efficient, but only works if the textures have the same
// format.
void XRGPUTextureArraySwapChain::DirectCopy(
    wgpu::CommandEncoder command_encoder,
    GPUTexture* source_texture,
    GPUTexture* dest_texture) {
  CHECK_EQ(source_texture->Format(), dest_texture->Format());

  wgpu::TexelCopyTextureInfo source = {
      .texture = source_texture->GetHandle(),
      .aspect = wgpu::TextureAspect::All,
  };
  wgpu::TexelCopyTextureInfo destination = {
      .texture = dest_texture->GetHandle(),
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
}

// Copy the texture layers into the wrapped swapchain with a render pass.
// This is the less efficient, but works for textures with differing formats.
void XRGPUTextureArraySwapChain::RenderCopy(
    wgpu::CommandEncoder command_encoder,
    GPUTexture* source_texture,
    GPUTexture* dest_texture) {
  CHECK(source_texture->Format() != dest_texture->Format());

  wgpu::TextureViewDescriptor dest_view_desc = {
      .dimension = wgpu::TextureViewDimension::e2D,
      .baseMipLevel = 0,
      .mipLevelCount = 1,
      .baseArrayLayer = 0,
      .arrayLayerCount = 1,
  };
  wgpu::TextureView dest_view =
      dest_texture->GetHandle().CreateView(&dest_view_desc);

  wgpu::RenderPassColorAttachment color_attachment = {
      .view = dest_view,
      .loadOp = wgpu::LoadOp::Clear,
      .storeOp = wgpu::StoreOp::Store,
      .clearValue = {0, 0, 0, 0},
  };
  wgpu::RenderPassDescriptor render_pass_desc = {
      .colorAttachmentCount = 1,
      .colorAttachments = &color_attachment,
  };
  wgpu::RenderPassEncoder render_pass =
      command_encoder.BeginRenderPass(&render_pass_desc);

  render_pass.SetPipeline(GetCopyPipelineForFormat(dest_texture->Format()));

  // Loop through each layer of the source texture and copy it to the
  // corresponding viewport of the destination texture.
  for (uint32_t i = 0; i < source_texture->depthOrArrayLayers(); ++i) {
    render_pass.SetViewport(source_texture->width() * i, 0,
                            source_texture->width(), source_texture->height(),
                            0, 1);

    wgpu::TextureViewDescriptor source_view_desc = {
        .dimension = wgpu::TextureViewDimension::e2D,
        .mipLevelCount = 1,
        .baseArrayLayer = i,
        .arrayLayerCount = 1,
    };
    wgpu::TextureView source_view =
        source_texture->GetHandle().CreateView(&source_view_desc);

    wgpu::BindGroupEntry source_binding = {
        .binding = 0,
        .textureView = source_view,
    };
    wgpu::BindGroupDescriptor source_bind_group_desc = {
        .layout = copy_pipeline_.GetBindGroupLayout(0),
        .entryCount = 1,
        .entries = &source_binding,
    };
    wgpu::BindGroup source_bind_group =
        device()->GetHandle().CreateBindGroup(&source_bind_group_desc);
    render_pass.SetBindGroup(0, source_bind_group);

    // Draw 3 vertices, comprising a triangle that covers the entire viewport.
    render_pass.Draw(3);
  }

  render_pass.End();
}

wgpu::RenderPipeline XRGPUTextureArraySwapChain::GetCopyPipelineForFormat(
    wgpu::TextureFormat format) {
  // Check to see if we have a copy pipeline for the appropriate format. If not,
  // create one.
  if (copy_pipeline_format_ != format) {
    wgpu::ShaderSourceWGSL wgsl_desc = {};
    wgsl_desc.code = R"(
      // Internal shader used to copy between two textures
      @group(0) @binding(0) var source: texture_2d<f32>;

      struct VertexOut {
        @builtin(position) pos: vec4f,
        @location(0) uv: vec2f,
      }

      @vertex fn vert_main(@builtin(vertex_index) vertexIndex : u32) -> VertexOut {
          var pos = array<vec2f, 3>(
              vec2f(-1.0, -1.0),
              vec2f( 3.0, -1.0),
              vec2f(-1.0,  3.0));
          return VertexOut(
            vec4f(pos[vertexIndex], 0.0, 1.0),
            vec2f(pos[vertexIndex].xy * vec2f(0.5, -0.5) + 0.5)
          );
      }

      @fragment fn frag_main(in: VertexOut) -> @location(0) vec4<f32> {
          let uv = vec2u(in.uv.xy * vec2f(textureDimensions(source)));
          return textureLoad(source, uv, 0);
      }
    )";
    wgpu::ShaderModuleDescriptor shader_module_desc = {.nextInChain =
                                                           &wgsl_desc};
    wgpu::ShaderModule shader_module =
        device()->GetHandle().CreateShaderModule(&shader_module_desc);

    wgpu::ColorTargetState color_target = {.format = format};
    wgpu::FragmentState fragment = {
        .module = shader_module,
        .targetCount = 1,
        .targets = &color_target,
    };
    wgpu::RenderPipelineDescriptor copy_pipeline_desc = {
        .vertex = {.module = shader_module},
        .fragment = &fragment,
    };

    copy_pipeline_ =
        device()->GetHandle().CreateRenderPipeline(&copy_pipeline_desc);
    copy_pipeline_format_ = format;
  }

  return copy_pipeline_;
}

void XRGPUTextureArraySwapChain::Trace(Visitor* visitor) const {
  visitor->Trace(wrapped_swap_chain_);
  XRGPUSwapChain::Trace(visitor);
}

}  // namespace blink
