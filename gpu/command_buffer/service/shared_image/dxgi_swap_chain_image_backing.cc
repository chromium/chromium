// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/dxgi_swap_chain_image_backing.h"

#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/d3d_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/d3d_image_utils.h"
#include "gpu/command_buffer/service/shared_image/dxgi_swap_chain_image_representation.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/skia_gl_image_representation.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/color_space_win.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/direct_composition_support.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_utils.h"

#if BUILDFLAG(SKIA_USE_DAWN)
#include "gpu/command_buffer/service/dawn_context_provider.h"
#include "gpu/command_buffer/service/shared_image/skia_graphite_dawn_image_representation.h"
#endif

namespace gpu {

namespace {
const char* kDXGISwapChainImageBackingLabel = "DXGISwapChainImageBacking";
}  // namespace

// static
std::unique_ptr<DXGISwapChainImageBacking> DXGISwapChainImageBacking::Create(
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    DXGI_FORMAT internal_format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    gpu::SharedImageUsageSet usage,
    std::string debug_label) {
  if (!d3d11_device) {
    return nullptr;
  }

  Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
  d3d11_device.As(&dxgi_device);
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
  desc.Format = internal_format;
  desc.Stereo = FALSE;
  desc.SampleDesc.Count = 1;
  desc.BufferCount = gl::DirectCompositionRootSurfaceBufferCount();
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT |
                     /* Needed to bind to GL texture */ DXGI_USAGE_SHADER_INPUT;
  desc.Scaling = DXGI_SCALING_STRETCH;
  desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
  desc.AlphaMode = SkAlphaTypeIsOpaque(alpha_type)
                       ? DXGI_ALPHA_MODE_IGNORE
                       : DXGI_ALPHA_MODE_PREMULTIPLIED;
  desc.Flags = 0;
  if (gl::DirectCompositionSwapChainTearingEnabled()) {
    desc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
  }
  if (gl::DXGIWaitableSwapChainEnabled()) {
    desc.Flags |= DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
  }

  Microsoft::WRL::ComPtr<IDXGISwapChain1> dxgi_swap_chain;
  HRESULT hr = dxgi_factory->CreateSwapChainForComposition(
      d3d11_device.Get(), &desc, nullptr, &dxgi_swap_chain);

  // If CreateSwapChainForComposition fails, we cannot draw to the
  // browser window. Return false after disabling Direct Composition support
  // and let the Renderer handle it. Either the GPU command buffer or the GPU
  // process will be restarted.
  if (FAILED(hr)) {
    DLOG(ERROR) << "CreateSwapChainForComposition failed: "
                << logging::SystemErrorCodeToString(hr);
    // Disable direct composition because SwapChain creation might fail again
    // next time.
    gl::SetDirectCompositionSwapChainFailed();
    return nullptr;
  }

  gl::LabelSwapChainAndBuffers(dxgi_swap_chain.Get(),
                               kDXGISwapChainImageBackingLabel);

  Microsoft::WRL::ComPtr<IDXGISwapChain3> swap_chain_3;
  if (SUCCEEDED(dxgi_swap_chain.As(&swap_chain_3))) {
    hr = swap_chain_3->SetColorSpace1(
        gfx::ColorSpaceWin::GetDXGIColorSpace(color_space));
    DCHECK_EQ(hr, S_OK) << ", SetColorSpace1 failed: "
                        << logging::SystemErrorCodeToString(hr);
    if (gl::DXGIWaitableSwapChainEnabled()) {
      hr = swap_chain_3->SetMaximumFrameLatency(
          gl::GetDXGIWaitableSwapChainMaxQueuedFrames());
      DCHECK_EQ(hr, S_OK) << ", SetMaximumFrameLatency failed: "
                          << logging::SystemErrorCodeToString(hr);
    }
  }

  // When |format| has no alpha (e.g. RGBX) but |internal_format| does, we wrap
  // the back buffer in ANGLE as |GL_RGB|. To ensure shaders that sample from
  // it see an opaque color, it needs to have 1.f in the alpha channel.
  // When |format| has alpha, we can rely on DirectRenderer to ensure all pixels
  // are initialized before use.
  int buffers_need_alpha_initialization_count =
      !format.HasAlpha() ? desc.BufferCount : 0;

  return base::WrapUnique(new DXGISwapChainImageBacking(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(debug_label), std::move(d3d11_device),
      std::move(dxgi_swap_chain), buffers_need_alpha_initialization_count));
}

DXGISwapChainImageBacking::DXGISwapChainImageBacking(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    gpu::SharedImageUsageSet usage,
    std::string debug_label,
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
    Microsoft::WRL::ComPtr<IDXGISwapChain1> dxgi_swap_chain,
    int buffers_need_alpha_initialization_count)
    : ClearTrackingSharedImageBacking(
          mailbox,
          format,
          size,
          color_space,
          surface_origin,
          alpha_type,
          usage,
          std::move(debug_label),
          gfx::BufferSizeForBufferFormat(size, ToBufferFormat(format)),
          /*is_thread_safe=*/false),
      d3d11_device_(std::move(d3d11_device)),
      dxgi_swap_chain_(std::move(dxgi_swap_chain)),
      buffers_need_alpha_initialization_count_(
          buffers_need_alpha_initialization_count) {
  const bool has_scanout = usage.Has(SHARED_IMAGE_USAGE_SCANOUT);
  const bool has_write = usage.Has(SHARED_IMAGE_USAGE_DISPLAY_WRITE);
  DCHECK(has_scanout);
  DCHECK(has_write);
}

DXGISwapChainImageBacking::~DXGISwapChainImageBacking() = default;

SharedImageBackingType DXGISwapChainImageBacking::GetType() const {
  return SharedImageBackingType::kDXGISwapChain;
}

void DXGISwapChainImageBacking::Update(
    std::unique_ptr<gfx::GpuFence> in_fence) {
  DCHECK(!in_fence);
}

bool DXGISwapChainImageBacking::DidBeginWriteAccess(
    const gfx::Rect& swap_rect) {
  if (pending_swap_rect_.has_value()) {
    // Force a Present if there's already a pending swap rect. For normal usage
    // of this backing, we normally expect one Skia write access to overlay read
    // access.
    // We'll log a message so it appears in the GPU log in case it aids in
    // debugging.
    LOG(WARNING) << "Multiple skia write accesses per overlay access, flushing "
                    "pending swap.";
    if (!Present(false)) {
      return false;
    }
  }

  gfx::Rect pending_swap_rect = swap_rect;

  std::optional<SkColor4f> initialize_color;

  // SharedImage allows an incomplete first draw so long as we only read from
  // the part that we've previously drawn to. However, IDXGISwapChain requires a
  // full draw on the first |Present1|. To make an incomplete first draw valid,
  // we'll initialize all the pixels and expand the swap rect.
  const gfx::Rect full_swap_rect = gfx::Rect(size());
  if (!IsCleared() && swap_rect != full_swap_rect) {
#if DCHECK_IS_ON()
    initialize_color = SkColors::kBlue;
#else
    initialize_color = SkColors::kTransparent;
#endif

    // Ensure that the next swap contains the entire swap chain since we just
    // cleared it.
    pending_swap_rect = full_swap_rect;
  }

  // See comment in |DXGISwapChainImageBacking::Create| for why we need this.
  if (buffers_need_alpha_initialization_count_ > 0) {
    // We only need to write the alpha channel, but we clear since it's simpler
    // and are guaranteed to not have pixels we need to preserve before the
    // first write to each buffer.
#if DCHECK_IS_ON()
    initialize_color = SkColors::kBlue;
#else
    initialize_color = SkColors::kBlack;
#endif

    // We don't need to modify the swap rect in this case since |Present1| will
    // copy the contents outside the swap rect from the previous buffer and
    // we've already forced a full swap on the first buffer above.
  }

  if (initialize_color.has_value()) {
    // To clear only uninitialized buffers, this must happen after |Present|s of
    // outstanding draws, including the one above.
    if (!D3DImageBackingFactory::ClearBackBufferToColor(
            dxgi_swap_chain_.Get(), initialize_color.value())) {
      LOG(ERROR) << "Could not initialize back buffer alpha";
      return false;
    }

    if (buffers_need_alpha_initialization_count_ > 0) {
      buffers_need_alpha_initialization_count_--;
    }
  }

  pending_swap_rect_ = {pending_swap_rect};

  return true;
}

bool DXGISwapChainImageBacking::Present(
    bool should_synchronize_present_with_vblank) {
  if (!pending_swap_rect_.has_value() || pending_swap_rect_.value().IsEmpty()) {
    DVLOG(1) << "Skipping present without an update rect";
    return true;
  }

  HRESULT hr, device_removed_reason;
  const bool use_swap_chain_tearing =
      gl::DirectCompositionSwapChainTearingEnabled();
  const bool force_present_interval_0 =
      base::FeatureList::IsEnabled(features::kDXGISwapChainPresentInterval0);
  UINT interval = first_swap_ || !should_synchronize_present_with_vblank ||
                          use_swap_chain_tearing || force_present_interval_0
                      ? 0
                      : 1;
  UINT flags = use_swap_chain_tearing ? DXGI_PRESENT_ALLOW_TEARING : 0;

  TRACE_EVENT2("gpu", "IDXGISwapChain1::Present1", "has_alpha",
               !SkAlphaTypeIsOpaque(alpha_type()), "dirty_rect",
               pending_swap_rect_->ToString());
  DXGI_PRESENT_PARAMETERS params = {};
  RECT dirty_rect = pending_swap_rect_.value().ToRECT();
  params.DirtyRectsCount = 1;
  params.pDirtyRects = &dirty_rect;
  hr = dxgi_swap_chain_->Present1(interval, flags, &params);

  // Ignore DXGI_STATUS_OCCLUDED since that's not an error but only
  // indicates that the window is occluded and we can stop rendering.
  if (FAILED(hr) && hr != DXGI_STATUS_OCCLUDED) {
    LOG(ERROR) << "Present1 failed: " << logging::SystemErrorCodeToString(hr);
    return false;
  }

  if (first_swap_) {
    // Wait for the GPU to finish executing its commands before
    // committing the DirectComposition tree, or else the swapchain
    // may flicker black when it's first presented.
    first_swap_ = false;
    Microsoft::WRL::ComPtr<IDXGIDevice2> dxgi_device2;
    d3d11_device_.As(&dxgi_device2);
    DCHECK(dxgi_device2);
    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    hr = dxgi_device2->EnqueueSetEvent(event.handle());
    if (SUCCEEDED(hr)) {
      event.Wait();
    } else {
      device_removed_reason = d3d11_device_->GetDeviceRemovedReason();
      base::debug::Alias(&hr);
      base::debug::Alias(&device_removed_reason);
      base::debug::DumpWithoutCrashing();
    }
  }

  pending_swap_rect_.reset();

  return true;
}

std::unique_ptr<OverlayImageRepresentation>
DXGISwapChainImageBacking::ProduceOverlay(SharedImageManager* manager,
                                          MemoryTypeTracker* tracker) {
  return std::make_unique<DXGISwapChainOverlayImageRepresentation>(
      manager, this, tracker);
}

std::unique_ptr<SkiaGaneshImageRepresentation>
DXGISwapChainImageBacking::ProduceSkiaGanesh(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  TRACE_EVENT0("gpu", "DXGISwapChainImageBacking::ProduceSkiaGanesh");

  if (!gl_texture_holder_) {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backbuffer_texture;
    HRESULT hr =
        dxgi_swap_chain_->GetBuffer(0, IID_PPV_ARGS(&backbuffer_texture));
    if (FAILED(hr)) {
      DLOG(ERROR) << "GetBuffer(0) failed: "
                  << logging::SystemErrorCodeToString(hr);
      return nullptr;
    }

    auto gl_format_desc = context_state->GetGLFormatCaps().ToGLFormatDesc(
        format(), /*plane_index=*/0);
    gl_texture_holder_ = D3DImageBacking::CreateGLTexture(
        gl_format_desc, size(), color_space(), backbuffer_texture,
        GL_TEXTURE_2D, /*array_slice=*/0, /*plane_index=*/0, dxgi_swap_chain_);
    if (!gl_texture_holder_) {
      LOG(ERROR) << "Failed to create GL texture.";
      return nullptr;
    }
  }

  return SkiaGLImageRepresentationDXGISwapChain::Create(
      std::make_unique<GLTexturePassthroughDXGISwapChainBufferRepresentation>(
          manager, this, tracker, gl_texture_holder_),
      std::move(context_state), manager, this, tracker);
}

std::unique_ptr<SkiaGraphiteImageRepresentation>
DXGISwapChainImageBacking::ProduceSkiaGraphite(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
#if BUILDFLAG(SKIA_USE_DAWN)
  DCHECK_EQ(context_state->gr_context_type(), GrContextType::kGraphiteDawn);

  auto device = context_state->dawn_context_provider()->GetDevice();
  if (!shared_texture_memory_) {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backbuffer_texture;
    HRESULT hr =
        dxgi_swap_chain_->GetBuffer(0, IID_PPV_ARGS(&backbuffer_texture));
    if (FAILED(hr)) {
      DLOG(ERROR) << "GetBuffer(0) failed: "
                  << logging::SystemErrorCodeToString(hr);
      return nullptr;
    }

    shared_texture_memory_ =
        CreateDawnSharedTextureMemory(device, backbuffer_texture);
    if (!shared_texture_memory_) {
      LOG(ERROR) << "Failed to create shared texture memory.";
      return nullptr;
    }
  }

  auto dawn_representation = std::make_unique<DawnRepresentationDXGISwapChain>(
      manager, this, tracker, device, wgpu::BackendType::D3D11);

  return SkiaGraphiteDawnImageRepresentation::Create(
      std::move(dawn_representation), context_state,
      context_state->gpu_main_graphite_recorder(), manager, this, tracker);
#else
  NOTREACHED();
#endif  // BUILDFLAG(SKIA_USE_DAWN)
}

wgpu::Texture DXGISwapChainImageBacking::BeginAccessDawn(
    const wgpu::Device& device,
    wgpu::TextureUsage usage,
    wgpu::TextureUsage internal_usage,
    const gfx::Rect& update_rect) {
  DidBeginWriteAccess(update_rect);

  CHECK(shared_texture_memory_);
  wgpu::SharedTextureMemoryD3DSwapchainBeginState swapchain_begin_state = {};
  swapchain_begin_state.isSwapchain = true;

  wgpu::SharedTextureMemoryBeginAccessDescriptor desc = {};
  desc.initialized = true;
  desc.nextInChain = &swapchain_begin_state;

  wgpu::Texture texture =
      CreateDawnSharedTexture(shared_texture_memory_, usage, internal_usage,
                              /*view_formats=*/{});
  if (!texture || !shared_texture_memory_.BeginAccess(texture, &desc)) {
    LOG(ERROR) << "Failed to begin access and produce WGPUTexture";
    return nullptr;
  }
  return texture;
}

void DXGISwapChainImageBacking::EndAccessDawn(const wgpu::Device& device,
                                              wgpu::Texture texture) {
  wgpu::SharedTextureMemoryEndAccessState end_state = {};
  shared_texture_memory_.EndAccess(texture.Get(), &end_state);
  texture.Destroy();
}

}  // namespace gpu
