// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_factory_d3d.h"

#include <d3d11_1.h>

#include "base/memory/shared_memory_mapping.h"
#include "base/win/scoped_handle.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/dxgi_shared_handle_manager.h"
#include "gpu/command_buffer/service/shared_image_backing_d3d.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gl/direct_composition_surface_win.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_utils.h"

namespace gpu {

namespace {

bool ClearBackBuffer(Microsoft::WRL::ComPtr<IDXGISwapChain1>& swap_chain,
                     Microsoft::WRL::ComPtr<ID3D11Device>& d3d11_device) {
  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture;
  HRESULT hr = swap_chain->GetBuffer(0, IID_PPV_ARGS(&d3d11_texture));
  if (FAILED(hr)) {
    LOG(ERROR) << "GetBuffer failed with error " << std::hex << hr;
    return false;
  }
  DCHECK(d3d11_texture);

  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> render_target;
  hr = d3d11_device->CreateRenderTargetView(d3d11_texture.Get(), nullptr,
                                            &render_target);
  if (FAILED(hr)) {
    LOG(ERROR) << "CreateRenderTargetView failed with error " << std::hex << hr;
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
    case viz::RED_8:
      return DXGI_FORMAT_R8_UNORM;
    case viz::RG_88:
      return DXGI_FORMAT_R8G8_UNORM;
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

scoped_refptr<DXGISharedHandleState> ValidateAndOpenSharedHandle(
    DXGISharedHandleManager* dxgi_shared_handle_manager,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat format,
    const gfx::Size& size) {
  if (handle.type != gfx::DXGI_SHARED_HANDLE || !handle.dxgi_handle.IsValid()) {
    LOG(ERROR) << "Invalid handle with type: " << handle.type;
    return nullptr;
  }

  if (!handle.dxgi_token.has_value()) {
    LOG(ERROR) << "Missing token for DXGI handle";
    return nullptr;
  }

  if (!gpu::IsImageSizeValidForGpuMemoryBufferFormat(size, format)) {
    LOG(ERROR) << "Invalid image size " << size.ToString() << " for "
               << gfx::BufferFormatToString(format);
    return nullptr;
  }

  scoped_refptr<DXGISharedHandleState> dxgi_shared_handle_state =
      dxgi_shared_handle_manager->GetOrCreateSharedHandleState(
          std::move(handle.dxgi_token.value()), std::move(handle.dxgi_handle));
  if (!dxgi_shared_handle_state) {
    LOG(ERROR) << "Failed to open DXGI shared handle";
    return nullptr;
  }

  D3D11_TEXTURE2D_DESC desc = {};
  dxgi_shared_handle_state->d3d11_texture()->GetDesc(&desc);

  // TODO: Add checks for device specific limits.
  if (desc.Width != static_cast<UINT>(size.width()) ||
      desc.Height != static_cast<UINT>(size.height())) {
    LOG(ERROR) << "Size must match texture being opened";
    return nullptr;
  }

  if ((desc.Format != GetDXGIFormat(format)) &&
      (desc.Format != GetDXGITypelessFormat(format))) {
    LOG(ERROR) << "Format must match texture being opened";
    return nullptr;
  }

  return dxgi_shared_handle_state;
}

}  // anonymous namespace

SharedImageBackingFactoryD3D::SharedImageBackingFactoryD3D(
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
    scoped_refptr<DXGISharedHandleManager> dxgi_shared_handle_manager)
    : d3d11_device_(std::move(d3d11_device)),
      dxgi_shared_handle_manager_(std::move(dxgi_shared_handle_manager)) {
  DCHECK(d3d11_device_);
}

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
bool SharedImageBackingFactoryD3D::IsD3DSharedImageSupported(
    const GpuPreferences& gpu_preferences) {
  // Only supported for passthrough command decoder and Skia-GL.
  const bool using_passthrough = gpu_preferences.use_passthrough_cmd_decoder &&
                                 gl::PassthroughCommandDecoderSupported();
  const bool is_skia_gl = gpu_preferences.gr_context_type == GrContextType::kGL;
  // D3D11 device will be null if ANGLE is using the D3D9 backend.
  const bool using_d3d11 = gl::QueryD3D11DeviceObjectFromANGLE() != nullptr;
  return using_passthrough && is_skia_gl && using_d3d11;
}

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
      LOG(ERROR) << gfx::BufferFormatToString(viz::BufferFormat(format))
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
    LOG(ERROR) << "CreateSwapChainForComposition failed with error " << std::hex
               << hr;
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
    LOG(ERROR) << "Present1 failed with error " << std::hex << hr;
    return {nullptr, nullptr};
  }

  if (!ClearBackBuffer(swap_chain, d3d11_device_))
    return {nullptr, nullptr};

  Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer_texture;
  hr = swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer_texture));
  if (FAILED(hr)) {
    LOG(ERROR) << "GetBuffer failed with error " << std::hex;
    return {nullptr, nullptr};
  }
  auto back_buffer_backing = SharedImageBackingD3D::CreateFromSwapChainBuffer(
      back_buffer_mailbox, format, size, color_space, surface_origin,
      alpha_type, usage, std::move(back_buffer_texture), swap_chain,
      /*is_back_buffer=*/true);
  if (!back_buffer_backing)
    return {nullptr, nullptr};
  back_buffer_backing->SetCleared();

  Microsoft::WRL::ComPtr<ID3D11Texture2D> front_buffer_texture;
  hr = swap_chain->GetBuffer(1, IID_PPV_ARGS(&front_buffer_texture));
  if (FAILED(hr)) {
    LOG(ERROR) << "GetBuffer failed with error " << std::hex;
    return {nullptr, nullptr};
  }
  auto front_buffer_backing = SharedImageBackingD3D::CreateFromSwapChainBuffer(
      front_buffer_mailbox, format, size, color_space, surface_origin,
      alpha_type, usage, std::move(front_buffer_texture), swap_chain,
      /*is_back_buffer=*/false);
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
    LOG(ERROR) << "Unsupported viz format found: " << format;
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
  // WebGPU can use RGBA_8888 and RGBA_16 for STORAGE_BINDING.
  if ((usage & gpu::SHARED_IMAGE_USAGE_WEBGPU) &&
      (format == viz::RGBA_8888 || format == viz::RGBA_F16)) {
    desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
  }
  desc.CPUAccessFlags = 0;
  desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE |
                   D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture;
  HRESULT hr = d3d11_device_->CreateTexture2D(&desc, nullptr, &d3d11_texture);
  if (FAILED(hr)) {
    LOG(ERROR) << "CreateTexture2D failed with error " << std::hex << hr;
    return nullptr;
  }

  const std::string debug_label =
      "SharedImage_Texture2D" + CreateLabelForSharedImageUsage(usage);
  d3d11_device_->SetPrivateData(WKPDID_D3DDebugObjectName, debug_label.length(),
                                debug_label.c_str());

  Microsoft::WRL::ComPtr<IDXGIResource1> dxgi_resource;
  hr = d3d11_texture.As(&dxgi_resource);
  if (FAILED(hr)) {
    LOG(ERROR) << "QueryInterface for IDXGIResource failed with error "
               << std::hex << hr;
    return nullptr;
  }

  HANDLE shared_handle;
  hr = dxgi_resource->CreateSharedHandle(
      nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr,
      &shared_handle);
  if (FAILED(hr)) {
    LOG(ERROR) << "Unable to create shared handle for DXGIResource " << std::hex
               << hr;
    return nullptr;
  }

  scoped_refptr<DXGISharedHandleState> dxgi_shared_handle_state =
      dxgi_shared_handle_manager_->CreateAnonymousSharedHandleState(
          base::win::ScopedHandle(shared_handle), d3d11_texture);

  return SharedImageBackingD3D::CreateFromDXGISharedHandle(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(d3d11_texture), std::move(dxgi_shared_handle_state));
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
  if (handle.type == gfx::DXGI_SHARED_HANDLE) {
    if (!GetSupportedRGBAFormat(viz::GetResourceFormat(format))) {
      LOG(ERROR) << "Unsupported format " << gfx::BufferFormatToString(format);
      return nullptr;
    }

    if (plane != gfx::BufferPlane::DEFAULT) {
      LOG(ERROR) << "Invalid plane " << gfx::BufferPlaneToString(plane);
      return nullptr;
    }

    scoped_refptr<DXGISharedHandleState> dxgi_shared_handle_state =
        ValidateAndOpenSharedHandle(dxgi_shared_handle_manager_.get(),
                                    std::move(handle), format, size);
    if (!dxgi_shared_handle_state)
      return nullptr;

    auto d3d11_texture = dxgi_shared_handle_state->d3d11_texture();

    auto backing = SharedImageBackingD3D::CreateFromDXGISharedHandle(
        mailbox, viz::GetResourceFormat(format), size, color_space,
        surface_origin, alpha_type, usage, std::move(d3d11_texture),
        std::move(dxgi_shared_handle_state));
    if (backing)
      backing->SetCleared();
    return backing;
  }

  DCHECK_EQ(handle.type, gfx::SHARED_MEMORY_BUFFER);
  switch (plane) {
    case gfx::BufferPlane::DEFAULT:
    case gfx::BufferPlane::Y:
    case gfx::BufferPlane::UV:
      break;
    default:
      LOG(ERROR) << "Invalid plane " << gfx::BufferPlaneToString(plane);
      return nullptr;
  }

  const gfx::Size plane_size = GetPlaneSize(plane, size);
  const viz::ResourceFormat plane_format =
      viz::GetResourceFormat(GetPlaneBufferFormat(plane, format));

  absl::optional<DXGI_FORMAT> dxgi_format =
      GetSupportedRGBAFormat(plane_format);
  if (!dxgi_format.has_value()) {
    LOG(ERROR) << "Invalid format " << gfx::BufferFormatToString(format);
    return nullptr;
  }

  D3D11_TEXTURE2D_DESC desc;
  desc.Width = plane_size.width();
  desc.Height = plane_size.height();
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = dxgi_format.value();
  desc.SampleDesc.Count = 1;
  desc.SampleDesc.Quality = 0;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
  desc.CPUAccessFlags = UseMapOnDefaultTextures()
                            ? (D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE)
                            : 0;
  desc.MiscFlags = 0;

  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture;
  HRESULT hr = d3d11_device_->CreateTexture2D(&desc, nullptr, &d3d11_texture);
  if (FAILED(hr)) {
    LOG(ERROR) << "CreateTexture2D failed. hr = " << std::hex << hr;
    return nullptr;
  }

  // Adjust offset to include plane offset.
  const size_t plane_index = plane == gfx::BufferPlane::UV ? 1 : 0;
  handle.offset += gfx::BufferOffsetForBufferFormat(size, format, plane_index);

  auto backing = SharedImageBackingD3D::CreateFromSharedMemoryHandle(
      mailbox, plane_format, plane_size, color_space, surface_origin,
      alpha_type, usage, std::move(d3d11_texture), std::move(handle));
  if (backing) {
    // This marks the needs_upload_to_gpu_ flag to defer uploading the GMB which
    // is unnecessary for GPU-write CPU-read scenarios e.g. two copy canvas
    // capture, but is needed for CPU-write GPU-read cases e.g. software video
    // decoder. In the GPU-write CPU-read scenario, previous GMB/CPU data is
    // discarded after calling CopyToGpuMemoryBuffer().
    backing->Update(/*in_fence=*/nullptr);
    backing->SetCleared();
  }
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
    LOG(ERROR) << "Unsupported format: " << gfx::BufferFormatToString(format);
    return {};
  }

  scoped_refptr<DXGISharedHandleState> dxgi_shared_handle_state =
      ValidateAndOpenSharedHandle(dxgi_shared_handle_manager_.get(),
                                  std::move(handle), format, size);
  if (!dxgi_shared_handle_state)
    return {};

  auto d3d11_texture = dxgi_shared_handle_state->d3d11_texture();

  return SharedImageBackingD3D::CreateFromVideoTexture(
      mailboxes, GetDXGIFormat(format), size, usage, std::move(d3d11_texture),
      /*array_slice=*/0, std::move(dxgi_shared_handle_state));
}

bool SharedImageBackingFactoryD3D::UseMapOnDefaultTextures() {
  if (!map_on_default_textures_.has_value()) {
    D3D11_FEATURE_DATA_D3D11_OPTIONS2 features;
    HRESULT hr = d3d11_device_->CheckFeatureSupport(
        D3D11_FEATURE_D3D11_OPTIONS2, &features,
        sizeof(D3D11_FEATURE_DATA_D3D11_OPTIONS2));
    if (SUCCEEDED(hr)) {
      map_on_default_textures_.emplace(features.MapOnDefaultTextures &&
                                       features.UnifiedMemoryArchitecture);
    } else {
      VLOG(1) << "Failed to retrieve D3D11_FEATURE_D3D11_OPTIONS2. hr = "
              << std::hex << hr;
      map_on_default_textures_.emplace(false);
    }
    VLOG(1) << "UseMapOnDefaultTextures = " << map_on_default_textures_.value();
  }
  return map_on_default_textures_.value();
}

// Returns true if the specified GpuMemoryBufferType can be imported using
// this factory.
bool SharedImageBackingFactoryD3D::CanImportGpuMemoryBuffer(
    gfx::GpuMemoryBufferType gmb_type,
    viz::ResourceFormat format) {
  return gmb_type == gfx::DXGI_SHARED_HANDLE ||
         // Only allow single NV12 shared memory GMBs for now. This excludes
         // dual shared memory GMBs used by software video decoder.
         (gmb_type == gfx::SHARED_MEMORY_BUFFER &&
          format == viz::YUV_420_BIPLANAR);
}

bool SharedImageBackingFactoryD3D::IsSupported(
    uint32_t usage,
    viz::ResourceFormat format,
    bool thread_safe,
    gfx::GpuMemoryBufferType gmb_type,
    GrContextType gr_context_type,
    bool* allow_legacy_mailbox,
    bool is_pixel_used) {
  if (is_pixel_used) {
    return false;
  }
  if (gmb_type != gfx::EMPTY_BUFFER &&
      !CanImportGpuMemoryBuffer(gmb_type, format)) {
    return false;
  }
  // TODO(crbug.com/969114): Not all shared image factory implementations
  // support concurrent read/write usage.
  if (usage & SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE) {
    return false;
  }

  *allow_legacy_mailbox = false;
  return true;
}

}  // namespace gpu
