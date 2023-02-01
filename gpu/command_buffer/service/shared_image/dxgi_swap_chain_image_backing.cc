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
#include "gpu/command_buffer/service/shared_image/d3d_image_backing.h"
#include "gpu/command_buffer/service/shared_image/dxgi_swap_chain_image_representation.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/skia_gl_image_representation.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/color_space_win.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/direct_composition_support.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_utils.h"

namespace gpu {
namespace {
const char* kDXGISwapChainImageBackingLabel = "DXGISwapChainImageBacking";
}  // namespace

// static
std::unique_ptr<DXGISwapChainImageBacking> DXGISwapChainImageBacking::Create(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    DXGI_FORMAT internal_format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage) {
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      gl::QueryD3D11DeviceObjectFromANGLE();

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

  base::UmaHistogramSparse(
      "GPU.DirectComposition.CreateSwapChainForComposition", hr);

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

  return base::WrapUnique(new DXGISwapChainImageBacking(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(d3d11_device), std::move(dxgi_swap_chain)));
}

DXGISwapChainImageBacking::DXGISwapChainImageBacking(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
    Microsoft::WRL::ComPtr<IDXGISwapChain1> dxgi_swap_chain)
    : ClearTrackingSharedImageBacking(
          mailbox,
          format,
          size,
          color_space,
          surface_origin,
          alpha_type,
          usage,
          gfx::BufferSizeForBufferFormat(size, ToBufferFormat(format)),
          false /* is_thread_safe */),
      d3d11_device_(std::move(d3d11_device)),
      dxgi_swap_chain_(std::move(dxgi_swap_chain)) {
  const bool has_scanout = !!(usage & SHARED_IMAGE_USAGE_SCANOUT);
  const bool has_write = !!(usage & SHARED_IMAGE_USAGE_DISPLAY_WRITE);
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

void DXGISwapChainImageBacking::AddSwapRect(const gfx::Rect& swap_rect) {
  if (pending_swap_rect_.has_value()) {
    // Force a Present if there's already a pending swap rect. For normal usage
    // of this backing, we normally expect one Skia write access to overlay read
    // access.
    // We'll log a message so it appears in the GPU log in case it aids in
    // debugging.
    LOG(WARNING) << "Multiple skia write accesses per overlay access, flushing "
                    "pending swap.";
    Present(false);
  }

  pending_swap_rect_ = swap_rect;
}

bool DXGISwapChainImageBacking::Present(
    bool should_synchronize_present_with_vblank) {
  if (!pending_swap_rect_.has_value()) {
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

  TRACE_EVENT2("gpu", "DirectCompositionChildSurfaceWin::PresentSwapChain",
               "has_alpha", !SkAlphaTypeIsOpaque(alpha_type()), "dirty_rect",
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

std::unique_ptr<SkiaImageRepresentation> DXGISwapChainImageBacking::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  TRACE_EVENT0("gpu", "DXGISwapChainImageBacking::ProduceSkia");

  if (!gl_texture_) {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backbuffer_texture;
    HRESULT hr =
        dxgi_swap_chain_->GetBuffer(0, IID_PPV_ARGS(&backbuffer_texture));
    if (FAILED(hr)) {
      DLOG(ERROR) << "GetBuffer(0) failed: "
                  << logging::SystemErrorCodeToString(hr);
      return nullptr;
    }

    gl_texture_ = D3DImageBacking::CreateGLTexture(
        format(), size(), color_space(), backbuffer_texture, GL_TEXTURE_2D, 0,
        0, dxgi_swap_chain_);
    if (!gl_texture_) {
      LOG(ERROR) << "Failed to create GL texture";
      return nullptr;
    }
  }

  return SkiaGLImageRepresentationDXGISwapChain::Create(
      std::make_unique<GLTexturePassthroughDXGISwapChainBufferRepresentation>(
          manager, this, tracker, gl_texture_),
      std::move(context_state), manager, this, tracker);
}

}  // namespace gpu
