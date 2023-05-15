// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_texture_alpha_clearer.h"

namespace blink {

WebGPUTextureAlphaClearer::WebGPUTextureAlphaClearer(
    scoped_refptr<DawnControlClientHolder> dawn_control_client,
    WGPUDevice device,
    WGPUTextureFormat format)
    : dawn_control_client_(std::move(dawn_control_client)),
      device_(device),
      format_(format) {
  const auto& procs = dawn_control_client_->GetProcs();

  procs.deviceReference(device_);

  WGPUShaderModuleWGSLDescriptor wgsl_desc = {
      .chain = {.sType = WGPUSType_ShaderModuleWGSLDescriptor},
      .code = R"(
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
    )",
  };
  WGPUShaderModuleDescriptor shader_module_desc = {.nextInChain =
                                                       &wgsl_desc.chain};
  WGPUShaderModule shader_module =
      procs.deviceCreateShaderModule(device_, &shader_module_desc);

  WGPUColorTargetState color_target = {
      .format = format,
      .writeMask = WGPUColorWriteMask_Alpha,
  };
  WGPUFragmentState fragment = {
      .module = shader_module,
      .entryPoint = "frag_main",
      .targetCount = 1,
      .targets = &color_target,
  };
  WGPURenderPipelineDescriptor pipeline_desc = {
      .vertex =
          {
              .module = shader_module,
              .entryPoint = "vert_main",
          },
      .primitive = {.topology = WGPUPrimitiveTopology_TriangleList},
      .multisample = {.count = 1, .mask = 0xFFFFFFFF},
      .fragment = &fragment,
  };
  alpha_to_one_pipeline_ =
      procs.deviceCreateRenderPipeline(device_, &pipeline_desc);
  procs.shaderModuleRelease(shader_module);
}

WebGPUTextureAlphaClearer::~WebGPUTextureAlphaClearer() {
  const auto& procs = dawn_control_client_->GetProcs();
  procs.renderPipelineRelease(alpha_to_one_pipeline_);
  procs.deviceRelease(device_);
}

bool WebGPUTextureAlphaClearer::IsCompatible(WGPUDevice device,
                                             WGPUTextureFormat format) const {
  return device_ == device && format_ == format;
}

void WebGPUTextureAlphaClearer::ClearAlpha(WGPUTexture texture) {
  const auto& procs = dawn_control_client_->GetProcs();

  // Push an error scope to capture errors here.
  procs.devicePushErrorScope(device_, WGPUErrorFilter_Validation);
  WGPUTextureView attachment_view = procs.textureCreateView(texture, nullptr);

  WGPUDawnEncoderInternalUsageDescriptor internal_usage_desc = {
      .chain = {.sType = WGPUSType_DawnEncoderInternalUsageDescriptor},
      .useInternalUsages = true,
  };
  WGPUCommandEncoderDescriptor command_encoder_desc = {
      .nextInChain = &internal_usage_desc.chain,
  };
  WGPUCommandEncoder command_encoder =
      procs.deviceCreateCommandEncoder(device_, &command_encoder_desc);

  WGPURenderPassColorAttachment color_attachment = {
      .view = attachment_view,
      .loadOp = WGPULoadOp_Load,
      .storeOp = WGPUStoreOp_Store,
  };
  WGPURenderPassDescriptor render_pass_desc = {
      .colorAttachmentCount = 1,
      .colorAttachments = &color_attachment,
  };
  WGPURenderPassEncoder pass =
      procs.commandEncoderBeginRenderPass(command_encoder, &render_pass_desc);
  DCHECK(alpha_to_one_pipeline_);
  procs.renderPassEncoderSetPipeline(pass, alpha_to_one_pipeline_);
  procs.renderPassEncoderDraw(pass, 3, 1, 0, 0);
  procs.renderPassEncoderEnd(pass);

  WGPUCommandBuffer command_buffer =
      procs.commandEncoderFinish(command_encoder, nullptr);

  WGPUQueue queue = procs.deviceGetQueue(device_);
  procs.queueSubmit(queue, 1, &command_buffer);

  procs.renderPassEncoderRelease(pass);
  procs.commandEncoderRelease(command_encoder);
  procs.commandBufferRelease(command_buffer);
  procs.textureViewRelease(attachment_view);

  // Pop the error scope and swallow errors. There are errors
  // when the configured canvas produces an error GPUTexture. Errors from
  // the alpha clear should be hidden from the application.
  procs.devicePopErrorScope(
      device_,
      [](WGPUErrorType type, const char* message, void*) {
        // There may be other error types like DeviceLost or Unknown if
        // the device is destroyed before we receive a response from the
        // GPU service.
        if (type == WGPUErrorType_Validation) {
          DLOG(ERROR) << "WebGPUTextureAlphaClearer errored:" << message;
        }
      },
      nullptr);
}

}  // namespace blink
