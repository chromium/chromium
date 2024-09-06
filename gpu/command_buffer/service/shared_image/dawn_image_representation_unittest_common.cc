// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/dawn_image_representation_unittest_common.h"

#include "base/threading/platform_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {
namespace {
constexpr size_t kBufferSizeMinAlignment = 256;

void CopyTexelToBuffer(const wgpu::CommandEncoder& encoder,
                       const wgpu::Texture& texture,
                       uint32_t x,
                       uint32_t y,
                       const wgpu::Buffer& buffer) {
  wgpu::ImageCopyBuffer buffer_copy;
  buffer_copy.buffer = buffer;
  buffer_copy.layout =
      wgpu::TextureDataLayout{.bytesPerRow = kBufferSizeMinAlignment};

  wgpu::ImageCopyTexture texture_copy;
  texture_copy.texture = texture;
  texture_copy.mipLevel = 0;
  texture_copy.origin = wgpu::Origin3D{.x = x, .y = y, .z = 0};

  wgpu::Extent3D extend = {1, 1, 1};

  encoder.CopyTextureToBuffer(&texture_copy, &buffer_copy, &extend);
}

}  // namespace

wgpu::ShaderModule CreateShaderModule(const wgpu::Device& device,
                                      const char* source) {
  wgpu::ShaderSourceWGSL wgsl_desc;
  wgsl_desc.code = source;
  wgpu::ShaderModuleDescriptor descriptor;
  descriptor.nextInChain = &wgsl_desc;
  return device.CreateShaderModule(&descriptor);
}

wgpu::RenderPipeline CreateRenderPipeline(
    const wgpu::Device& device,
    wgpu::ShaderModule vs_module,
    wgpu::ShaderModule fs_module,
    wgpu::TextureFormat render_pass_color_format) {
  wgpu::RenderPipelineDescriptor descriptor;

  wgpu::ColorTargetState color_target;
  color_target.format = render_pass_color_format;
  color_target.blend = nullptr;
  color_target.writeMask = wgpu::ColorWriteMask::All;

  wgpu::FragmentState fragment;
  fragment.module = std::move(fs_module);
  fragment.entryPoint = "main";
  fragment.targetCount = 1;
  fragment.targets = &color_target;
  descriptor.fragment = &fragment;

  descriptor.vertex.module = std::move(vs_module);
  descriptor.vertex.entryPoint = "main";

  descriptor.primitive.frontFace = wgpu::FrontFace::CCW;
  descriptor.primitive.cullMode = wgpu::CullMode::None;
  descriptor.primitive.topology = wgpu::PrimitiveTopology::TriangleStrip;
  descriptor.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined;

  descriptor.multisample.count = 1;

  return device.CreateRenderPipeline(&descriptor);
}

wgpu::Buffer CreateBuffer(const wgpu::Device& device,
                          uint32_t size,
                          wgpu::BufferUsage usage) {
  wgpu::BufferDescriptor descriptor;
  descriptor.size = size;
  descriptor.usage = usage;
  return device.CreateBuffer(&descriptor);
}

wgpu::Texture CreateTexture(const wgpu::Device& device,
                            uint32_t width,
                            uint32_t height,
                            wgpu::TextureUsage usage,
                            wgpu::TextureFormat format) {
  wgpu::TextureDescriptor descriptor = {};
  descriptor.size = {width, height, 1};
  descriptor.format = format;
  descriptor.usage = usage;
  descriptor.mipLevelCount = 1;
  descriptor.sampleCount = 1;
  return device.CreateTexture(&descriptor);
}

wgpu::TextureView CreateTextureView(const wgpu::Texture& texture,
                                    wgpu::TextureAspect aspect) {
  wgpu::TextureViewDescriptor descriptor;
  descriptor.arrayLayerCount = 1;
  descriptor.mipLevelCount = 1;
  descriptor.aspect = aspect;
  return texture.CreateView(&descriptor);
}

void RunDawnVideoSamplingTest(
    wgpu::Instance instance,
    wgpu::Device device,
    const std::unique_ptr<DawnImageRepresentation>& shared_image,
    uint8_t expected_y_value,
    uint8_t expected_u_value,
    uint8_t expected_v_value) {
  ASSERT_EQ(shared_image->format(), viz::MultiPlaneFormat::kNV12);

  const gfx::Size size = shared_image->size();

  // Render pipeline
  constexpr char kVS[] = R"(
struct VertexOut {
  @location(0) tex_coord : vec2 <f32>,
  @builtin(position) position : vec4f,
}

@vertex fn main(
  @builtin(vertex_index) vertex_index : u32,
) -> VertexOut {
  const pos = array(
      vec2f(-1.0, -1.0),
      vec2f( 3.0, -1.0),
      vec2f(-1.0,  3.0));

  var out_vert: VertexOut;
  out_vert.position = vec4f(pos[vertex_index], 0.0, 1.0);
  out_vert.tex_coord = vec2f(out_vert.position.xy * 0.5) + vec2f(0.5, 0.5);

  return out_vert;
}
)";

  constexpr char kFS[] = R"(
@group(0) @binding(0) var sampler0 : sampler;
@group(0) @binding(1) var y_tex : texture_2d<f32>;
@group(0) @binding(2) var uv_tex : texture_2d<f32>;

@fragment fn main(@location(0) tex_coord : vec2f) -> @location(0) vec4f {
  let y : f32 = textureSample(y_tex, sampler0, tex_coord).r;
  let uv : vec2f = textureSample(uv_tex, sampler0, tex_coord).rg;
  return vec4f(y, uv.r, uv.g, 1.0);
}

)";

  auto render_pipeline = CreateRenderPipeline(
      device, CreateShaderModule(device, kVS), CreateShaderModule(device, kFS),
      wgpu::TextureFormat::RGBA8Unorm);

  ASSERT_NE(render_pipeline, nullptr);

  auto texture_access = shared_image->BeginScopedAccess(
      wgpu::TextureUsage::TextureBinding,
      SharedImageRepresentation::AllowUnclearedAccess::kNo);
  ASSERT_NE(texture_access, nullptr);

  // Bind group
  wgpu::SamplerDescriptor sampler_desc;
  auto sampler = device.CreateSampler(&sampler_desc);
  ASSERT_NE(sampler, nullptr);

  std::array<wgpu::BindGroupEntry, 3> bind_group_entries;
  bind_group_entries[0].binding = 0;
  bind_group_entries[0].sampler = sampler;
  bind_group_entries[1].binding = 1;
  bind_group_entries[1].textureView = CreateTextureView(
      texture_access->texture(), wgpu::TextureAspect::Plane0Only);
  bind_group_entries[2].binding = 2;
  bind_group_entries[2].textureView = CreateTextureView(
      texture_access->texture(), wgpu::TextureAspect::Plane1Only);

  wgpu::BindGroupDescriptor bind_group_desc;
  bind_group_desc.entryCount = 3;
  bind_group_desc.entries = bind_group_entries.data();
  bind_group_desc.layout = render_pipeline.GetBindGroupLayout(0);

  auto bind_group = device.CreateBindGroup(&bind_group_desc);
  ASSERT_NE(bind_group, nullptr);

  // Pender pass
  auto render_target_texture = CreateTexture(
      device, size.width(), size.height(),
      wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc,
      wgpu::TextureFormat::RGBA8Unorm);
  ASSERT_NE(render_target_texture, nullptr);

  wgpu::RenderPassColorAttachment color_attachment;
  color_attachment.view = render_target_texture.CreateView();
  color_attachment.loadOp = wgpu::LoadOp::Clear;
  color_attachment.clearValue = wgpu::Color{0, 0, 0, 0};
  color_attachment.storeOp = wgpu::StoreOp::Store;

  wgpu::RenderPassDescriptor render_pass;
  render_pass.colorAttachmentCount = 1;
  render_pass.colorAttachments = &color_attachment;

  auto encoder = device.CreateCommandEncoder();
  auto render_pass_encoder = encoder.BeginRenderPass(&render_pass);
  ASSERT_NE(render_pass_encoder, nullptr);

  // Sampling the video texture and draw to the framebuffer.
  render_pass_encoder.SetPipeline(render_pipeline);
  render_pass_encoder.SetBindGroup(0, bind_group);
  render_pass_encoder.Draw(3);

  render_pass_encoder.End();

  // Readback the framebuffer's texture to buffer.
  auto readback_buffer =
      CreateBuffer(device, kBufferSizeMinAlignment,
                   wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead);
  ASSERT_NE(readback_buffer, nullptr);

  CopyTexelToBuffer(encoder, render_target_texture, /*x=*/size.width() / 2,
                    /*y=*/size.height() / 2, readback_buffer);

  auto command_buffer = encoder.Finish();
  ASSERT_NE(command_buffer, nullptr);

  device.GetQueue().Submit(1, &command_buffer);

  wgpu::FutureWaitInfo wait_info{readback_buffer.MapAsync(
      wgpu::MapMode::Read, 0, wgpu::kWholeMapSize,
      wgpu::CallbackMode::WaitAnyOnly,
      [](wgpu::MapAsyncStatus status, const char*) {
        ASSERT_EQ(status, wgpu::MapAsyncStatus::Success);
      })};

  wgpu::WaitStatus status =
      instance.WaitAny(1, &wait_info, std::numeric_limits<uint64_t>::max());
  DCHECK(status == wgpu::WaitStatus::Success);

  uint8_t pixel_color[4];

  memcpy(pixel_color, readback_buffer.GetConstMappedRange(),
         sizeof(pixel_color));

  EXPECT_EQ(expected_y_value, pixel_color[0]);
  EXPECT_EQ(expected_u_value, pixel_color[1]);
  EXPECT_EQ(expected_v_value, pixel_color[2]);
  EXPECT_EQ(255, pixel_color[3]);
}

}  // namespace gpu
