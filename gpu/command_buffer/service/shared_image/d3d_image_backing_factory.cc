// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/d3d_image_backing_factory.h"

#include <d3d11_1.h>

#include "base/memory/shared_memory_mapping.h"
#include "base/win/scoped_handle.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/dxgi_shared_handle_manager.h"
#include "gpu/command_buffer/service/shared_image/d3d_image_backing.h"
#include "gpu/command_buffer/service/shared_image/d3d_image_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gl/direct_composition_support.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_utils.h"

namespace gpu {

namespace {

// Formats supported by CreateSharedImage() with no GpuMemoryBufferHandle.
DXGI_FORMAT GetDXGIFormatForCreateTexture(viz::SharedImageFormat format) {
  if (format == viz::SinglePlaneFormat::kRGBA_F16) {
    return DXGI_FORMAT_R16G16B16A16_FLOAT;
  } else if (format == viz::SinglePlaneFormat::kBGRA_8888) {
    return DXGI_FORMAT_B8G8R8A8_UNORM;
  } else if (format == viz::SinglePlaneFormat::kRGBA_8888) {
    return DXGI_FORMAT_R8G8B8A8_UNORM;
  } else if (format == viz::SinglePlaneFormat::kBGRX_8888) {
    return DXGI_FORMAT_B8G8R8X8_UNORM;
  } else if (format == viz::SinglePlaneFormat::kRGBX_8888) {
    return DXGI_FORMAT_R8G8B8A8_UNORM;
  } else if (format == viz::SinglePlaneFormat::kR_8) {
    return DXGI_FORMAT_R8_UNORM;
  } else if (format == viz::SinglePlaneFormat::kRG_88) {
    return DXGI_FORMAT_R8G8_UNORM;
  } else if (format == viz::SinglePlaneFormat::kR_16) {
    return DXGI_FORMAT_R16_UNORM;
  } else if (format == viz::SinglePlaneFormat::kRG_1616) {
    return DXGI_FORMAT_R16G16_UNORM;
  } else if (format == viz::MultiPlaneFormat::kNV12) {
    return DXGI_FORMAT_NV12;
  }

  NOTREACHED() << "Unsupported format: " << format.ToString();
  return DXGI_FORMAT_UNKNOWN;
}

// Formats supported by CreateSharedImage(GMB).
DXGI_FORMAT GetDXGIFormatForGMB(viz::SharedImageFormat format) {
  if (format == viz::SinglePlaneFormat::kRGBA_8888) {
    return DXGI_FORMAT_R8G8B8A8_UNORM;
  } else if (format == viz::SinglePlaneFormat::kBGRA_8888) {
    return DXGI_FORMAT_B8G8R8A8_UNORM;
  } else if (format == viz::SinglePlaneFormat::kRGBA_F16) {
    return DXGI_FORMAT_R16G16B16A16_FLOAT;
  } else if (format == viz::MultiPlaneFormat::kNV12 ||
             format == viz::LegacyMultiPlaneFormat::kNV12) {
    return DXGI_FORMAT_NV12;
  }

  return DXGI_FORMAT_UNKNOWN;
}

// Typeless formats supported by CreateSharedImage(GMB) for XR.
DXGI_FORMAT GetDXGITypelessFormat(viz::SharedImageFormat format) {
  if (format == viz::SinglePlaneFormat::kRGBA_8888) {
    return DXGI_FORMAT_R8G8B8A8_TYPELESS;
  } else if (format == viz::SinglePlaneFormat::kBGRA_8888) {
    return DXGI_FORMAT_B8G8R8A8_TYPELESS;
  } else if (format == viz::SinglePlaneFormat::kRGBA_F16) {
    return DXGI_FORMAT_R16G16B16A16_TYPELESS;
  }

  return DXGI_FORMAT_UNKNOWN;
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
  // Only supported for passthrough command decoder.
  if (!gpu_preferences.use_passthrough_cmd_decoder ||
      !gl::PassthroughCommandDecoderSupported()) {
    return false;
  }

  // D3D11 device will be null if ANGLE is using the D3D9 backend.
  if (!gl::QueryD3D11DeviceObjectFromANGLE()) {
    return false;
  }

  // Only supported for Skia GL or Skia GraphiteDawn
  if (gpu_preferences.gr_context_type != GrContextType::kGL &&
      gpu_preferences.gr_context_type != GrContextType::kGraphiteDawn) {
    return false;
  }

  return true;
}

// static
bool D3DImageBackingFactory::IsSwapChainSupported() {
  return gl::DirectCompositionSupported() &&
         gl::DXGISwapChainTearingSupported();
}

// static
bool D3DImageBackingFactory::ClearBackBufferToColor(IDXGISwapChain1* swap_chain,
                                                    const SkColor4f& color) {
  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture;
  HRESULT hr = swap_chain->GetBuffer(0, IID_PPV_ARGS(&d3d11_texture));
  if (FAILED(hr)) {
    LOG(ERROR) << "GetBuffer failed: " << logging::SystemErrorCodeToString(hr);
    return false;
  }
  DCHECK(d3d11_texture);

  return ClearD3D11TextureToColor(d3d11_texture, color);
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
  if (!ClearBackBufferToColor(swap_chain.Get(), SkColors::kBlack)) {
    return {nullptr, nullptr};
  }

  DXGI_PRESENT_PARAMETERS params = {};
  params.DirtyRectsCount = 0;
  params.pDirtyRects = nullptr;
  hr = swap_chain->Present1(/*interval=*/0, /*flags=*/0, &params);
  if (FAILED(hr)) {
    LOG(ERROR) << "Present1 failed with error " << std::hex << hr;
    return {nullptr, nullptr};
  }

  if (!ClearBackBufferToColor(swap_chain.Get(), SkColors::kBlack)) {
    return {nullptr, nullptr};
  }

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
    std::string debug_label,
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
  // GL_TEXTURE_2D is okay to use here as D3D11_BIND_RENDER_TARGET is being
  // used.
  const GLenum texture_target = GL_TEXTURE_2D;

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
    // WebGPU can use RGBA_8888 and RGBA_16 for STORAGE_BINDING.

    if (format == viz::SinglePlaneFormat::kRGBA_8888 ||
        format == viz::SinglePlaneFormat::kRGBA_F16) {
      desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
    }

      // WebGPU can use BGRA_8888 for STORAGE_BINDING when BGRA_8888 is
      // supported as UAV.
    if (format == viz::SinglePlaneFormat::kBGRA_8888) {
      if (SupportsBGRA8UnormStorage()) {
        desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
      } else {
        LOG(ERROR)
            << "D3D11_BIND_UNORDERED_ACCESS is not supported on BGRA_8888";
        return nullptr;
      }
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

  debug_label = "D3DSharedImage_" + debug_label;
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
    std::string debug_label,
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
    std::string debug_label,
    gfx::GpuMemoryBufferHandle handle) {
  // Windows does not support external sampler.
  CHECK(!format.PrefersExternalSampler());
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
    uint32_t usage,
    std::string debug_label) {
  if (!IsPlaneValidForGpuMemoryBufferFormat(plane, buffer_format)) {
    LOG(ERROR) << "Invalid plane " << gfx::BufferPlaneToString(plane)
               << " for format " << gfx::BufferFormatToString(buffer_format);
    return nullptr;
  }

  auto format = viz::GetSharedImageFormat(buffer_format);
  // Format cannot be using external sampling due to checks in
  // `IsPlaneValidForGpuMemoryBufferFormat`.
  if (format.IsLegacyMultiplanar()) {
    CHECK_NE(plane, gfx::BufferPlane::DEFAULT);
  }
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

  if (plane != gfx::BufferPlane::DEFAULT && plane != gfx::BufferPlane::Y &&
      plane != gfx::BufferPlane::UV) {
    LOG(ERROR) << "Invalid buffer plane " << gfx::BufferPlaneToString(plane);
    return nullptr;
  }

  if (handle.type != gfx::DXGI_SHARED_HANDLE || !handle.dxgi_handle.IsValid()) {
    LOG(ERROR) << "Invalid handle with type: " << handle.type;
    return nullptr;
  }

  if (!handle.dxgi_token.has_value()) {
    LOG(ERROR) << "Missing token for DXGI handle";
    return nullptr;
  }

  scoped_refptr<DXGISharedHandleState> dxgi_shared_handle_state =
      dxgi_shared_handle_manager_->GetOrCreateSharedHandleState(
          std::move(handle.dxgi_token.value()), std::move(handle.dxgi_handle),
          d3d11_device_);
  if (!dxgi_shared_handle_state) {
    LOG(ERROR) << "Failed to retrieve matching DXGI shared handle state";
    return nullptr;
  }

  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture =
      dxgi_shared_handle_state->GetOrCreateD3D11Texture(d3d11_device_);
  CHECK(d3d11_texture);

  D3D11_TEXTURE2D_DESC d3d11_texture_desc = {};
  d3d11_texture->GetDesc(&d3d11_texture_desc);

  // TODO: Add checks for device specific limits.
  if (d3d11_texture_desc.Width != static_cast<UINT>(size.width()) ||
      d3d11_texture_desc.Height != static_cast<UINT>(size.height())) {
    LOG(ERROR) << "Size must match texture being opened";
    return nullptr;
  }

  if ((d3d11_texture_desc.Format != GetDXGIFormatForGMB(format)) &&
      (d3d11_texture_desc.Format != GetDXGITypelessFormat(format))) {
    LOG(ERROR) << "Format must match texture being opened";
    return nullptr;
  }

  const GLenum texture_target = GL_TEXTURE_2D;
  std::unique_ptr<D3DImageBacking> backing;
  if (format.IsLegacyMultiplanar()) {
    // Get format and size per plane. For multiplanar formats, `plane_format` is
    // R/RG based on channels in plane.
    const gfx::Size plane_size = GetPlaneSize(plane, size);
    const viz::SharedImageFormat plane_format =
        viz::GetSharedImageFormat(GetPlaneBufferFormat(plane, buffer_format));
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
