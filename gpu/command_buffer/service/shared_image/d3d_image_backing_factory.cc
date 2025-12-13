// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/d3d_image_backing_factory.h"

#include <d3d11_1.h>

// clang-format off
#include <dawn/native/D3D11Backend.h>
// clang-format on

#include "base/memory/shared_memory_mapping.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/trace_event.h"
#include "base/win/scoped_handle.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/dxgi_shared_handle_manager.h"
#include "gpu/command_buffer/service/shared_image/d3d_image_backing.h"
#include "gpu/command_buffer/service/shared_image/d3d_image_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/ipc/common/dxgi_helpers.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/buffer_usage_util.h"
#include "ui/gfx/color_space_win.h"
#include "ui/gl/direct_composition_support.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_utils.h"

#if BUILDFLAG(SKIA_USE_DAWN)
#include "gpu/command_buffer/service/dawn_context_provider.h"
#endif

namespace gpu {

namespace {

class GpuMemoryBufferHandleSharedState {
 public:
  GpuMemoryBufferHandleSharedState() { DETACH_FROM_THREAD(thread_checker_); }
  ~GpuMemoryBufferHandleSharedState() = default;

  GpuMemoryBufferHandleSharedState(const GpuMemoryBufferHandleSharedState&) =
      delete;
  GpuMemoryBufferHandleSharedState& operator=(
      const GpuMemoryBufferHandleSharedState&) = delete;

  // TODO(crbug.com/40774668): Avoid the need for a separate D3D device here by
  // sharing keyed mutex state between DXGI GMBs and D3D shared image backings.
  Microsoft::WRL::ComPtr<ID3D11Device> GetOrCreateD3D11Device() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    if (!d3d11_device_ || FAILED(d3d11_device_->GetDeviceRemovedReason())) {
      // Reset device if it was removed.
      d3d11_device_ = nullptr;
      // Use same adapter as ANGLE device.
      auto angle_d3d11_device = gl::QueryD3D11DeviceObjectFromANGLE();
      if (!angle_d3d11_device) {
        DLOG(ERROR) << "Failed to get ANGLE D3D11 device";
        return nullptr;
      }

      Microsoft::WRL::ComPtr<IDXGIDevice> angle_dxgi_device;
      HRESULT hr = angle_d3d11_device.As(&angle_dxgi_device);
      CHECK_EQ(hr, S_OK);

      Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter = nullptr;
      hr = FAILED(angle_dxgi_device->GetAdapter(&dxgi_adapter));
      if (FAILED(hr)) {
        DLOG(ERROR) << "GetAdapter failed with error 0x" << std::hex << hr;
        return nullptr;
      }

      // If adapter is not null, driver type must be D3D_DRIVER_TYPE_UNKNOWN
      // otherwise D3D11CreateDevice will return E_INVALIDARG.
      // See
      // https://docs.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-d3d11createdevice#return-value
      const D3D_DRIVER_TYPE driver_type =
          dxgi_adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE;

      // It's ok to use D3D11_CREATE_DEVICE_SINGLETHREADED because this device
      // is only ever used on the IO thread (verified by |thread_checker_|).
      const UINT flags = D3D11_CREATE_DEVICE_SINGLETHREADED;

      // Using D3D_FEATURE_LEVEL_11_1 is ok since we only support D3D11 when the
      // platform update containing DXGI 1.2 is present on Win7.
      const D3D_FEATURE_LEVEL feature_levels[] = {
          D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
          D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
          D3D_FEATURE_LEVEL_9_3,  D3D_FEATURE_LEVEL_9_2,
          D3D_FEATURE_LEVEL_9_1};

      hr = D3D11CreateDevice(dxgi_adapter.Get(), driver_type,
                             /*Software=*/nullptr, flags, feature_levels,
                             std::size(feature_levels), D3D11_SDK_VERSION,
                             &d3d11_device_, /*pFeatureLevel=*/nullptr,
                             /*ppImmediateContext=*/nullptr);
      if (FAILED(hr)) {
        DLOG(ERROR) << "D3D11CreateDevice failed with error 0x" << std::hex
                    << hr;
        return nullptr;
      }

      const char* kDebugName = "GPUIPC_GpuMemoryBufferHandleSharedState";
      d3d11_device_->SetPrivateData(WKPDID_D3DDebugObjectName,
                                    strlen(kDebugName), kDebugName);
    }
    DCHECK(d3d11_device_);
    return d3d11_device_;
  }

  Microsoft::WRL::ComPtr<ID3D11Texture2D> staging_texture() {
    return staging_texture_;
  }

 private:
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_
      GUARDED_BY_CONTEXT(thread_checker_);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> staging_texture_;
  THREAD_CHECKER(thread_checker_);
};

GpuMemoryBufferHandleSharedState* GetGpuMemoryBufferHandleSharedState() {
  static auto* factory = new GpuMemoryBufferHandleSharedState();
  return factory;
}

Microsoft::WRL::ComPtr<ID3D11Device> GetD3D11DeviceForGMBHandles() {
  return GetGpuMemoryBufferHandleSharedState()->GetOrCreateD3D11Device();
}

Microsoft::WRL::ComPtr<ID3D11Texture2D> GetStagingTextureForGMBHandleCopies() {
  return GetGpuMemoryBufferHandleSharedState()->staging_texture();
}

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

// CPU_READ is needed for RenderableGMBVideoFramePool case where the DXGI handle
// is mapped on GPU process.
constexpr SharedImageUsageSet kSupportedUsage =
    SHARED_IMAGE_USAGE_GLES2_READ | SHARED_IMAGE_USAGE_GLES2_WRITE |
    SHARED_IMAGE_USAGE_DISPLAY_WRITE | SHARED_IMAGE_USAGE_DISPLAY_READ |
    SHARED_IMAGE_USAGE_RASTER_READ | SHARED_IMAGE_USAGE_RASTER_WRITE |
    SHARED_IMAGE_USAGE_SCANOUT | SHARED_IMAGE_USAGE_WEBGPU_READ |
    SHARED_IMAGE_USAGE_WEBGPU_WRITE | SHARED_IMAGE_USAGE_VIDEO_DECODE |
    SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE |
    SHARED_IMAGE_USAGE_WEBGPU_SWAP_CHAIN_TEXTURE |
    SHARED_IMAGE_USAGE_HIGH_PERFORMANCE_GPU | SHARED_IMAGE_USAGE_CPU_UPLOAD |
    SHARED_IMAGE_USAGE_WEBGPU_STORAGE_TEXTURE |
    SHARED_IMAGE_USAGE_WEBGPU_SHARED_BUFFER | SHARED_IMAGE_USAGE_CPU_READ |
    SHARED_IMAGE_USAGE_CPU_WRITE_ONLY | SHARED_IMAGE_USAGE_WEBNN_SHARED_TENSOR |
    SHARED_IMAGE_USAGE_WEBNN_SHARED_TENSOR_WRITE |
    SHARED_IMAGE_USAGE_WEBNN_SHARED_TENSOR_READ;

const char* kD3DImageBackingLabel = "D3DImageBacking";

}  // anonymous namespace

D3DImageBackingFactory::D3DImageBackingFactory(
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
    scoped_refptr<DXGISharedHandleManager> dxgi_shared_handle_manager,
    const GLFormatCaps& gl_format_caps,
    const GpuDriverBugWorkarounds& workarounds,
    bool enable_webnn_only_d3d_factory)
    : SharedImageBackingFactory(kSupportedUsage),
      d3d11_device_(std::move(d3d11_device)),
      dxgi_shared_handle_manager_(std::move(dxgi_shared_handle_manager)),
      angle_d3d11_device_(gl::QueryD3D11DeviceObjectFromANGLE()),
      gl_format_caps_(gl_format_caps),
      use_update_subresource1_(UseUpdateSubresource1(workarounds)),
      enable_webnn_only_d3d_factory_(enable_webnn_only_d3d_factory) {
  CHECK(angle_d3d11_device_ || enable_webnn_only_d3d_factory)
      << "D3DImageBackingFactory requires a D3D11 device.";

  if (d3d11_device_) {
    UINT format_support;
    HRESULT hr =
        d3d11_device_->CheckFormatSupport(DXGI_FORMAT_NV12, &format_support);
    constexpr auto kRequiredUsage = D3D11_FORMAT_SUPPORT_TEXTURE2D |
                                    D3D11_FORMAT_SUPPORT_SHADER_SAMPLE |
                                    D3D11_FORMAT_SUPPORT_RENDER_TARGET;
    bool has_required_format_support =
        (format_support & kRequiredUsage) == kRequiredUsage;
    d3d11_supports_nv12_ = SUCCEEDED(hr) && has_required_format_support;

    D3D_FEATURE_LEVEL feature_level = d3d11_device_->GetFeatureLevel();
    if (feature_level < D3D_FEATURE_LEVEL_9_3) {
      max_nv12_dim_supported_ = 2048;
    } else if (feature_level < D3D_FEATURE_LEVEL_11_0) {
      max_nv12_dim_supported_ = 4096;
    } else {
      max_nv12_dim_supported_ = 16384;
    }
  } else {
    d3d11_supports_nv12_ = false;
  }
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
gfx::GpuMemoryBufferHandle
D3DImageBackingFactory::CreateGpuMemoryBufferHandleOnIO(
    scoped_refptr<base::SingleThreadTaskRunner> io_runner,
    const gfx::Size& size,
    viz::SharedImageFormat format,
    gfx::BufferUsage usage) {
  gfx::GpuMemoryBufferHandle result;
  base::WaitableEvent event;

  io_runner->PostTask(
      FROM_HERE, base::BindOnce(
                     [](gfx::GpuMemoryBufferHandle* out_gmb_handle,
                        base::WaitableEvent* waitable_event,
                        scoped_refptr<base::SingleThreadTaskRunner> io_runner,
                        const gfx::Size& size, viz::SharedImageFormat format,
                        gfx::BufferUsage usage) {
                       *out_gmb_handle =
                           D3DImageBackingFactory::CreateGpuMemoryBufferHandle(
                               io_runner, size, format, usage);

                       waitable_event->Signal();
                     },
                     &result, &event, io_runner, size, format, usage));

  event.Wait();

  return result;
}

// static
gfx::GpuMemoryBufferHandle D3DImageBackingFactory::CreateGpuMemoryBufferHandle(
    scoped_refptr<base::SingleThreadTaskRunner> io_runner,
    const gfx::Size& size,
    viz::SharedImageFormat format,
    gfx::BufferUsage usage) {
  if (io_runner && !io_runner->BelongsToCurrentThread()) {
    // Thread-hop is required!
    return CreateGpuMemoryBufferHandleOnIO(io_runner, size, format, usage);
  }

  TRACE_EVENT0("gpu", "D3DImageBackingFactory::CrceateGpuMemoryBufferHandle");

  gfx::GpuMemoryBufferHandle handle;
  auto d3d11_device = GetD3D11DeviceForGMBHandles();
  if (!d3d11_device) {
    return handle;
  }

  DXGI_FORMAT dxgi_format = gpu::ToDXGIFormat(format);
  if (dxgi_format == DXGI_FORMAT_UNKNOWN) {
    return handle;
  }

  auto buffer_size = viz::SharedMemorySizeForSharedImageFormat(format, size);
  if (!buffer_size) {
    return handle;
  }

  // We are binding as a shader resource and render target regardless of usage,
  // so make sure that the usage is one that we support.
  DCHECK(usage == gfx::BufferUsage::GPU_READ ||
         usage == gfx::BufferUsage::SCANOUT ||
         usage == gfx::BufferUsage::SCANOUT_CPU_READ_WRITE)
      << "Incorrect usage, usage=" << gfx::BufferUsageToString(usage);

  D3D11_TEXTURE2D_DESC desc = {
      static_cast<UINT>(size.width()),
      static_cast<UINT>(size.height()),
      1,
      1,
      dxgi_format,
      {1, 0},
      D3D11_USAGE_DEFAULT,
      D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
      0,
      D3D11_RESOURCE_MISC_SHARED_NTHANDLE |
          D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX};

  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture;

  if (FAILED(d3d11_device->CreateTexture2D(&desc, nullptr, &d3d11_texture))) {
    return handle;
  }

  Microsoft::WRL::ComPtr<IDXGIResource1> dxgi_resource;
  const HRESULT hr = d3d11_texture.As(&dxgi_resource);
  CHECK_EQ(hr, S_OK);

  HANDLE texture_handle;
  if (FAILED(dxgi_resource->CreateSharedHandle(
          nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
          nullptr, &texture_handle))) {
    return handle;
  }

  handle = gfx::GpuMemoryBufferHandle(
      gfx::DXGIHandle(base::win::ScopedHandle(texture_handle)));

  return handle;
}

// static
bool D3DImageBackingFactory::CopyNativeBufferToSharedMemoryAsync(
    gfx::GpuMemoryBufferHandle buffer_handle,
    base::UnsafeSharedMemoryRegion shared_memory) {
  DCHECK_EQ(buffer_handle.type, gfx::GpuMemoryBufferType::DXGI_SHARED_HANDLE);
  auto d3d11_device = GetD3D11DeviceForGMBHandles();
  if (!d3d11_device) {
    return false;
  }

  base::WritableSharedMemoryMapping mapping = shared_memory.Map();
  if (!mapping.IsValid()) {
    return false;
  }

  return CopyDXGIBufferToShMem(buffer_handle.dxgi_handle().buffer_handle(),
                               mapping.GetMemoryAsSpan<uint8_t>(),
                               d3d11_device.Get(),
                               &GetStagingTextureForGMBHandleCopies());
}

// static
bool D3DImageBackingFactory::IsD3DSharedImageSupported(
    ID3D11Device* d3d11_device,
    const GpuPreferences& gpu_preferences) {
  // Only supported for passthrough command decoder.
  if (!gpu_preferences.use_passthrough_cmd_decoder) {
    return false;
  }

  // D3D11 device will be null if ANGLE is using the D3D9 backend or
  // when we're running with Graphite on D3D12.
  if (!d3d11_device) {
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
    const GpuPreferences& gpu_preferences,
    DawnContextProvider* dawn_context_provider /*=nullptr*/) {
  if (gpu_preferences.gr_context_type == GrContextType::kGraphiteDawn) {
    // This is only supported if graphite and ANGLE share the same D3D11 device.
    CHECK(dawn_context_provider);
    auto dawn_d3d11_device = dawn_context_provider->GetD3D11Device();
    auto angle_d3d11_device = gl::QueryD3D11DeviceObjectFromANGLE();
    if (dawn_d3d11_device != angle_d3d11_device) {
      return false;
    }
  }

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

bool D3DImageBackingFactory::CreateSwapChainInternal(
    Microsoft::WRL::ComPtr<IDXGISwapChain1>& swap_chain,
    Microsoft::WRL::ComPtr<ID3D11Texture2D>& back_buffer_texture,
    Microsoft::WRL::ComPtr<ID3D11Texture2D>& front_buffer_texture,
    viz::SharedImageFormat format,
    const gfx::Size& size) {
  DXGI_FORMAT swap_chain_format;
  if (format == viz::SinglePlaneFormat::kRGBA_8888 ||
      format == viz::SinglePlaneFormat::kRGBX_8888) {
    swap_chain_format = DXGI_FORMAT_R8G8B8A8_UNORM;
  } else if (format == viz::SinglePlaneFormat::kBGRA_8888) {
    swap_chain_format = DXGI_FORMAT_B8G8R8A8_UNORM;
  } else if (format == viz::SinglePlaneFormat::kRGBA_F16) {
    swap_chain_format = DXGI_FORMAT_R16G16B16A16_FLOAT;
  } else {
    LOG(ERROR) << format.ToString()
               << " format is not supported by swap chain.";
    return false;
  }

  Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
  HRESULT hr = d3d11_device_.As(&dxgi_device);
  CHECK_EQ(hr, S_OK);
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

  hr = dxgi_factory->CreateSwapChainForComposition(d3d11_device_.Get(), &desc,
                                                   nullptr, &swap_chain);
  if (FAILED(hr)) {
    LOG(ERROR) << "CreateSwapChainForComposition failed with error " << std::hex
               << hr;
    return false;
  }

  gl::LabelSwapChainAndBuffers(swap_chain.Get(), kD3DImageBackingLabel);

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
    return false;
  }

  DXGI_PRESENT_PARAMETERS params = {};
  params.DirtyRectsCount = 0;
  params.pDirtyRects = nullptr;
  hr = swap_chain->Present1(/*interval=*/0, /*flags=*/0, &params);
  if (FAILED(hr)) {
    LOG(ERROR) << "Present1 failed with error " << std::hex << hr;
    return false;
  }

  if (!ClearBackBufferToColor(swap_chain.Get(), SkColors::kBlack)) {
    return false;
  }

  hr = swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer_texture));
  if (FAILED(hr)) {
    LOG(ERROR) << "GetBuffer failed with error " << std::hex;
    return false;
  }

  hr = swap_chain->GetBuffer(1, IID_PPV_ARGS(&front_buffer_texture));
  if (FAILED(hr)) {
    LOG(ERROR) << "GetBuffer failed with error " << std::hex;
    return false;
  }
  return true;
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
  if (usage.Has(SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE)) {
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer_texture;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> front_buffer_texture;
    if (!CreateSwapChainInternal(swap_chain, back_buffer_texture,
                                 front_buffer_texture, format, size)) {
      return nullptr;
    }

    auto backing = D3DImageBacking::CreateFromSwapChainBuffers(
        mailbox, format, size, color_space, surface_origin, alpha_type, usage,
        std::move(back_buffer_texture), std::move(front_buffer_texture),
        swap_chain, gl_format_caps_);
    if (!backing) {
      return nullptr;
    }
    backing->SetCleared();

    return std::move(backing);
  }

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
    gfx::Size buffer_size = size;
    // WebNN tensors have a valid height and format and must be converted to 1D
    // byte buffer.
    if (usage.Has(SHARED_IMAGE_USAGE_WEBNN_SHARED_TENSOR)) {
      if (size.width() <= 0 || size.height() <= 0 ||
          size.width() > std::numeric_limits<int>::max() / size.height()) {
        LOG(ERROR) << "Shared image dimensions for tensor are invalid.";
        return nullptr;
      }

      int element_count = size.width() * size.height();
      int bytes_per_element = format.BytesPerPixel();
      if (element_count > std::numeric_limits<int>::max() / bytes_per_element) {
        LOG(ERROR) << "Shared image size for tensor is invalid.";
        return nullptr;
      }

      buffer_size = gfx::Size(element_count * bytes_per_element, 1);
    }

    return CreateSharedBufferD3D12(mailbox, buffer_size, color_space,
                                   surface_origin, alpha_type, usage,
                                   debug_label);
  }

  // Without D3D11, we cannot do shared images. This will happen if we're
  // running with Vulkan, D3D12, D3D9, GL or with the non-passthrough command
  // decoder in tests.
  if (!d3d11_device_) {
    return nullptr;
  }

  DXGI_FORMAT dxgi_format = ToDXGIFormat(format);
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
    initial_data.SysMemPitch = format.BytesPerPixel() * size.width();
    initial_data.SysMemSlicePitch = 0;
  }

  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture;
  HRESULT hr = d3d11_device_->CreateTexture2D(
      &desc, initial_data.pSysMem ? &initial_data : nullptr, &d3d11_texture);
  if (!SUCCEEDED(hr)) {
    LOG(ERROR) << "CreateTexture2D failed with size " << size.ToString()
               << " error " << std::hex << hr;
    return nullptr;
  }

  debug_label = "D3DSharedImage_" + debug_label;
  d3d11_texture->SetPrivateData(WKPDID_D3DDebugObjectName, debug_label.length(),
                                debug_label.c_str());

  scoped_refptr<DXGISharedHandleState> dxgi_shared_handle_state;
  if (needs_cross_device_synchronization) {
    Microsoft::WRL::ComPtr<IDXGIResource1> dxgi_resource;
    hr = d3d11_texture.As(&dxgi_resource);
    CHECK_EQ(hr, S_OK);

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

  if (want_dcomp_texture) {
    // If this trips, it means we're claiming support for SCANOUT when DComp
    // textures is not supported by the system, or an incompatible texture was
    // created above.
    if (!DCompTextureIsSupported(desc)) {
      LOG(ERROR) << "Composition texture not supported for scanout usage";
      return nullptr;
    }
  }

  // SkiaOutputDeviceDComp will hold onto DComp texture overlay accesses for
  // longer than a frame, due to DWM synchronization requirements. This is
  // incompatible with the assumption that keyed mutex access will be minimal.
  CHECK(!want_dcomp_texture || !(dxgi_shared_handle_state &&
                                 dxgi_shared_handle_state->has_keyed_mutex()));

  auto backing = D3DImageBacking::Create(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(debug_label), std::move(d3d11_texture),
      std::move(dxgi_shared_handle_state), gl_format_caps_, texture_target,
      /*array_slice=*/0u, use_update_subresource1_, want_dcomp_texture,
      /*is_thread_safe=*/false,
      /*share_dxgi_handle_with_other_backings=*/false);
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
    bool is_thread_safe,
    gfx::GpuMemoryBufferHandle handle) {
  // Windows does not support external sampler.
  CHECK(!format.PrefersExternalSampler());

  // TOOD(hitawala): Move this size check to IsSupported.
  if (!IsSizeForBufferHandleValid(size, format)) {
    LOG(ERROR) << "Invalid image size " << size.ToString() << " for "
               << format.ToString();
    return nullptr;
  }

  if (handle.type != gfx::DXGI_SHARED_HANDLE) {
    LOG(ERROR) << "Invalid handle with type: " << handle.type;
    return nullptr;
  }

  gfx::DXGIHandle dxgi_handle = std::move(handle).dxgi_handle();
  // This shouldn't happen as the GpuMemoryBufferHandle constructor that takes a
  // DXGIHandle asserts the handle is valid. However, it is currently possible
  // for code to set the type to DXGI_SHARED_HANDLE directly but never actually
  // set the handle. Make this an eventual CHECK() but handle this gracefully
  // for now just in case.
  CHECK(dxgi_handle.IsValid(), base::NotFatalUntil::M138);
  if (!dxgi_handle.IsValid()) {
    return nullptr;
  }
  scoped_refptr<DXGISharedHandleState> dxgi_shared_handle_state =
      dxgi_shared_handle_manager_->GetOrCreateSharedHandleState(
          dxgi_handle.token(), dxgi_handle.TakeBufferHandle(), d3d11_device_);
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
      std::move(dxgi_shared_handle_state), gl_format_caps_,
      /*texture_target=*/GL_TEXTURE_2D, /*array_slice=*/0u,
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
  // TODO(crbug.com/345352987): replace with IsSupported().
  constexpr auto kValidWebNNUsage = SHARED_IMAGE_USAGE_WEBGPU_READ |
                                    SHARED_IMAGE_USAGE_WEBGPU_WRITE |
                                    SHARED_IMAGE_USAGE_WEBGPU_SHARED_BUFFER;
  if (!kValidWebNNUsage.HasAll(usage)) {
    LOG(ERROR) << "Only shared image usages SHARED_IMAGE_USAGE_WEBGPU_READ, "
                  "SHARED_IMAGE_USAGE_WEBGPU_WRITE, and "
                  "SHARED_IMAGE_USAGE_WEBGPU_SHARED_BUFFER are allowed when "
                  "creating a buffer-backed shared image.";
  }

  uint64_t buffer_width = size.width();

  D3D12_HEAP_PROPERTIES heap_properties;
  heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT,
  heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heap_properties.CreationNodeMask = 1;
  heap_properties.VisibleNodeMask = 1;

  if (usage.Has(SHARED_IMAGE_USAGE_WEBNN_SHARED_TENSOR)) {
    // DML requires buffers to be in multiple of 4 bytes.
    // https://learn.microsoft.com/en-us/windows/ai/directml/dml-helper-functions#dmlcalcbuffertensorsize
    constexpr uint64_t kDMLBufferAlignment = 4ull;
    if (std::numeric_limits<uint64_t>::max() - kDMLBufferAlignment <
        buffer_width) {
      LOG(ERROR) << "Width exceeds maximum alignable size.";
      return nullptr;
    }
    buffer_width = base::bits::AlignUp(buffer_width, kDMLBufferAlignment);

    D3D12_FEATURE_DATA_ARCHITECTURE arch = {};
    if (FAILED(d3d12_device_->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE,
                                                  &arch, sizeof(arch)))) {
      LOG(ERROR) << "D3D12 device failed to check feature support.";
      return nullptr;
    }

    // If adapter supports UMA, create the custom heap with equivalent heap
    // type.
    if (arch.UMA == TRUE) {
      if (usage.Has(SHARED_IMAGE_USAGE_WEBNN_SHARED_TENSOR_WRITE)) {
        heap_properties =
            d3d12_device_->GetCustomHeapProperties(0, D3D12_HEAP_TYPE_UPLOAD);
      } else if (usage.Has(SHARED_IMAGE_USAGE_WEBNN_SHARED_TENSOR_READ)) {
        heap_properties =
            d3d12_device_->GetCustomHeapProperties(0, D3D12_HEAP_TYPE_READBACK);
      } else {
        // Default to UPLOAD heap to enable CPU read-write access. This is
        // currently required for ORT interop.
        heap_properties =
            d3d12_device_->GetCustomHeapProperties(0, D3D12_HEAP_TYPE_UPLOAD);
      }
    }
  }

  D3D12_RESOURCE_DESC desc;
  desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  desc.Alignment = 0;
  desc.Width = buffer_width;
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
  // Only usages for WebNN is allowed if D3D shared images are disabled.
  if (enable_webnn_only_d3d_factory_) {
    constexpr auto kAllowedUsages =
        gpu::SHARED_IMAGE_USAGE_WEBNN_SHARED_TENSOR |
        gpu::SHARED_IMAGE_USAGE_WEBGPU_SHARED_BUFFER |
        gpu::SHARED_IMAGE_USAGE_WEBNN_SHARED_TENSOR_READ |
        gpu::SHARED_IMAGE_USAGE_WEBNN_SHARED_TENSOR_WRITE;
    return kAllowedUsages.HasAll(usage);
  }

  if (!pixel_data.empty() && !IsFormatSupportedForInitialData(format)) {
    return false;
  }

  const bool is_scanout = usage.Has(gpu::SHARED_IMAGE_USAGE_SCANOUT);
  const bool is_video_decode = usage.Has(gpu::SHARED_IMAGE_USAGE_VIDEO_DECODE);
  const bool is_concurrent_read_write =
      usage.Has(gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE);
  const bool is_buffer =
      usage.Has(gpu::SHARED_IMAGE_USAGE_WEBGPU_SHARED_BUFFER);
  if (is_scanout) {
    if (is_video_decode || gmb_type == gfx::DXGI_SHARED_HANDLE ||
        is_concurrent_read_write) {
      // Video decode and video frames via GMBs are handled specially in
      // |SwapChainPresenter|, so we must assume it's safe to create a scanout
      // image backing for it. Concurrent read/write is handled specially by
      // creating a swapchain backing.
    } else if (gmb_type == gfx::EMPTY_BUFFER) {
      return gl::DirectCompositionTextureSupported() &&
             IsFormatSupportedForDCompTexture(ToDXGIFormat(format));
    }
  }

  // Allow WebNN as part of a buffer usage when D3D shared images are supported.
  if (is_buffer && usage.Has(gpu::SHARED_IMAGE_USAGE_WEBNN_SHARED_TENSOR)) {
    return true;
  }

  if (gmb_type == gfx::EMPTY_BUFFER) {
    if (ToDXGIFormat(format) == DXGI_FORMAT_UNKNOWN && !is_buffer) {
      return false;
    }
  } else if (gmb_type == gfx::DXGI_SHARED_HANDLE) {
    if (GetDXGIFormatForGMB(format) == DXGI_FORMAT_UNKNOWN) {
      return false;
    }
  } else {
    return false;
  }

  if (format == viz::MultiPlaneFormat::kNV12) {
    // Return early if d3d11 cannot support nv12 formats.
    if (!d3d11_supports_nv12_) {
      LOG(ERROR) << "D3D device does not support NV12 texture creation";
      return false;
    }
    // We know current size width and height must be within
    // `max_nv12_dim_supported_` as nv12 creation is supported for
    // `max_nv12_dim_supported_`.
    if (size.width() > max_nv12_dim_supported_ ||
        size.height() > max_nv12_dim_supported_) {
      LOG(ERROR)
          << "Provided size=" << size.ToString()
          << "is not supported by d3d device, with max supported dimensions="
          << max_nv12_dim_supported_;
      return false;
    }
  }

  return true;
}

SharedImageBackingType D3DImageBackingFactory::GetBackingType() {
  return SharedImageBackingType::kD3D;
}

}  // namespace gpu
