// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_factory_d3d.h"

#include <d3d11_1.h>

#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image_backing_d3d.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gl/direct_composition_surface_win.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_bindings.h"

namespace gpu {

namespace {

bool ClearBackBuffer(Microsoft::WRL::ComPtr<IDXGISwapChain1>& swap_chain,
                     Microsoft::WRL::ComPtr<ID3D11Device>& d3d11_device) {
  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture;
  HRESULT hr = swap_chain->GetBuffer(0, IID_PPV_ARGS(&d3d11_texture));
  if (FAILED(hr)) {
    DLOG(ERROR) << "GetBuffer failed with error " << std::hex << hr;
    return false;
  }
  DCHECK(d3d11_texture);

  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> render_target;
  hr = d3d11_device->CreateRenderTargetView(d3d11_texture.Get(), nullptr,
                                            &render_target);
  if (FAILED(hr)) {
    DLOG(ERROR) << "CreateRenderTargetView failed with error " << std::hex
                << hr;
    return false;
  }
  DCHECK(render_target);

  Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_device_context;
  d3d11_device->GetImmediateContext(&d3d11_device_context);
  DCHECK(d3d11_device_context);

  float color_rgba[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  d3d11_device_context->ClearRenderTargetView(render_target.Get(), color_rgba);
  return true;
}

// Only RGBA formats supported by CreateSharedImage.
absl::optional<DXGI_FORMAT> GetSupportedRGBAFormat(
    viz::ResourceFormat viz_resource_format) {
  switch (viz_resource_format) {
    case viz::RGBA_F16:
      return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case viz::BGRA_8888:
      return DXGI_FORMAT_B8G8R8A8_UNORM;
    case viz::RGBA_8888:
      return DXGI_FORMAT_R8G8B8A8_UNORM;
    default:
      NOTREACHED();
      return {};
  }
}

// Formats supported by CreateSharedImage(GMB) and CreateSharedImageVideoPlanes.
DXGI_FORMAT GetDXGIFormat(gfx::BufferFormat buffer_format) {
  switch (buffer_format) {
    case gfx::BufferFormat::RGBA_8888:
      return DXGI_FORMAT_R8G8B8A8_UNORM;
    case gfx::BufferFormat::BGRA_8888:
      return DXGI_FORMAT_B8G8R8A8_UNORM;
    case gfx::BufferFormat::RGBA_F16:
      return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return DXGI_FORMAT_NV12;
    default:
      NOTREACHED();
      return DXGI_FORMAT_UNKNOWN;
  }
}

// Formats supported by CreateSharedImage(GMB) and CreateSharedImageVideoPlanes.
DXGI_FORMAT GetDXGITypelessFormat(gfx::BufferFormat buffer_format) {
  switch (buffer_format) {
    case gfx::BufferFormat::RGBA_8888:
      return DXGI_FORMAT_R8G8B8A8_TYPELESS;
    case gfx::BufferFormat::BGRA_8888:
      return DXGI_FORMAT_B8G8R8A8_TYPELESS;
    case gfx::BufferFormat::RGBA_F16:
      return DXGI_FORMAT_R16G16B16A16_TYPELESS;
    default:
      return DXGI_FORMAT_UNKNOWN;
  }
}

Microsoft::WRL::ComPtr<ID3D11Texture2D> ValidateAndOpenSharedHandle(
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
    const gfx::GpuMemoryBufferHandle& handle,
    gfx::BufferFormat format,
    const gfx::Size& size) {
  if (handle.type != gfx::DXGI_SHARED_HANDLE || !handle.dxgi_handle.IsValid()) {
    DLOG(ERROR) << "Invalid handle with type: " << handle.type;
    return nullptr;
  }

  if (!gpu::IsImageSizeValidForGpuMemoryBufferFormat(size, format)) {
    DLOG(ERROR) << "Invalid image size " << size.ToString() << " for "
                << gfx::BufferFormatToString(format);
    return nullptr;
  }

  Microsoft::WRL::ComPtr<ID3D11Device1> d3d11_device1;
  HRESULT hr = d3d11_device.As(&d3d11_device1);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to query for ID3D11Device1. Error: "
                << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture;
  hr = d3d11_device1->OpenSharedResource1(handle.dxgi_handle.Get(),
                                          IID_PPV_ARGS(&d3d11_texture));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Unable to open shared resource from DXGI handle. Error: "
                << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  D3D11_TEXTURE2D_DESC desc;
  d3d11_texture->GetDesc(&desc);

  // TODO: Add checks for device specific limits.
  if (desc.Width != static_cast<UINT>(size.width()) ||
      desc.Height != static_cast<UINT>(size.height())) {
    DLOG(ERROR) << "Size must match texture being opened";
    return nullptr;
  }

  if ((desc.Format != GetDXGIFormat(format)) &&
      (desc.Format != GetDXGITypelessFormat(format))) {
    DLOG(ERROR) << "Format must match texture being opened";
    return nullptr;
  }

  return d3d11_texture;
}

}  // anonymous namespace

SharedImageBackingFactoryD3D::SharedImageBackingFactoryD3D()
    : d3d11_device_(gl::QueryD3D11DeviceObjectFromANGLE()) {}

SharedImageBackingFactoryD3D::~SharedImageBackingFactoryD3D() = default;

SharedImageBackingFactoryD3D::SwapChainBackings::SwapChainBackings(
    std::unique_ptr<SharedImageBacking> front_buffer,
    std::unique_ptr<SharedImageBacking> back_buffer)
    : front_buffer(std::move(front_buffer)),
      back_buffer(std::move(back_buffer)) {}

SharedImageBackingFactoryD3D::SwapChainBackings::~SwapChainBackings() = default;

SharedImageBackingFactoryD3D::SwapChainBackings::SwapChainBackings(
    SharedImageBackingFactoryD3D::SwapChainBackings&&) = default;

SharedImageBackingFactoryD3D::SwapChainBackings&
SharedImageBackingFactoryD3D::SwapChainBackings::operator=(
    SharedImageBackingFactoryD3D::SwapChainBackings&&) = default;

// static
bool SharedImageBackingFactoryD3D::IsSwapChainSupported() {
  return gl::DirectCompositionSurfaceWin::IsDirectCompositionSupported() &&
         gl::DirectCompositionSurfaceWin::IsSwapChainTearingSupported();
}

SharedImageBackingFactoryD3D::SwapChainBackings
SharedImageBackingFactoryD3D::CreateSwapChain(
    const Mailbox& front_buffer_mailbox,
    const Mailbox& back_buffer_mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage) {
  if (!SharedImageBackingFactoryD3D::IsSwapChainSupported())
    return {nullptr, nullptr};

  DXGI_FORMAT swap_chain_format;
  switch (format) {
    case viz::RGBA_8888:
    case viz::RGBX_8888:
    case viz::BGRA_8888:
      swap_chain_format = DXGI_FORMAT_B8G8R8A8_UNORM;
      break;
    case viz::RGBA_F16:
      swap_chain_format = DXGI_FORMAT_R16G16B16A16_FLOAT;
      break;
    default:
      DLOG(ERROR) << gfx::BufferFormatToString(viz::BufferFormat(format))
                  << " format is not supported by swap chain.";
      return {nullptr, nullptr};
  }

  Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
  d3d11_device_.As(&dxgi_device);
  DCHECK(dxgi_device);
  Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
  dxgi_device->GetAdapter(&dxgi_adapter);
  DCHECK(dxgi_adapter);
  Microsoft::WRL::ComPtr<IDXGIFactory2> dxgi_factory;
  dxgi_adapter->GetParent(IID_PPV_ARGS(&dxgi_factory));
  DCHECK(dxgi_factory);

  DXGI_SWAP_CHAIN_DESC1 desc = {};
  desc.Width = size.width();
  desc.Height = size.height();
  desc.Format = swap_chain_format;
  desc.Stereo = FALSE;
  desc.SampleDesc.Count = 1;
  desc.BufferCount = 2;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
  desc.Scaling = DXGI_SCALING_STRETCH;
  desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
  desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
  desc.AlphaMode = viz::HasAlpha(format) ? DXGI_ALPHA_MODE_PREMULTIPLIED
                                         : DXGI_ALPHA_MODE_IGNORE;

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain;

  HRESULT hr = dxgi_factory->CreateSwapChainForComposition(
      d3d11_device_.Get(), &desc, nullptr, &swap_chain);

  if (FAILED(hr)) {
    DLOG(ERROR) << "CreateSwapChainForComposition failed with error "
                << std::hex << hr;
    return {nullptr, nullptr};
  }

  // Explicitly clear front and back buffers to ensure that there are no
  // uninitialized pixels.
  if (!ClearBackBuffer(swap_chain, d3d11_device_))
    return {nullptr, nullptr};

  DXGI_PRESENT_PARAMETERS params = {};
  params.DirtyRectsCount = 0;
  params.pDirtyRects = nullptr;
  hr = swap_chain->Present1(0 /* interval */, 0 /* flags */, &params);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Present1 failed with error " << std::hex << hr;
    return {nullptr, nullptr};
  }

  if (!ClearBackBuffer(swap_chain, d3d11_device_))
    return {nullptr, nullptr};

  Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer_texture;
  hr = swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer_texture));
  if (FAILED(hr)) {
    DLOG(ERROR) << "GetBuffer failed with error " << std::hex;
    return {nullptr, nullptr};
  }
  auto back_buffer_backing = SharedImageBackingD3D::CreateFromSwapChainBuffer(
      back_buffer_mailbox, format, size, color_space, surface_origin,
      alpha_type, usage, std::move(back_buffer_texture), swap_chain,
      /*buffer_index=*/0);
  if (!back_buffer_backing)
    return {nullptr, nullptr};
  back_buffer_backing->SetCleared();

  Microsoft::WRL::ComPtr<ID3D11Texture2D> front_buffer_texture;
  hr = swap_chain->GetBuffer(1, IID_PPV_ARGS(&front_buffer_texture));
  if (FAILED(hr)) {
    DLOG(ERROR) << "GetBuffer failed with error " << std::hex;
    return {nullptr, nullptr};
  }
  auto front_buffer_backing = SharedImageBackingD3D::CreateFromSwapChainBuffer(
      front_buffer_mailbox, format, size, color_space, surface_origin,
      alpha_type, usage, std::move(front_buffer_texture), swap_chain,
      /*buffer_index=*/1);
  if (!front_buffer_backing)
    return {nullptr, nullptr};
  front_buffer_backing->SetCleared();

  return {std::move(front_buffer_backing), std::move(back_buffer_backing)};
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryD3D::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    bool is_thread_safe) {
  DCHECK(!is_thread_safe);

  // Without D3D11, we cannot do shared images. This will happen if we're
  // running with Vulkan, D3D9, GL or with the non-passthrough command decoder
  // in tests.
  if (!d3d11_device_) {
    return nullptr;
  }

  const absl::optional<DXGI_FORMAT> dxgi_format =
      GetSupportedRGBAFormat(format);
  if (!dxgi_format.has_value()) {
    DLOG(ERROR) << "Unsupported viz format found: " << format;
    return nullptr;
  }

  D3D11_TEXTURE2D_DESC desc;
  desc.Width = size.width();
  desc.Height = size.height();
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = dxgi_format.value();
  desc.SampleDesc.Count = 1;
  desc.SampleDesc.Quality = 0;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
  desc.CPUAccessFlags = 0;
  desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE |
                   D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture;
  HRESULT hr = d3d11_device_->CreateTexture2D(&desc, nullptr, &d3d11_texture);
  if (FAILED(hr)) {
    DLOG(ERROR) << "CreateTexture2D failed with error " << std::hex << hr;
    return nullptr;
  }

  Microsoft::WRL::ComPtr<IDXGIResource1> dxgi_resource;
  hr = d3d11_texture.As(&dxgi_resource);
  if (FAILED(hr)) {
    DLOG(ERROR) << "QueryInterface for IDXGIResource failed with error "
                << std::hex << hr;
    return nullptr;
  }

  HANDLE shared_handle;
  hr = dxgi_resource->CreateSharedHandle(
      nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr,
      &shared_handle);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Unable to create shared handle for DXGIResource "
                << std::hex << hr;
    return nullptr;
  }
  // Put the shared handle into an RAII object as quickly as possible to
  // ensure we do not leak it.
  base::win::ScopedHandle scoped_shared_handle(shared_handle);
  return SharedImageBackingD3D::CreateFromSharedHandle(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(d3d11_texture), std::move(scoped_shared_handle));
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryD3D::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    base::span<const uint8_t> pixel_data) {
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryD3D::CreateSharedImage(
    const Mailbox& mailbox,
    int client_id,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat format,
    gfx::BufferPlane plane,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage) {
  // TODO: Add support for shared memory GMBs.
  if (!GetSupportedRGBAFormat(viz::GetResourceFormat(format))) {
    DLOG(ERROR) << "Unsupported format " << gfx::BufferFormatToString(format);
    return nullptr;
  }
  if (plane != gfx::BufferPlane::DEFAULT) {
    DLOG(ERROR) << "Invalid plane " << gfx::BufferPlaneToString(plane);
    return nullptr;
  }

  auto d3d11_texture =
      ValidateAndOpenSharedHandle(d3d11_device_, handle, format, size);
  if (!d3d11_texture)
    return nullptr;

  auto backing = SharedImageBackingD3D::CreateFromSharedHandle(
      mailbox, viz::GetResourceFormat(format), size, color_space,
      surface_origin, alpha_type, usage, std::move(d3d11_texture),
      std::move(handle.dxgi_handle));
  if (backing)
    backing->SetCleared();
  return backing;
}

std::vector<std::unique_ptr<SharedImageBacking>>
SharedImageBackingFactoryD3D::CreateSharedImageVideoPlanes(
    base::span<const Mailbox> mailboxes,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat format,
    const gfx::Size& size,
    uint32_t usage) {
  // Only supports NV12 for now.
  if (format != gfx::BufferFormat::YUV_420_BIPLANAR) {
    DLOG(ERROR) << "Unsupported format: " << gfx::BufferFormatToString(format);
    return {};
  }

  auto d3d11_texture =
      ValidateAndOpenSharedHandle(d3d11_device_, handle, format, size);
  if (!d3d11_texture)
    return {};

  return SharedImageBackingD3D::CreateFromVideoTexture(
      mailboxes, GetDXGIFormat(format), size, usage, std::move(d3d11_texture),
      /*array_slice=*/0, std::move(handle.dxgi_handle));
}

// Returns true if the specified GpuMemoryBufferType can be imported using
// this factory.
bool SharedImageBackingFactoryD3D::CanImportGpuMemoryBuffer(
    gfx::GpuMemoryBufferType memory_buffer_type) {
  return (memory_buffer_type == gfx::DXGI_SHARED_HANDLE);
}

}  // namespace gpu
