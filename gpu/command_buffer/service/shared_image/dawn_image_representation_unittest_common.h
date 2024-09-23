// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DAWN_IMAGE_REPRESENTATION_UNITTEST_COMMON_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DAWN_IMAGE_REPRESENTATION_UNITTEST_COMMON_H_

#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"

namespace gpu {
void RunDawnVideoSamplingTest(
    wgpu::Instance instance,
    wgpu::Device device,
    const std::unique_ptr<DawnImageRepresentation>& shared_image,
    uint8_t expected_y_value,
    uint8_t expected_u_value,
    uint8_t expected_v_value);

wgpu::ShaderModule CreateShaderModule(const wgpu::Device& device,
                                      const char* source);

wgpu::RenderPipeline CreateRenderPipeline(
    const wgpu::Device& device,
    wgpu::ShaderModule vs_module,
    wgpu::ShaderModule fs_module,
    wgpu::TextureFormat render_pass_color_format);

wgpu::Buffer CreateBuffer(const wgpu::Device& device,
                          uint32_t size,
                          wgpu::BufferUsage usage);

wgpu::Texture CreateTexture(const wgpu::Device& device,
                            uint32_t width,
                            uint32_t height,
                            wgpu::TextureUsage usage,
                            wgpu::TextureFormat format);

wgpu::TextureView CreateTextureView(const wgpu::Texture& texture,
                                    wgpu::TextureAspect aspect);
}  // namespace gpu
#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DAWN_IMAGE_REPRESENTATION_UNITTEST_COMMON_H_
