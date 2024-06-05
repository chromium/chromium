// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/d3d_image_utils.h"

#include <dawn/native/D3D11Backend.h>

#include "base/logging.h"
#include "base/notreached.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/config/gpu_finch_features.h"

using dawn::native::d3d11::SharedTextureMemoryD3D11Texture2DDescriptor;

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
  if (base::FeatureList::IsEnabled(
          features::kDawnSIRepsUseClientProvidedInternalUsages)) {
    wgpu_internal_usage_desc.internalUsage = internal_usage;
  } else {
    // We need to have internal usages of CopySrc for copies,
    // RenderAttachment for clears, and TextureBinding for copyTextureForBrowser
    // if texture format allows these usages.
    wgpu_internal_usage_desc.internalUsage = properties.usage;
  }
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

}  // namespace gpu
