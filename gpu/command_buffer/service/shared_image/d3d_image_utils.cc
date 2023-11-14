// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/d3d_image_utils.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "gpu/command_buffer/common/shared_image_usage.h"

#if BUILDFLAG(USE_DAWN)
using dawn::native::ExternalImageDescriptor;
using dawn::native::d3d::ExternalImageDescriptorD3D11Texture;
using dawn::native::d3d::ExternalImageDescriptorDXGISharedHandle;
using dawn::native::d3d::ExternalImageDXGI;
#endif

namespace gpu {

#if BUILDFLAG(USE_DAWN)
namespace {

wgpu::TextureFormat DXGIToWGPUFormat(DXGI_FORMAT dxgi_format) {
  switch (dxgi_format) {
    case DXGI_FORMAT_R8G8B8A8_UNORM:
      return wgpu::TextureFormat::RGBA8Unorm;
    case DXGI_FORMAT_B8G8R8A8_UNORM:
      return wgpu::TextureFormat::BGRA8Unorm;
    case DXGI_FORMAT_R8_UNORM:
      return wgpu::TextureFormat::R8Unorm;
    case DXGI_FORMAT_R8G8_UNORM:
      return wgpu::TextureFormat::RG8Unorm;
    case DXGI_FORMAT_R16_UNORM:
      return wgpu::TextureFormat::R16Unorm;
    case DXGI_FORMAT_R16G16_UNORM:
      return wgpu::TextureFormat::RG16Unorm;
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
      return wgpu::TextureFormat::RGBA16Float;
    case DXGI_FORMAT_R10G10B10A2_UNORM:
      return wgpu::TextureFormat::RGB10A2Unorm;
    case DXGI_FORMAT_NV12:
      return wgpu::TextureFormat::R8BG8Biplanar420Unorm;
    case DXGI_FORMAT_P010:
      return wgpu::TextureFormat::R10X6BG10X6Biplanar420Unorm;
    default:
      NOTREACHED();
      return wgpu::TextureFormat::Undefined;
  }
}

wgpu::TextureUsage GetAllowedDawnUsages(
    const wgpu::Device& device,
    const D3D11_TEXTURE2D_DESC& d3d11_texture_desc,
    const wgpu::TextureFormat wgpu_format) {
  DCHECK_EQ(wgpu_format, DXGIToWGPUFormat(d3d11_texture_desc.Format));
  if (wgpu_format == wgpu::TextureFormat::R8BG8Biplanar420Unorm ||
      wgpu_format == wgpu::TextureFormat::R10X6BG10X6Biplanar420Unorm) {
    // The bi-planar 420 formats are only supported as a texture binding.
    return wgpu::TextureUsage::TextureBinding;
  }

  wgpu::TextureUsage wgpu_usage =
      wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::CopyDst;
  if (d3d11_texture_desc.BindFlags & D3D11_BIND_RENDER_TARGET) {
    wgpu_usage |= wgpu::TextureUsage::RenderAttachment;
  }
  if (d3d11_texture_desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) {
    wgpu_usage |= wgpu::TextureUsage::TextureBinding;
  }
  if (d3d11_texture_desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS) {
    if (wgpu_format != wgpu::TextureFormat::BGRA8Unorm ||
        device.HasFeature(wgpu::FeatureName::BGRA8UnormStorage)) {
      wgpu_usage |= wgpu::TextureUsage::StorageBinding;
    }
  }

  return wgpu_usage;
}

}  // namespace
#endif  // BUILDFLAG(USE_DAWN)

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

#if BUILDFLAG(USE_DAWN)
std::unique_ptr<ExternalImageDXGI> CreateDawnExternalImageDXGI(
    const wgpu::Device& device,
    uint32_t shared_image_usage,
    const D3D11_TEXTURE2D_DESC& d3d11_texture_desc,
    absl::variant<HANDLE, Microsoft::WRL::ComPtr<ID3D11Texture2D>>
        handle_or_texture,
    base::span<wgpu::TextureFormat> view_formats) {
  const wgpu::TextureFormat wgpu_format =
      DXGIToWGPUFormat(d3d11_texture_desc.Format);
  if (wgpu_format == wgpu::TextureFormat::Undefined) {
    LOG(ERROR) << "Unsupported DXGI_FORMAT found: "
               << d3d11_texture_desc.Format;
    return nullptr;
  }

  // The below usages are not supported for multiplanar formats in Dawn.
  // TODO(crbug.com/1451784): Use read/write intent instead of format to get
  // correct usages. This needs support in Skia to loosen TextureUsage
  // validation. Alternatively, add support in Dawn for multiplanar formats to
  // be Renderable.
  wgpu::TextureUsage wgpu_allowed_usage =
      GetAllowedDawnUsages(device, d3d11_texture_desc, wgpu_format);
  if (wgpu_allowed_usage == wgpu::TextureUsage::None) {
    LOG(ERROR)
        << "Allowed wgpu::TextureUsage is unknown for wgpu::TextureFormat: "
        << static_cast<int>(wgpu_format);
    return nullptr;
  }

  if (shared_image_usage & SHARED_IMAGE_USAGE_WEBGPU_STORAGE_TEXTURE &&
      !(wgpu_allowed_usage & wgpu::TextureUsage::StorageBinding)) {
    LOG(ERROR) << "Storage binding is not allowed for wgpu::TextureFormat: "
               << static_cast<int>(wgpu_format);
    return nullptr;
  }

  wgpu::TextureDescriptor wgpu_texture_desc;
  wgpu_texture_desc.format = wgpu_format;
  wgpu_texture_desc.usage = wgpu_allowed_usage;
  wgpu_texture_desc.dimension = wgpu::TextureDimension::e2D;
  wgpu_texture_desc.size = {d3d11_texture_desc.Width, d3d11_texture_desc.Height,
                            1};
  wgpu_texture_desc.mipLevelCount = 1;
  wgpu_texture_desc.sampleCount = 1;
  wgpu_texture_desc.viewFormatCount =
      static_cast<uint32_t>(view_formats.size());
  wgpu_texture_desc.viewFormats = view_formats.data();

  // We need to have internal usages of CopySrc for copies,
  // RenderAttachment for clears, and TextureBinding for copyTextureForBrowser
  // if texture format allows these usages.
  wgpu::DawnTextureInternalUsageDescriptor wgpu_internal_usage_desc;
  wgpu_internal_usage_desc.internalUsage = wgpu_allowed_usage;
  wgpu_texture_desc.nextInChain = &wgpu_internal_usage_desc;

  std::unique_ptr<ExternalImageDXGI> external_image;
  if (absl::holds_alternative<HANDLE>(handle_or_texture)) {
    ExternalImageDescriptorDXGISharedHandle external_image_desc;
    external_image_desc.cTextureDescriptor =
        reinterpret_cast<WGPUTextureDescriptor*>(&wgpu_texture_desc);
    external_image_desc.sharedHandle = absl::get<HANDLE>(handle_or_texture);

    external_image =
        ExternalImageDXGI::Create(device.Get(), &external_image_desc);
  } else {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
        absl::get<Microsoft::WRL::ComPtr<ID3D11Texture2D>>(handle_or_texture);
    ExternalImageDescriptorD3D11Texture external_image_desc;
    external_image_desc.cTextureDescriptor =
        reinterpret_cast<WGPUTextureDescriptor*>(&wgpu_texture_desc);
    external_image_desc.texture = std::move(texture);

    external_image =
        ExternalImageDXGI::Create(device.Get(), &external_image_desc);
  }

  if (!external_image) {
    LOG(ERROR) << "Failed to create external image";
    return nullptr;
  }

  DCHECK(external_image->IsValid());
  return external_image;
}
#endif  // BUILDFLAG(USE_DAWN)

}  // namespace gpu
