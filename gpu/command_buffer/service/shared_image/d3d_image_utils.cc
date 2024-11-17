// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/d3d_image_utils.h"

#include <dawn/native/D3D11Backend.h>
#include <dawn/native/D3D12Backend.h>

#include "base/logging.h"
#include "base/notreached.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/config/gpu_finch_features.h"

using dawn::native::d3d11::SharedTextureMemoryD3D11Texture2DDescriptor;
using dawn::native::d3d12::SharedBufferMemoryD3D12ResourceDescriptor;

namespace gpu {

bool ClearD3D11TextureToColor(
    const Microsoft::WRL::ComPtr<ID3D11Texture2D>& d3d11_texture,
    const SkColor4f& color) {
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device;
  d3d11_texture->GetDevice(&d3d11_device);

  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> render_target;
  HRESULT hr = d3d11_device->CreateRenderTargetView(d3d11_texture.Get(),
                                                    nullptr, &render_target);
  if (FAILED(hr)) {
    LOG(ERROR) << "CreateRenderTargetView failed: "
               << logging::SystemErrorCodeToString(hr);
    return false;
  }
  DCHECK(render_target);

  Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_device_context;
  d3d11_device->GetImmediateContext(&d3d11_device_context);
  DCHECK(d3d11_device_context);

  d3d11_device_context->ClearRenderTargetView(render_target.Get(), color.vec());

  return true;
}

wgpu::Texture CreateDawnSharedTexture(
    const wgpu::SharedTextureMemory& shared_texture_memory,
    wgpu::TextureUsage usage,
    wgpu::TextureUsage internal_usage,
    base::span<wgpu::TextureFormat> view_formats) {
  wgpu::SharedTextureMemoryProperties properties;
  shared_texture_memory.GetProperties(&properties);

  wgpu::TextureDescriptor wgpu_texture_desc;
  wgpu_texture_desc.format = properties.format;
  wgpu_texture_desc.dimension = wgpu::TextureDimension::e2D;
  wgpu_texture_desc.size = properties.size;
  wgpu_texture_desc.mipLevelCount = 1;
  wgpu_texture_desc.sampleCount = 1;

  wgpu_texture_desc.usage = usage;
  wgpu_texture_desc.viewFormatCount =
      static_cast<uint32_t>(view_formats.size());
  wgpu_texture_desc.viewFormats = view_formats.data();

  wgpu::DawnTextureInternalUsageDescriptor wgpu_internal_usage_desc;
  wgpu_internal_usage_desc.internalUsage = internal_usage;
  wgpu_texture_desc.nextInChain = &wgpu_internal_usage_desc;

  return shared_texture_memory.CreateTexture(&wgpu_texture_desc);
}

wgpu::SharedTextureMemory CreateDawnSharedTextureMemory(
    const wgpu::Device& device,
    bool use_keyed_mutex,
    HANDLE handle) {
  wgpu::SharedTextureMemory shared_texture_memory;
  wgpu::SharedTextureMemoryDXGISharedHandleDescriptor shared_handle_desc;
  shared_handle_desc.handle = handle;
  shared_handle_desc.useKeyedMutex = use_keyed_mutex;

  wgpu::SharedTextureMemoryDescriptor desc;
  desc.nextInChain = &shared_handle_desc;
  desc.label = "SharedImageD3D_SharedTextureMemory_SharedHandle";

  shared_texture_memory = device.ImportSharedTextureMemory(&desc);

  if (!shared_texture_memory || shared_texture_memory.IsDeviceLost()) {
    LOG(ERROR) << "Failed to create shared texture memory";
    return nullptr;
  }

  return shared_texture_memory;
}

wgpu::SharedTextureMemory CreateDawnSharedTextureMemory(
    const wgpu::Device& device,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture) {
  wgpu::SharedTextureMemory shared_texture_memory;
  SharedTextureMemoryD3D11Texture2DDescriptor texture2d_desc;
  texture2d_desc.texture = texture;

  wgpu::SharedTextureMemoryDescriptor desc;
  desc.nextInChain = &texture2d_desc;
  desc.label = "SharedImageD3D_SharedTextureMemory_Texture2D";
  shared_texture_memory = device.ImportSharedTextureMemory(&desc);

  if (!shared_texture_memory || shared_texture_memory.IsDeviceLost()) {
    LOG(ERROR) << "Failed to create shared texture memory";
    return nullptr;
  }

  return shared_texture_memory;
}

wgpu::Buffer CreateDawnSharedBuffer(
    const wgpu::SharedBufferMemory& shared_buffer_memory,
    wgpu::BufferUsage usage) {
  wgpu::SharedBufferMemoryProperties properties;
  shared_buffer_memory.GetProperties(&properties);

  wgpu::BufferDescriptor wgpu_buffer_desc;
  wgpu_buffer_desc.size = properties.size;
  wgpu_buffer_desc.usage = usage;

  return shared_buffer_memory.CreateBuffer(&wgpu_buffer_desc);
}

wgpu::SharedBufferMemory CreateDawnSharedBufferMemory(
    const wgpu::Device& device,
    Microsoft::WRL::ComPtr<ID3D12Resource> resource) {
  wgpu::SharedBufferMemory shared_buffer_memory;
  SharedBufferMemoryD3D12ResourceDescriptor d3d12_resource_desc;
  d3d12_resource_desc.resource = std::move(resource);

  wgpu::SharedBufferMemoryDescriptor desc;
  desc.nextInChain = &d3d12_resource_desc;
  desc.label = "SharedBufferD3D_SharedBufferMemory_Resource";
  shared_buffer_memory = device.ImportSharedBufferMemory(&desc);

  if (!shared_buffer_memory) {
    LOG(ERROR) << "Failed to create shared buffer memory";
    return nullptr;
  }

  DCHECK(!shared_buffer_memory.IsDeviceLost());
  return shared_buffer_memory;
}

wgpu::SharedFence CreateDawnSharedFence(
    const wgpu::Device& device,
    scoped_refptr<gfx::D3DSharedFence> fence) {
  wgpu::SharedFence shared_fence;
  wgpu::SharedFenceDXGISharedHandleDescriptor dxgi_desc;
  dxgi_desc.handle = fence->GetSharedHandle();
  wgpu::SharedFenceDescriptor fence_desc;
  fence_desc.nextInChain = &dxgi_desc;

  shared_fence = device.ImportSharedFence(&fence_desc);

  if (!shared_fence) {
    LOG(ERROR) << "Failed to create shared fence.";
    return nullptr;
  }

  return shared_fence;
}
}  // namespace gpu
