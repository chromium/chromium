// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_texture_alpha_clearer.h"

namespace blink {

WebGPUTextureAlphaClearer::WebGPUTextureAlphaClearer(
    scoped_refptr<DawnControlClientHolder> dawn_control_client,
    const wgpu::Device& device,
    wgpu::TextureFormat format)
    : dawn_control_client_(std::move(dawn_control_client)),
      device_(device),
      format_(format) {
  wgpu::ShaderSourceWGSL wgsl_desc = {};
  wgsl_desc.code = R"(
    // Internal shader used to clear the alpha channel of a texture.
    @vertex fn vert_main(@builtin(vertex_index) VertexIndex : u32) -> @builtin(position) vec4<f32> {
        var pos = array<vec2<f32>, 3>(
            vec2<f32>(-1.0, -1.0),
            vec2<f32>( 3.0, -1.0),
            vec2<f32>(-1.0,  3.0));
        return vec4<f32>(pos[VertexIndex], 0.0, 1.0);
    }

    @fragment fn frag_main() -> @location(0) vec4<f32> {
        return vec4<f32>(1.0);
    }
    )";
  wgpu::ShaderModuleDescriptor shader_module_desc = {.nextInChain = &wgsl_desc};
  wgpu::ShaderModule shader_module =
      device_.CreateShaderModule(&shader_module_desc);

  wgpu::ColorTargetState color_target = {
      .format = format,
      .writeMask = wgpu::ColorWriteMask::Alpha,
  };
  wgpu::FragmentState fragment = {
      .module = shader_module,
      .targetCount = 1,
      .targets = &color_target,
  };
  wgpu::RenderPipelineDescriptor pipeline_desc = {
      .vertex = {.module = shader_module},
      .primitive = {.topology = wgpu::PrimitiveTopology::TriangleList},
      .multisample = {.count = 1, .mask = 0xFFFFFFFF},
      .fragment = &fragment,
  };
  alpha_to_one_pipeline_ = device_.CreateRenderPipeline(&pipeline_desc);
}

WebGPUTextureAlphaClearer::~WebGPUTextureAlphaClearer() = default;

bool WebGPUTextureAlphaClearer::IsCompatible(const wgpu::Device& device,
                                             wgpu::TextureFormat format) const {
  return device_.Get() == device.Get() && format_ == format;
}

void WebGPUTextureAlphaClearer::ClearAlpha(const wgpu::Texture& texture) {
  // Push an error scope to capture errors here.
  device_.PushErrorScope(wgpu::ErrorFilter::Validation);
  wgpu::TextureView attachment_view = texture.CreateView();

  wgpu::DawnEncoderInternalUsageDescriptor internal_usage_desc = {};
  internal_usage_desc.useInternalUsages = true;

  wgpu::CommandEncoderDescriptor command_encoder_desc = {
      .nextInChain = &internal_usage_desc,
  };
  wgpu::CommandEncoder command_encoder =
      device_.CreateCommandEncoder(&command_encoder_desc);

  wgpu::RenderPassColorAttachment color_attachment = {
      .view = attachment_view,
      // The depthSlice must be initialized with the 'undefined' value for 2d
      // color attachments.
      .depthSlice = wgpu::kDepthSliceUndefined,
      .loadOp = wgpu::LoadOp::Load,
      .storeOp = wgpu::StoreOp::Store,
  };
  wgpu::RenderPassDescriptor render_pass_desc = {
      .colorAttachmentCount = 1,
      .colorAttachments = &color_attachment,
  };
  wgpu::RenderPassEncoder pass =
      command_encoder.BeginRenderPass(&render_pass_desc);
  DCHECK(alpha_to_one_pipeline_);
  pass.SetPipeline(alpha_to_one_pipeline_);
  pass.Draw(3, 1, 0, 0);
  pass.End();

  wgpu::CommandBuffer command_buffer = command_encoder.Finish();

  device_.GetQueue().Submit(1, &command_buffer);

  // Pop the error scope and swallow errors. There are errors
  // when the configured canvas produces an error GPUTexture. Errors from
  // the alpha clear should be hidden from the application.
  device_.PopErrorScope(
      wgpu::CallbackMode::AllowSpontaneous,
      [](wgpu::PopErrorScopeStatus, wgpu::ErrorType type, const char* message) {
        // There may be other error types like DeviceLost or
        // Unknown if the device is destroyed before we
        // receive a response from the GPU service.
        if (type == wgpu::ErrorType::Validation) {
          DLOG(ERROR) << "WebGPUTextureAlphaClearer errored:" << message;
        }
      });
}

}  // namespace blink
