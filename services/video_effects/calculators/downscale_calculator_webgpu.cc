// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_effects/calculators/downscale_calculator_webgpu.h"

#include "base/bit_cast.h"
#include "base/strings/strcat.h"
#include "base/strings/to_string.h"
#include "services/video_effects/calculators/mediapipe_webgpu_utils.h"
#include "services/video_effects/calculators/video_effects_graph_config.h"
#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/dawn/include/dawn/webgpu_cpp_print.h"
#include "third_party/mediapipe/src/mediapipe/framework/calculator_context.h"
#include "third_party/mediapipe/src/mediapipe/framework/calculator_contract.h"
#include "third_party/mediapipe/src/mediapipe/framework/calculator_registry.h"
#include "third_party/mediapipe/src/mediapipe/framework/packet.h"
#include "third_party/mediapipe/src/mediapipe/gpu/gpu_buffer.h"
#include "third_party/mediapipe/src/mediapipe/gpu/gpu_buffer_format.h"
#include "third_party/mediapipe/src/mediapipe/gpu/webgpu/webgpu_service.h"
#include "third_party/mediapipe/src/mediapipe/gpu/webgpu/webgpu_texture_view.h"

namespace video_effects {

namespace {

wgpu::RenderPipeline CreateRenderPipelineForTextureCopy(
    wgpu::Device device,
    wgpu::TextureFormat destination_format) {
  std::vector<wgpu::BindGroupLayoutEntry> entries;
  entries.push_back({
      .binding = 0,
      .visibility = wgpu::ShaderStage::Fragment,
      .sampler =
          {
              .type = wgpu::SamplerBindingType::NonFiltering,
          },
  });

  entries.push_back(
      {.binding = 1,
       .visibility = wgpu::ShaderStage::Fragment,
       .texture = {
           .sampleType = wgpu::TextureSampleType::UnfilterableFloat,
           .viewDimension = wgpu::TextureViewDimension::e2D,
           .multisampled = false,
       }});

  entries.push_back({
      .binding = 2,
      .visibility = wgpu::ShaderStage::Fragment,
      .buffer =
          {
              .type = wgpu::BufferBindingType::Uniform,
          },
  });

  const std::string bind_group_layout_label =
      base::StrCat({"DownscaleCalculatorTextureCopyBindGroup::",
                    base::ToString(destination_format)});
  wgpu::BindGroupLayoutDescriptor bind_group_layout_descriptor = {
      .label = bind_group_layout_label.c_str(),
      .entryCount = entries.size(),
      .entries = entries.data(),
  };
  wgpu::BindGroupLayout bind_group_layout =
      device.CreateBindGroupLayout(&bind_group_layout_descriptor);

  const std::string pipeline_layout_label =
      base::StrCat({"DownscaleCalculatorTextureCopyPipelineLayout::",
                    base::ToString(destination_format)});
  wgpu::PipelineLayoutDescriptor pipeline_layout_descriptor = {
      .label = pipeline_layout_label.c_str(),
      .bindGroupLayoutCount = 1,
      .bindGroupLayouts = &bind_group_layout,
  };

  wgpu::ShaderModuleWGSLDescriptor shader_module_wgsl_descriptor;
  shader_module_wgsl_descriptor.code = R"(

struct Uniforms {
    perform_y_flip: u32,    // bools are not host-shareable
}

@group(0) @binding(0) var sourceTextureSampler: sampler;
@group(0) @binding(1) var sourceTexture: texture_2d<f32>;
@group(0) @binding(2) var<uniform> uniforms: Uniforms;

struct VertexOutput {
  @builtin(position) position: vec4f,
  @location(0) uv: vec2f,
};

@vertex
fn vertex_main(@builtin(vertex_index) in_vertex_index: u32) -> VertexOutput {
  // x,y coordinates of our triangles, in clip space:
  var triangle_points = array<vec2<f32>, 6>(
    // 1st triangle:
    vec2<f32>(-1.0, -1.0), // lower left
    vec2<f32>( 1.0,  1.0), // upper right
    vec2<f32>(-1.0,  1.0), // upper left
    // 2nd triangle:
    vec2<f32>(-1.0, -1.0), // lower left
    vec2<f32>( 1.0, -1.0), // lower right
    vec2<f32>( 1.0,  1.0), // upper right
  );

  let xy = triangle_points[in_vertex_index];

  var out: VertexOutput;
  // `position` is a vec4 so let's extend x,y with z=0.0 (at near plane), w=1.0.
  // (with w=1.0, x,y,z will be unchanged during clip space -> NDC conversion)
  out.position = vec4f(xy, 0.0, 1.0);
  // Transform from clip space to UV:
  out.uv = xy.xy * 0.5 + 0.5;   // [-1; 1] => [0; 1]
  return out;
}

@fragment
fn fragment_main(in: VertexOutput) -> @location(0) vec4f {
  let uvs = select(
    in.uv, vec2f(in.uv.x, 1.0 - in.uv.y), uniforms.perform_y_flip == 1);
  return textureSample(sourceTexture, sourceTextureSampler, uvs);
}
  )";

  const std::string shader_label =
      base::StrCat({"DownscaleCalculatorTextureCopyShaderModule::",
                    base::ToString(destination_format)});
  wgpu::ShaderModuleDescriptor shader_module_descriptor = {
      .nextInChain = &shader_module_wgsl_descriptor,
      .label = shader_label.c_str(),
  };
  wgpu::ShaderModule shader_module =
      device.CreateShaderModule(&shader_module_descriptor);

  wgpu::ColorTargetState color_target_state = {
      .format = destination_format,
      .blend = nullptr,
      .writeMask = wgpu::ColorWriteMask::All,
  };
  wgpu::FragmentState fragment_state = {
      .module = shader_module,
      .entryPoint = "fragment_main",
      .constantCount = 0,
      .constants = nullptr,
      .targetCount = 1,
      .targets = &color_target_state,
  };

  const std::string pipeline_label =
      base::StrCat({"DownscaleCalculatorTextureCopyRenderPipeline::",
                    base::ToString(destination_format)});
  wgpu::RenderPipelineDescriptor pipeline_descriptor = {
      .label = pipeline_label.c_str(),
      .layout = device.CreatePipelineLayout(&pipeline_layout_descriptor),
      .vertex =
          {
              .module = shader_module,
              .entryPoint = "vertex_main",
              .constantCount = 0,
              .constants = nullptr,
          },
      .primitive =
          {
              .topology = wgpu::PrimitiveTopology::TriangleList,
              .stripIndexFormat = wgpu::IndexFormat::Undefined,
              .frontFace = wgpu::FrontFace::CCW,
              .cullMode = wgpu::CullMode::Back,
          },
      .depthStencil = nullptr,  // no need for depth test
      .multisample = {},        // no need to multisample
      .fragment = &fragment_state,
  };

  return device.CreateRenderPipeline(&pipeline_descriptor);
}

}  // namespace

DownscaleCalculatorWebGpu::DownscaleCalculatorWebGpu() = default;
DownscaleCalculatorWebGpu::~DownscaleCalculatorWebGpu() = default;

absl::Status DownscaleCalculatorWebGpu::GetContract(
    mediapipe::CalculatorContract* cc) {
  cc->UseService(mediapipe::kWebGpuService);

  cc->Inputs().Tag(kInputStreamTag).Set<mediapipe::GpuBuffer>();

  cc->Outputs().Tag(kOutputStreamTag).Set<mediapipe::GpuBuffer>();
  return absl::OkStatus();
}

absl::Status DownscaleCalculatorWebGpu::Open(mediapipe::CalculatorContext* cc) {
  device_ = cc->Service(mediapipe::kWebGpuService).GetObject().device();
  if (!device_) {
    return absl::InternalError(
        "Failed to obtain the WebGPU device from the service!");
  }

  wgpu::BufferDescriptor uniforms_descriptor = {
      .label = "VideoEffectsProcessorTextureCopyUniforms",
      .usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
      .size = sizeof(uint32_t),
      .mappedAtCreation = false,
  };
  texture_copy_uniforms_buffer_ = device_.CreateBuffer(&uniforms_descriptor);

  render_pipeline_ = CreateRenderPipelineForTextureCopy(
      device_, wgpu::TextureFormat::RGBA32Float);

  return absl::OkStatus();
}

absl::Status DownscaleCalculatorWebGpu::Process(
    mediapipe::CalculatorContext* cc) {
  mediapipe::Packet& input_packet = cc->Inputs().Tag(kInputStreamTag).Value();
  auto input_buffer = input_packet.Get<mediapipe::GpuBuffer>();
  auto input_texture_view =
      input_buffer.GetReadView<mediapipe::WebGpuTextureView>();

  mediapipe::GpuBuffer output_buffer(
      kInferenceInputBufferWidth, kInferenceInputBufferHeight,
      WebGpuTextureFormatToGpuBufferFormat(kInferenceInputBufferFormat));
  auto output_texture_view =
      output_buffer.GetWriteView<mediapipe::WebGpuTextureView>();

  wgpu::CommandEncoderDescriptor command_encoder_descriptor = {
      .label = "DownscaleCalculatorComandEncoder"};
  wgpu::CommandEncoder command_encoder =
      device_.CreateCommandEncoder(&command_encoder_descriptor);

  wgpu::RenderPassColorAttachment destination_color_attachment = {
      .view = output_texture_view.texture().CreateView(),
      .loadOp = wgpu::LoadOp::Clear,
      .storeOp = wgpu::StoreOp::Store,
      .clearValue =
          {
              .r = 1.0,
              .g = 0.0,
              .b = 0.0,
              .a = 1.0,
          },
  };

  const std::vector<wgpu::BindGroupEntry> entries = {
      {.binding = 0, .sampler = device_.CreateSampler()},
      {.binding = 1, .textureView = input_texture_view.texture().CreateView()},
      {.binding = 2, .buffer = texture_copy_uniforms_buffer_},
  };

  wgpu::BindGroupDescriptor bind_group_descriptor = {
      .label = "DownscaleCalculatorBindGroup",
      .layout = render_pipeline_.GetBindGroupLayout(0),
      .entryCount = entries.size(),
      .entries = entries.data(),
  };

  const uint32_t uniforms_perform_y_flip = true;
  auto uniforms_bytes = base::bit_cast<
      std::array<const uint8_t, sizeof(uniforms_perform_y_flip)>>(
      uniforms_perform_y_flip);
  command_encoder.WriteBuffer(texture_copy_uniforms_buffer_, 0,
                              uniforms_bytes.data(), uniforms_bytes.size());

  wgpu::RenderPassDescriptor render_pass_descriptor = {
      .label = "DownscaleCalculatorRenderPass",
      .colorAttachmentCount = 1,
      .colorAttachments = &destination_color_attachment,
  };
  wgpu::RenderPassEncoder render_pass_encoder =
      command_encoder.BeginRenderPass(&render_pass_descriptor);
  render_pass_encoder.SetPipeline(render_pipeline_);
  render_pass_encoder.SetBindGroup(
      0, device_.CreateBindGroup(&bind_group_descriptor));
  render_pass_encoder.Draw(6);
  render_pass_encoder.End();

  wgpu::CommandBufferDescriptor command_buffer_descriptor = {
      .label = "DownscaleCalculatorCommandBuffer"};
  auto command_buffer = command_encoder.Finish(&command_buffer_descriptor);
  device_.GetQueue().Submit(1, &command_buffer);

  cc->Outputs()
      .Tag(kOutputStreamTag)
      .AddPacket(
          mediapipe::MakePacket<mediapipe::GpuBuffer>(std::move(output_buffer))
              .At(input_packet.Timestamp()));
  return absl::OkStatus();
}

absl::Status DownscaleCalculatorWebGpu::Close(
    mediapipe::CalculatorContext* cc) {
  return absl::OkStatus();
}

REGISTER_CALCULATOR(DownscaleCalculatorWebGpu)

}  // namespace video_effects
