// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/d3d_image_backing_factory.h"

#include <d3d11_1.h>

#include "base/memory/shared_memory_mapping.h"
#include "base/win/scoped_handle.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/dxgi_shared_handle_manager.h"
#include "gpu/command_buffer/service/shared_image/d3d_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_utils.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gl/direct_composition_support.h"
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

// Formats supported by CreateSharedImage() with no GpuMemoryBufferHandle.
DXGI_FORMAT GetDXGIFormatForCreateTexture(viz::SharedImageFormat format) {
  if (format.is_single_plane()) {
    switch (format.resource_format()) {
      case viz::ResourceFormat::RGBA_F16:
        return DXGI_FORMAT_R16G16B16A16_FLOAT;
      case viz::ResourceFormat::BGRA_8888:
        return DXGI_FORMAT_B8G8R8A8_UNORM;
      case viz::ResourceFormat::RGBA_8888:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
      case viz::ResourceFormat::RED_8:
        return DXGI_FORMAT_R8_UNORM;
      case viz::ResourceFormat::RG_88:
        return DXGI_FORMAT_R8G8_UNORM;
      case viz::ResourceFormat::R16_EXT:
        return DXGI_FORMAT_R16_UNORM;
      case viz::ResourceFormat::RG16_EXT:
        return DXGI_FORMAT_R16G16_UNORM;
      default:
        break;
    }
  }

  if (format == viz::MultiPlaneFormat::kYUV_420_BIPLANAR) {
    return DXGI_FORMAT_NV12;
  }

  NOTREACHED();
  return DXGI_FORMAT_UNKNOWN;
}

// Formats supported by CreateSharedImage(GMB).
DXGI_FORMAT GetDXGIFormatForGMB(viz::SharedImageFormat format) {
  if (format.is_single_plane()) {
    switch (format.resource_format()) {
      case viz::ResourceFormat::RGBA_8888:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
      case viz::ResourceFormat::BGRA_8888:
        return DXGI_FORMAT_B8G8R8A8_UNORM;
      case viz::ResourceFormat::RGBA_F16:
        return DXGI_FORMAT_R16G16B16A16_FLOAT;
      case viz::ResourceFormat::YUV_420_BIPLANAR:
        return DXGI_FORMAT_NV12;
      default:
        return DXGI_FORMAT_UNKNOWN;
    }
  }
  if (format == viz::MultiPlaneFormat::kYUV_420_BIPLANAR) {
    return DXGI_FORMAT_NV12;
  }
  return DXGI_FORMAT_UNKNOWN;
}

// Typeless formats supported by CreateSharedImage(GMB) for XR.
DXGI_FORMAT GetDXGITypelessFormat(viz::SharedImageFormat format) {
  if (format.is_single_plane()) {
    switch (format.resource_format()) {
      case viz::ResourceFormat::RGBA_8888:
        return DXGI_FORMAT_R8G8B8A8_TYPELESS;
      case viz::ResourceFormat::BGRA_8888:
        return DXGI_FORMAT_B8G8R8A8_TYPELESS;
      case viz::ResourceFormat::RGBA_F16:
        return DXGI_FORMAT_R16G16B16A16_TYPELESS;
      default:
        return DXGI_FORMAT_UNKNOWN;
    }
  }
  return DXGI_FORMAT_UNKNOWN;
}

scoped_refptr<DXGISharedHandleState> ValidateAndOpenSharedHandle(
    DXGISharedHandleManager* dxgi_shared_handle_manager,
    gfx::GpuMemoryBufferHandle handle,
    viz::SharedImageFormat format,
    const gfx::Size& size) {
  if (handle.type != gfx::DXGI_SHARED_HANDLE || !handle.dxgi_handle.IsValid()) {
    LOG(ERROR) << "Invalid handle with type: " << handle.type;
    return nullptr;
  }

  if (!handle.dxgi_token.has_value()) {
    LOG(ERROR) << "Missing token for DXGI handle";
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

  if ((desc.Format != GetDXGIFormatForGMB(format)) &&
      (desc.Format != GetDXGITypelessFormat(format))) {
    LOG(ERROR) << "Format must match texture being opened";
    return nullptr;
  }

  return dxgi_shared_handle_state;
}
constexpr uint32_t kSupportedUsage =
    SHARED_IMAGE_USAGE_GLES2 | SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT |
    SHARED_IMAGE_USAGE_DISPLAY_WRITE | SHARED_IMAGE_USAGE_DISPLAY_READ |
    SHARED_IMAGE_USAGE_RASTER | SHARED_IMAGE_USAGE_OOP_RASTERIZATION |
    SHARED_IMAGE_USAGE_SCANOUT | SHARED_IMAGE_USAGE_WEBGPU |
    SHARED_IMAGE_USAGE_VIDEO_DECODE |
    SHARED_IMAGE_USAGE_WEBGPU_SWAP_CHAIN_TEXTURE |
    SHARED_IMAGE_USAGE_HIGH_PERFORMANCE_GPU | SHARED_IMAGE_USAGE_CPU_UPLOAD |
    SHARED_IMAGE_USAGE_WEBGPU_STORAGE_TEXTURE;

}  // anonymous namespace

D3DImageBackingFactory::D3DImageBackingFactory(
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
    scoped_refptr<DXGISharedHandleManager> dxgi_shared_handle_manager)
    : SharedImageBackingFactory(kSupportedUsage),
      d3d11_device_(std::move(d3d11_device)),
      dxgi_shared_handle_manager_(std::move(dxgi_shared_handle_manager)) {
  DCHECK(d3d11_device_);
}

D3DImageBackingFactory::~D3DImageBackingFactory() = default;

D3DImageBackingFactory::SwapChainBackings::SwapChainBackings(
    std::unique_ptr<SharedImageBacking> front_buffer,
    std::unique_ptr<SharedImageBacking> back_buffer)
    : front_buffer(std::move(front_buffer)),
      back_buffer(std::move(back_buffer)) {}

D3DImageBackingFactory::SwapChainBackings::~SwapChainBackings() = default;

D3DImageBackingFactory::SwapChainBackings::SwapChainBackings(
    D3DImageBackingFactory::SwapChainBackings&&) = default;

D3DImageBackingFactory::SwapChainBackings&
D3DImageBackingFactory::SwapChainBackings::operator=(
    D3DImageBackingFactory::SwapChainBackings&&) = default;

// static
bool D3DImageBackingFactory::IsD3DSharedImageSupported(
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
bool D3DImageBackingFactory::IsSwapChainSupported() {
  return gl::DirectCompositionSupported() &&
         gl::DXGISwapChainTearingSupported();
}

D3DImageBackingFactory::SwapChainBackings
D3DImageBackingFactory::CreateSwapChain(const Mailbox& front_buffer_mailbox,
                                        const Mailbox& back_buffer_mailbox,
                                        viz::SharedImageFormat format,
                                        const gfx::Size& size,
                                        const gfx::ColorSpace& color_space,
                                        GrSurfaceOrigin surface_origin,
                                        SkAlphaType alpha_type,
                                        uint32_t usage) {
  if (!D3DImageBackingFactory::IsSwapChainSupported())
    return {nullptr, nullptr};

  DXGI_FORMAT swap_chain_format;
  if ((format == viz::SinglePlaneFormat::kRGBA_8888) ||
      (format == viz::SinglePlaneFormat::kRGBX_8888) ||
      (format == viz::SinglePlaneFormat::kBGRA_8888)) {
    swap_chain_format = DXGI_FORMAT_B8G8R8A8_UNORM;
  } else if (format == viz::SinglePlaneFormat::kRGBA_F16) {
    swap_chain_format = DXGI_FORMAT_R16G16B16A16_FLOAT;
  } else {
    LOG(ERROR) << format.ToString()
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
  desc.Flags = 0;
  if (gl::DXGISwapChainTearingSupported()) {
    desc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
  }
  if (gl::DXGIWaitableSwapChainEnabled()) {
    desc.Flags |= DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
  }
  desc.AlphaMode = format.HasAlpha() ? DXGI_ALPHA_MODE_PREMULTIPLIED
                                     : DXGI_ALPHA_MODE_IGNORE;

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain;
  HRESULT hr = dxgi_factory->CreateSwapChainForComposition(
      d3d11_device_.Get(), &desc, nullptr, &swap_chain);
  if (FAILED(hr)) {
    LOG(ERROR) << "CreateSwapChainForComposition failed with error " << std::hex
               << hr;
    return {nullptr, nullptr};
  }

  if (gl::DXGIWaitableSwapChainEnabled()) {
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swap_chain3;
    if (SUCCEEDED(swap_chain.As(&swap_chain3))) {
      hr = swap_chain3->SetMaximumFrameLatency(
          gl::GetDXGIWaitableSwapChainMaxQueuedFrames());
      DCHECK(SUCCEEDED(hr)) << "SetMaximumFrameLatency failed with error "
                            << logging::SystemErrorCodeToString(hr);
    }
  }

  // Explicitly clear front and back buffers to ensure that there are no
  // uninitialized pixels.
  if (!ClearBackBuffer(swap_chain, d3d11_device_))
    return {nullptr, nullptr};

  DXGI_PRESENT_PARAMETERS params = {};
  params.DirtyRectsCount = 0;
  params.pDirtyRects = nullptr;
  hr = swap_chain->Present1(/*interval=*/0, /*flags=*/0, &params);
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
  auto back_buffer_backing = D3DImageBacking::CreateFromSwapChainBuffer(
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
  auto front_buffer_backing = D3DImageBacking::CreateFromSwapChainBuffer(
      front_buffer_mailbox, format, size, color_space, surface_origin,
      alpha_type, usage, std::move(front_buffer_texture), swap_chain,
      /*is_back_buffer=*/false);
  if (!front_buffer_backing)
    return {nullptr, nullptr};
  front_buffer_backing->SetCleared();

  return {std::move(front_buffer_backing), std::move(back_buffer_backing)};
}

std::unique_ptr<SharedImageBacking> D3DImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
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

  DXGI_FORMAT dxgi_format = GetDXGIFormatForCreateTexture(format);
  DCHECK_NE(dxgi_format, DXGI_FORMAT_UNKNOWN);

  // SHARED_IMAGE_USAGE_CPU_UPLOAD is set for shared memory GMBs.
  const bool is_shm_gmb = usage & SHARED_IMAGE_USAGE_CPU_UPLOAD;

  // TODO(https://anglebug.com/7998): Binding a GL texture that represents one
  // plane of a multi-planar D3D to the GL framebuffer doesn't work correctly.
  // This sets the texture target to GL_TEXTURE_EXTERNAL_OES to prevent that
  // until the issue is fixed.
  const GLenum texture_target =
      format.is_single_plane() ? GL_TEXTURE_2D : GL_TEXTURE_EXTERNAL_OES;

  D3D11_TEXTURE2D_DESC desc;
  desc.Width = size.width();
  desc.Height = size.height();
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = dxgi_format;
  desc.SampleDesc.Count = 1;
  desc.SampleDesc.Quality = 0;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
  desc.CPUAccessFlags = 0;
  desc.MiscFlags = 0;

  if ((usage & gpu::SHARED_IMAGE_USAGE_WEBGPU_STORAGE_TEXTURE) &&
      format.is_single_plane()) {
    DCHECK(usage & gpu::SHARED_IMAGE_USAGE_WEBGPU);
    switch (format.resource_format()) {
      // WebGPU can use RGBA_8888 and RGBA_16 for STORAGE_BINDING.
      case viz::ResourceFormat::RGBA_8888:
      case viz::ResourceFormat::RGBA_F16:
        desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
        break;

      // WebGPU can use BGRA_8888 for STORAGE_BINDING when BGRA_8888 is
      // supported as UAV.
      case viz::ResourceFormat::BGRA_8888: {
        if (SupportsBGRA8UnormStorage()) {
          desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
        } else {
          LOG(ERROR)
              << "D3D11_BIND_UNORDERED_ACCESS is not supported on BGRA_8888";
          return nullptr;
        }
        break;
      }
      default:
        break;
    }
  }
  if (is_shm_gmb) {
    // D3D doesn't support mappable+default YUV textures.
    if (format.is_single_plane() && UseMapOnDefaultTextures()) {
      desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
    }
  } else {
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE |
                     (D3DSharedFence::IsSupported(d3d11_device_.Get())
                          ? D3D11_RESOURCE_MISC_SHARED
                          : D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX);
  }

  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture;
  HRESULT hr = d3d11_device_->CreateTexture2D(&desc, nullptr, &d3d11_texture);
  if (FAILED(hr)) {
    LOG(ERROR) << "CreateTexture2D failed with error " << std::hex << hr;
    return nullptr;
  }

  const std::string debug_label =
      "SharedImage_Texture2D" + CreateLabelForSharedImageUsage(usage);
  d3d11_texture->SetPrivateData(WKPDID_D3DDebugObjectName, debug_label.length(),
                                debug_label.c_str());

  if (is_shm_gmb) {
    // Early return before creating DXGI keyed mutex.
    return D3DImageBacking::Create(
        mailbox, format, size, color_space, surface_origin, alpha_type, usage,
        std::move(d3d11_texture), nullptr, texture_target);
  }

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

  return D3DImageBacking::Create(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(d3d11_texture), std::move(dxgi_shared_handle_state),
      texture_target);
}

std::unique_ptr<SharedImageBacking> D3DImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    base::span<const uint8_t> pixel_data) {
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<SharedImageBacking> D3DImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    gfx::GpuMemoryBufferHandle handle) {
  return CreateSharedImageGMBs(mailbox, std::move(handle), format,
                               gfx::BufferPlane::DEFAULT, size, color_space,
                               surface_origin, alpha_type, usage);
}

std::unique_ptr<SharedImageBacking> D3DImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat buffer_format,
    gfx::BufferPlane plane,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage) {
  if (!IsPlaneValidForGpuMemoryBufferFormat(plane, buffer_format)) {
    LOG(ERROR) << "Invalid plane " << gfx::BufferPlaneToString(plane)
               << " for format " << gfx::BufferFormatToString(buffer_format);
    return nullptr;
  }

  auto format = viz::SharedImageFormat::SinglePlane(
      viz::GetResourceFormat(buffer_format));
  return CreateSharedImageGMBs(mailbox, std::move(handle), format, plane, size,
                               color_space, surface_origin, alpha_type, usage);
}

bool D3DImageBackingFactory::UseMapOnDefaultTextures() {
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

bool D3DImageBackingFactory::SupportsBGRA8UnormStorage() {
  if (!supports_bgra8unorm_storage_.has_value()) {
    D3D11_FEATURE_DATA_FORMAT_SUPPORT bgra8UnormSupport = {};
    bgra8UnormSupport.InFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
    HRESULT hr = d3d11_device_->CheckFeatureSupport(
        D3D11_FEATURE_FORMAT_SUPPORT, &bgra8UnormSupport,
        sizeof(D3D11_FEATURE_DATA_FORMAT_SUPPORT));
    if (SUCCEEDED(hr)) {
      supports_bgra8unorm_storage_.emplace(
          bgra8UnormSupport.OutFormatSupport &
          D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW);
    } else {
      VLOG(1) << "Failed to retrieve D3D11_FEATURE_FORMAT_SUPPORT. hr = "
              << std::hex << hr;
      supports_bgra8unorm_storage_.emplace(false);
    }
  }
  return supports_bgra8unorm_storage_.value();
}

bool D3DImageBackingFactory::IsSupported(uint32_t usage,
                                         viz::SharedImageFormat format,
                                         const gfx::Size& size,
                                         bool thread_safe,
                                         gfx::GpuMemoryBufferType gmb_type,
                                         GrContextType gr_context_type,
                                         base::span<const uint8_t> pixel_data) {
  if (!pixel_data.empty()) {
    return false;
  }

  if (gmb_type == gfx::EMPTY_BUFFER) {
    if (GetDXGIFormatForCreateTexture(format) == DXGI_FORMAT_UNKNOWN) {
      return false;
    }
  } else if (gmb_type == gfx::DXGI_SHARED_HANDLE) {
    if (GetDXGIFormatForGMB(format) == DXGI_FORMAT_UNKNOWN) {
      return false;
    }
  } else {
    return false;
  }

  return true;
}

std::unique_ptr<SharedImageBacking>
D3DImageBackingFactory::CreateSharedImageGMBs(
    const Mailbox& mailbox,
    gfx::GpuMemoryBufferHandle handle,
    viz::SharedImageFormat format,
    gfx::BufferPlane plane,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage) {
  const gfx::BufferFormat buffer_format = gpu::ToBufferFormat(format);
  if (!gpu::IsImageSizeValidForGpuMemoryBufferFormat(size, buffer_format)) {
    LOG(ERROR) << "Invalid image size " << size.ToString() << " for "
               << gfx::BufferFormatToString(buffer_format);
    return nullptr;
  }

  DCHECK_EQ(handle.type, gfx::DXGI_SHARED_HANDLE);
  DCHECK(plane == gfx::BufferPlane::DEFAULT || plane == gfx::BufferPlane::Y ||
         plane == gfx::BufferPlane::UV);

  scoped_refptr<DXGISharedHandleState> dxgi_shared_handle_state =
      ValidateAndOpenSharedHandle(dxgi_shared_handle_manager_.get(),
                                  std::move(handle), format, size);
  if (!dxgi_shared_handle_state) {
    return nullptr;
  }

  auto d3d11_texture = dxgi_shared_handle_state->d3d11_texture();

  const GLenum texture_target = GL_TEXTURE_2D;
  std::unique_ptr<D3DImageBacking> backing;
  if (format.IsLegacyMultiplanar()) {
    // Get format and size per plane. For multiplanar formats, `plane_format` is
    // R/RG based on channels in plane.
    const gfx::Size plane_size = GetPlaneSize(plane, size);
    const viz::SharedImageFormat plane_format =
        viz::SharedImageFormat::SinglePlane(
            viz::GetResourceFormat(GetPlaneBufferFormat(plane, buffer_format)));
    const size_t plane_index = plane == gfx::BufferPlane::UV ? 1 : 0;
    backing = D3DImageBacking::Create(
        mailbox, plane_format, plane_size, color_space, surface_origin,
        alpha_type, usage, std::move(d3d11_texture),
        std::move(dxgi_shared_handle_state), texture_target, /*array_slice=*/0u,
        /*plane_index=*/plane_index);
  } else {
    backing = D3DImageBacking::Create(
        mailbox, format, size, color_space, surface_origin, alpha_type, usage,
        std::move(d3d11_texture), std::move(dxgi_shared_handle_state),
        texture_target, /*array_slice=*/0u,
        /*plane_index=*/0);
  }

  if (backing) {
    backing->SetCleared();
  }
  return backing;
}

}  // namespace gpu
