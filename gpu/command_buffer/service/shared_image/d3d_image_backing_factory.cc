// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/d3d_image_backing_factory.h"

#include <d3d11_1.h>

#include "base/memory/shared_memory_mapping.h"
#include "base/win/scoped_handle.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/dxgi_shared_handle_manager.h"
#include "gpu/command_buffer/service/shared_image/d3d_image_backing.h"
#include "gpu/command_buffer/service/shared_image/d3d_image_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/config/gpu_finch_features.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/color_space_win.h"
#include "ui/gl/direct_composition_support.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_utils.h"

namespace gpu {

namespace {

// Formats supported by CreateSharedImage() for uploading initial data.
bool IsFormatSupportedForInitialData(viz::SharedImageFormat format) {
  // The set of formats is artificially limited to avoid needing to handle
  // formats outside of what is required. If more are needed, we may need to
  // adjust our initial data's packing or the |D3D11_SUBRESOURCE_DATA|'s pitch.
  return format == viz::SinglePlaneFormat::kRGBA_8888 ||
         format == viz::SinglePlaneFormat::kBGRA_8888;
}

bool IsFormatSupportedForDCompTexture(DXGI_FORMAT dxgi_format) {
  switch (dxgi_format) {
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_YUY2:
    case DXGI_FORMAT_420_OPAQUE:
    case DXGI_FORMAT_P010:
      return true;
    default:
      return false;
  }
}

bool IsColorSpaceSupportedForDCompTexture(
    DXGI_COLOR_SPACE_TYPE dxgi_color_space) {
  switch (dxgi_color_space) {
    case DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709:
    case DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709:
    case DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P709:
    case DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P2020:
    case DXGI_COLOR_SPACE_YCBCR_FULL_G22_NONE_P709_X601:
    case DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P601:
    case DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P601:
    case DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709:
    case DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709:
    case DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P2020:
    case DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P2020:
    case DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020:
    case DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_LEFT_P2020:
    case DXGI_COLOR_SPACE_RGB_STUDIO_G2084_NONE_P2020:
    case DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_TOPLEFT_P2020:
    case DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_TOPLEFT_P2020:
    case DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020:
      return true;

    default:
      return false;
  }
}

// Returns |true| if |desc| describes a texture that we can wrap in an
// |IDCompositionTexture|.
bool DCompTextureIsSupported(const D3D11_TEXTURE2D_DESC& desc) {
  if (!gl::DirectCompositionTextureSupported()) {
    return false;
  }

  return desc.MipLevels == 1 && desc.ArraySize == 1 &&
         IsFormatSupportedForDCompTexture(desc.Format) &&
         desc.SampleDesc.Count == 1 && desc.SampleDesc.Quality == 0 &&
         desc.Usage == D3D11_USAGE_DEFAULT &&
         (desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) != 0 &&
         desc.CPUAccessFlags == 0 &&
         desc.MiscFlags ==
             (D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE);
}

// Formats supported by CreateSharedImage() with no GpuMemoryBufferHandle.
DXGI_FORMAT GetDXGIFormatForCreateTexture(viz::SharedImageFormat format) {
  if (format == viz::SinglePlaneFormat::kRGBA_F16) {
    return DXGI_FORMAT_R16G16B16A16_FLOAT;
  } else if (format == viz::SinglePlaneFormat::kBGRA_8888) {
    return DXGI_FORMAT_B8G8R8A8_UNORM;
  } else if (format == viz::SinglePlaneFormat::kRGBA_8888) {
    return DXGI_FORMAT_R8G8B8A8_UNORM;
  } else if (format == viz::SinglePlaneFormat::kBGRX_8888) {
    return DXGI_FORMAT_B8G8R8A8_UNORM;
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
  } else if (format == viz::MultiPlaneFormat::kNV12) {
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

bool UseUpdateSubresource1(const GpuDriverBugWorkarounds& workarounds) {
  return base::FeatureList::IsEnabled(
             features::kD3DBackingUploadWithUpdateSubresource) &&
         !workarounds.disable_d3d11_update_subresource1;
}

constexpr SharedImageUsageSet kSupportedUsage =
    SHARED_IMAGE_USAGE_GLES2_READ | SHARED_IMAGE_USAGE_GLES2_WRITE |
    SHARED_IMAGE_USAGE_GLES2_FOR_RASTER_ONLY |
    SHARED_IMAGE_USAGE_DISPLAY_WRITE | SHARED_IMAGE_USAGE_DISPLAY_READ |
    SHARED_IMAGE_USAGE_RASTER_READ | SHARED_IMAGE_USAGE_RASTER_WRITE |
    SHARED_IMAGE_USAGE_RASTER_OVER_GLES2_ONLY |
    SHARED_IMAGE_USAGE_OOP_RASTERIZATION | SHARED_IMAGE_USAGE_SCANOUT |
    SHARED_IMAGE_USAGE_WEBGPU_READ | SHARED_IMAGE_USAGE_WEBGPU_WRITE |
    SHARED_IMAGE_USAGE_VIDEO_DECODE |
    SHARED_IMAGE_USAGE_WEBGPU_SWAP_CHAIN_TEXTURE |
    SHARED_IMAGE_USAGE_HIGH_PERFORMANCE_GPU | SHARED_IMAGE_USAGE_CPU_UPLOAD |
    SHARED_IMAGE_USAGE_WEBGPU_STORAGE_TEXTURE |
    SHARED_IMAGE_USAGE_RASTER_DELEGATED_COMPOSITING |
    SHARED_IMAGE_USAGE_WEBGPU_SHARED_BUFFER;

}  // anonymous namespace

D3DImageBackingFactory::D3DImageBackingFactory(
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
    scoped_refptr<DXGISharedHandleManager> dxgi_shared_handle_manager,
    const GLFormatCaps& gl_format_caps,
    const GpuDriverBugWorkarounds& workarounds)
    : SharedImageBackingFactory(kSupportedUsage),
      d3d11_device_(std::move(d3d11_device)),
      dxgi_shared_handle_manager_(std::move(dxgi_shared_handle_manager)),
      angle_d3d11_device_(gl::QueryD3D11DeviceObjectFromANGLE()),
      gl_format_caps_(gl_format_caps),
      use_update_subresource1_(UseUpdateSubresource1(workarounds)) {
  CHECK(angle_d3d11_device_);
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
bool D3DImageBackingFactory::IsSwapChainSupported(
    const GpuPreferences& gpu_preferences) {
  // TODO(crbug.com/40074896): enable swapchain support when d3d11 is shared
  // with ANGLE.
  return gl::DirectCompositionSupported() &&
         gl::DXGISwapChainTearingSupported() &&
         gpu_preferences.gr_context_type == GrContextType::kGL;
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
                                        gpu::SharedImageUsageSet usage) {
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
      gl_format_caps_, /*is_back_buffer=*/true);
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
      gl_format_caps_, /*is_back_buffer=*/false);
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
    SharedImageUsageSet usage,
    std::string debug_label,
    bool is_thread_safe) {
  return CreateSharedImage(mailbox, format, size, color_space, surface_origin,
                           alpha_type, usage, std::move(debug_label),
                           is_thread_safe, base::span<const uint8_t>());
}

std::unique_ptr<SharedImageBacking> D3DImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    SharedImageUsageSet usage,
    std::string debug_label,
    bool is_thread_safe,
    base::span<const uint8_t> pixel_data) {
  DCHECK(!is_thread_safe);

  if (usage.Has(SHARED_IMAGE_USAGE_WEBGPU_SHARED_BUFFER)) {
    return CreateSharedBufferD3D12(mailbox, size, color_space, surface_origin,
                                   alpha_type, usage, debug_label);
  }

  // Without D3D11, we cannot do shared images. This will happen if we're
  // running with Vulkan, D3D12, D3D9, GL or with the non-passthrough command
  // decoder in tests.
  if (!d3d11_device_) {
    return nullptr;
  }

  DXGI_FORMAT dxgi_format = GetDXGIFormatForCreateTexture(format);
  DCHECK_NE(dxgi_format, DXGI_FORMAT_UNKNOWN);

  // GL_TEXTURE_2D is ok to use here as D3D11_BIND_RENDER_TARGET is being used.
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

  if (usage.Has(gpu::SHARED_IMAGE_USAGE_WEBGPU_STORAGE_TEXTURE) &&
      format.is_single_plane()) {
    DCHECK(usage.HasAny(SHARED_IMAGE_USAGE_WEBGPU_READ |
                        SHARED_IMAGE_USAGE_WEBGPU_WRITE));
    // WebGPU can always use RGBA_8888 and RGBA_16 for STORAGE_BINDING.
    if (format == viz::SinglePlaneFormat::kRGBA_8888 ||
        format == viz::SinglePlaneFormat::kRGBA_F16) {
      desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
    }

    // WebGPU can use BGRA_8888 for STORAGE_BINDING only when supported for UAV.
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

  const bool has_webgpu_usage = usage.HasAny(SHARED_IMAGE_USAGE_WEBGPU_READ |
                                             SHARED_IMAGE_USAGE_WEBGPU_WRITE);
  const bool has_gl_usage = HasGLES2ReadOrWriteUsage(usage);
  const bool want_dcomp_texture =
      usage.Has(SHARED_IMAGE_USAGE_SCANOUT) &&
      IsFormatSupportedForDCompTexture(desc.Format) &&
      IsColorSpaceSupportedForDCompTexture(
          gfx::ColorSpaceWin::GetDXGIColorSpace(color_space));
  // TODO(crbug.com/40204134): Look into using DXGI handle when MF VEA is used.
  const bool needs_cross_device_synchronization =
      has_webgpu_usage ||
      (has_gl_usage && (d3d11_device_ != angle_d3d11_device_));
  const bool needs_shared_handle =
      needs_cross_device_synchronization || want_dcomp_texture;
  if (needs_shared_handle) {
    // TODO(crbug.com/40068319): Many texture formats cannot be shared on old
    // GPUs/drivers to try to detect that and implement a fallback path or
    // disallow Graphite/WebGPU in those cases.
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE |
                     (gfx::D3DSharedFence::IsSupported(d3d11_device_.Get())
                          ? D3D11_RESOURCE_MISC_SHARED
                          : D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX);
  }

  D3D11_SUBRESOURCE_DATA initial_data = {};
  if (!pixel_data.empty()) {
    if (!IsFormatSupportedForInitialData(format)) {
      LOG(ERROR) << "Unsupported format: " << format.ToString();
      return nullptr;
    }
    if (pixel_data.size_bytes() < format.EstimatedSizeInBytes(size)) {
      LOG(ERROR) << "Not enough pixel data";
      return nullptr;
    }

    initial_data.pSysMem = pixel_data.data();
    initial_data.SysMemPitch = format.BitsPerPixel() * size.width() / 8;
    initial_data.SysMemSlicePitch = 0;
  }

  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture;
  HRESULT hr = d3d11_device_->CreateTexture2D(
      &desc, initial_data.pSysMem ? &initial_data : nullptr, &d3d11_texture);
  if (FAILED(hr)) {
    LOG(ERROR) << "CreateTexture2D failed with error " << std::hex << hr;
    return nullptr;
  }

  debug_label = "D3DSharedImage_" + debug_label;
  d3d11_texture->SetPrivateData(WKPDID_D3DDebugObjectName, debug_label.length(),
                                debug_label.c_str());

  scoped_refptr<DXGISharedHandleState> dxgi_shared_handle_state;
  if (needs_cross_device_synchronization) {
    Microsoft::WRL::ComPtr<IDXGIResource1> dxgi_resource;
    hr = d3d11_texture.As(&dxgi_resource);
    if (FAILED(hr)) {
      LOG(ERROR) << "QueryInterface for IDXGIResource failed with error "
                 << std::hex << hr;
      return nullptr;
    }

    HANDLE shared_handle;
    hr = dxgi_resource->CreateSharedHandle(
        nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
        nullptr, &shared_handle);
    if (FAILED(hr)) {
      LOG(ERROR) << "Unable to create shared handle for DXGIResource "
                 << std::hex << hr;
      return nullptr;
    }

    dxgi_shared_handle_state =
        dxgi_shared_handle_manager_->CreateAnonymousSharedHandleState(
            base::win::ScopedHandle(shared_handle), d3d11_texture);
  }

  Microsoft::WRL::ComPtr<IDCompositionTexture> dcomp_texture;
  if (want_dcomp_texture) {
    // If this trips, it means we're claiming support for SCANOUT when DComp
    // textures is not supported by the system, or an incompatible texture was
    // created above.
    CHECK(DCompTextureIsSupported(desc));

    Microsoft::WRL::ComPtr<IDCompositionDevice3> dcomp_device =
        gl::GetDirectCompositionDevice();
    Microsoft::WRL::ComPtr<IDCompositionDevice4> dcomp_device4;
    hr = dcomp_device.As(&dcomp_device4);
    CHECK_EQ(hr, S_OK) << ", QueryInterface failed: "
                       << logging::SystemErrorCodeToString(hr);

    hr = dcomp_device4->CreateCompositionTexture(d3d11_texture.Get(),
                                                 &dcomp_texture);
    CHECK_EQ(hr, S_OK) << ", CreateCompositionTexture failed: "
                       << logging::SystemErrorCodeToString(hr);

    hr = dcomp_texture->SetAlphaMode(SkAlphaTypeIsOpaque(alpha_type)
                                         ? DXGI_ALPHA_MODE_IGNORE
                                         : DXGI_ALPHA_MODE_PREMULTIPLIED);
    CHECK_EQ(hr, S_OK) << ", SetAlphaMode failed: "
                       << logging::SystemErrorCodeToString(hr);

    hr = dcomp_texture->SetColorSpace(
        gfx::ColorSpaceWin::GetDXGIColorSpace(color_space));
    CHECK_EQ(hr, S_OK) << ", SetColorSpace failed: "
                       << logging::SystemErrorCodeToString(hr);
  }

  // SkiaOutputDeviceDComp will hold onto DComp texture overlay accesses for
  // longer than a frame, due to DWM synchronization requirements. This is
  // incompatible with the assumption that keyed mutex access will be minimal.
  CHECK(!dcomp_texture || !(dxgi_shared_handle_state &&
                            dxgi_shared_handle_state->has_keyed_mutex()));

  auto backing = D3DImageBacking::Create(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(debug_label), std::move(d3d11_texture),
      std::move(dcomp_texture), std::move(dxgi_shared_handle_state),
      gl_format_caps_, texture_target, /*array_slice=*/0u,
      use_update_subresource1_);
  if (backing && !pixel_data.empty()) {
    backing->SetCleared();
  }

  return backing;
}

std::unique_ptr<SharedImageBacking> D3DImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    SharedImageUsageSet usage,
    std::string debug_label,
    gfx::GpuMemoryBufferHandle handle) {
  // Windows does not support external sampler.
  CHECK(!format.PrefersExternalSampler());

  // TOOD(hitawala): Move this size check to IsSupported.
  const gfx::BufferFormat buffer_format = gpu::ToBufferFormat(format);
  if (!gpu::IsImageSizeValidForGpuMemoryBufferFormat(size, buffer_format)) {
    LOG(ERROR) << "Invalid image size " << size.ToString() << " for "
               << gfx::BufferFormatToString(buffer_format);
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

  std::unique_ptr<D3DImageBacking> backing = D3DImageBacking::Create(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(debug_label), std::move(d3d11_texture),
      /*dcomp_texture=*/nullptr, std::move(dxgi_shared_handle_state),
      gl_format_caps_, /*texture_target=*/GL_TEXTURE_2D, /*array_slice=*/0u,
      use_update_subresource1_);

  if (backing) {
    backing->SetCleared();
  }
  return backing;
}

std::unique_ptr<SharedImageBacking>
D3DImageBackingFactory::CreateSharedBufferD3D12(
    const Mailbox& mailbox,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    SharedImageUsageSet usage,
    std::string debug_label) {
  if (!d3d12_device_) {
    // Lazily create a D3D12 Device by acquiring the DXGI adapter of the
    // existing D3D11 device.
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
    HRESULT hr = d3d11_device_.As(&dxgi_device);
    CHECK_EQ(hr, S_OK);

    Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
    hr = dxgi_device->GetAdapter(&dxgi_adapter);
    CHECK_EQ(hr, S_OK);

    hr = D3D12CreateDevice(dxgi_adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                           IID_PPV_ARGS(&d3d12_device_));

    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to create a D3D12 device."
                 << logging::SystemErrorCodeToString(hr);
      return nullptr;
    }
  }

  if (size.height() != 1) {
    LOG(ERROR) << "Height must be 1 when creating a shared buffer.";
    return nullptr;
  }

  if (color_space != gfx::ColorSpace()) {
    LOG(ERROR) << "Color spaces are not supported for buffer-backed shared "
                  "images. Only gfx::ColorSpace() is accepted.";
    return nullptr;
  }

  if (surface_origin != kTopLeft_GrSurfaceOrigin) {
    LOG(ERROR) << "Surface origin is not supported for buffer-backed shared "
                  "images. Only kkTopLeft_GrSurfaceOrigin is accepted.";
  }

  if (alpha_type != kUnknown_SkAlphaType) {
    LOG(ERROR) << "Alpha type is not supported for buffer-backed shared "
                  "images. Only kUnknown_SkAlphaType is accepted.";
  }

  // The passed usages AND-ed with the compliment of the OR-d valid usages
  // should be zero.
  if (usage &
      ~(SHARED_IMAGE_USAGE_WEBGPU_READ | SHARED_IMAGE_USAGE_WEBGPU_WRITE |
        SHARED_IMAGE_USAGE_WEBGPU_SHARED_BUFFER)) {
    LOG(ERROR) << "Only shared image usages SHARED_IMAGE_USAGE_WEBGPU_READ, "
                  "SHARED_IMAGE_USAGE_WEBGPU_WRITE, and "
                  "SHARED_IMAGE_USAGE_WEBGPU_SHARED_BUFFER are allowed when "
                  "creating a buffer-backed shared image.";
  }

  D3D12_HEAP_PROPERTIES heap_properties;
  heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT,
  heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heap_properties.CreationNodeMask = 1;
  heap_properties.VisibleNodeMask = 1;

  D3D12_RESOURCE_DESC desc;
  desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  desc.Alignment = 0;
  desc.Width = size.width();
  desc.Height = 1;
  desc.DepthOrArraySize = 1;
  desc.MipLevels = 1;
  desc.Format = DXGI_FORMAT_UNKNOWN;
  desc.SampleDesc = {1, 0};
  desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

  Microsoft::WRL::ComPtr<ID3D12Resource> resource;
  HRESULT hr = d3d12_device_->CreateCommittedResource(
      &heap_properties, D3D12_HEAP_FLAG_NONE, &desc,
      D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&resource));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to create D3D12 resource: "
               << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  debug_label = "D3DSharedBuffer_" + debug_label;
  hr = resource->SetPrivateData(WKPDID_D3DDebugObjectName, debug_label.length(),
                                debug_label.c_str());

  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to set resource debug name: "
               << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  auto backing = D3DImageBacking::CreateFromD3D12Resource(
      mailbox, size, usage, std::move(debug_label), std::move(resource));

  // CreateCommittedResource will zero the resource for us, which means we can
  // set it as cleared.
  backing->SetCleared();
  return backing;
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

bool D3DImageBackingFactory::IsSupported(SharedImageUsageSet usage,
                                         viz::SharedImageFormat format,
                                         const gfx::Size& size,
                                         bool thread_safe,
                                         gfx::GpuMemoryBufferType gmb_type,
                                         GrContextType gr_context_type,
                                         base::span<const uint8_t> pixel_data) {
  if (!pixel_data.empty() && !IsFormatSupportedForInitialData(format)) {
    return false;
  }

  const bool is_scanout = usage.Has(gpu::SHARED_IMAGE_USAGE_SCANOUT);
  const bool is_video_decode = usage.Has(gpu::SHARED_IMAGE_USAGE_VIDEO_DECODE);
  const bool is_buffer =
      usage.Has(gpu::SHARED_IMAGE_USAGE_WEBGPU_SHARED_BUFFER);
  if (is_scanout) {
    if (is_video_decode || gmb_type == gfx::DXGI_SHARED_HANDLE) {
      // Video decode and video frames via GMBs are handled specially in
      // |SwapChainPresenter|, so we must assume it's safe to create a scanout
      // image backing for it.
    } else if (gmb_type == gfx::EMPTY_BUFFER) {
      return gl::DirectCompositionTextureSupported() &&
             IsFormatSupportedForDCompTexture(
                 GetDXGIFormatForCreateTexture(format));
    }
  }

  if (gmb_type == gfx::EMPTY_BUFFER) {
    if (GetDXGIFormatForCreateTexture(format) == DXGI_FORMAT_UNKNOWN &&
        !is_buffer) {
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

SharedImageBackingType D3DImageBackingFactory::GetBackingType() {
  return SharedImageBackingType::kD3D;
}

}  // namespace gpu
