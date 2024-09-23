// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_D3D_IMAGE_UTILS_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_D3D_IMAGE_UTILS_H_

#include <windows.h>

#include <d3d11.h>
#include <d3d12.h>
#include <wrl/client.h>

// clang-format off
#include <webgpu/webgpu_cpp.h>
// clang-format on

#include "base/containers/span.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/win/d3d_shared_fence.h"
#include "ui/gl/buildflags.h"

namespace gpu {

bool ClearD3D11TextureToColor(
    const Microsoft::WRL::ComPtr<ID3D11Texture2D>& d3d11_texture,
    const SkColor4f& color);

wgpu::Texture CreateDawnSharedTexture(
    const wgpu::SharedTextureMemory& shared_texture_memory,
    wgpu::TextureUsage usage,
    wgpu::TextureUsage internal_usage,
    base::span<wgpu::TextureFormat> view_formats);

wgpu::SharedTextureMemory CreateDawnSharedTextureMemory(
    const wgpu::Device& device,
    bool use_keyed_mutex,
    HANDLE handle);

wgpu::SharedTextureMemory CreateDawnSharedTextureMemory(
    const wgpu::Device& device,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture);

wgpu::Buffer CreateDawnSharedBuffer(
    const wgpu::SharedBufferMemory& shared_buffer_memory,
    wgpu::BufferUsage usage);

wgpu::SharedBufferMemory CreateDawnSharedBufferMemory(
    const wgpu::Device& device,
    Microsoft::WRL::ComPtr<ID3D12Resource> resource);

wgpu::SharedFence CreateDawnSharedFence(
    const wgpu::Device& device,
    scoped_refptr<gfx::D3DSharedFence> fence);

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_D3D_IMAGE_UTILS_H_
