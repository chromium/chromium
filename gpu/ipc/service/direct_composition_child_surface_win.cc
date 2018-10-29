// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/direct_composition_child_surface_win.h"

#include <d3d11_1.h>
#include <dcomptypes.h>

#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/trace_event/trace_event.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/egl_util.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/scoped_make_current.h"

#ifndef EGL_ANGLE_flexible_surface_compatibility
#define EGL_ANGLE_flexible_surface_compatibility 1
#define EGL_FLEXIBLE_SURFACE_COMPATIBILITY_SUPPORTED_ANGLE 0x33A6
#endif /* EGL_ANGLE_flexible_surface_compatibility */

#ifndef EGL_ANGLE_d3d_texture_client_buffer
#define EGL_ANGLE_d3d_texture_client_buffer 1
#define EGL_D3D_TEXTURE_ANGLE 0x33A3
#endif /* EGL_ANGLE_d3d_texture_client_buffer */

namespace gpu {

namespace {
// Only one DirectComposition surface can be rendered into at a time. Track
// here which IDCompositionSurface is being rendered into. If another context
// is made current, then this surface will be suspended.
IDCompositionSurface* g_current_surface;
}

DirectCompositionChildSurfaceWin::DirectCompositionChildSurfaceWin(
    const gfx::Size& size,
    bool is_hdr,
    bool has_alpha,
    bool use_dcomp_surface,
    bool allow_tearing)
    : gl::GLSurfaceEGL(),
      size_(size),
      is_hdr_(is_hdr),
      has_alpha_(has_alpha),
      use_dcomp_surface_(use_dcomp_surface),
      allow_tearing_(allow_tearing) {}

DirectCompositionChildSurfaceWin::~DirectCompositionChildSurfaceWin() {
  Destroy();
}

bool DirectCompositionChildSurfaceWin::Initialize(gl::GLSurfaceFormat format) {
  ui::ScopedReleaseCurrent release_current;
  d3d11_device_ = gl::QueryD3D11DeviceObjectFromANGLE();
  dcomp_device_ = gl::QueryDirectCompositionDevice(d3d11_device_);
  if (!dcomp_device_)
    return false;

  EGLDisplay display = GetDisplay();

  EGLint pbuffer_attribs[] = {
      EGL_WIDTH,
      1,
      EGL_HEIGHT,
      1,
      EGL_FLEXIBLE_SURFACE_COMPATIBILITY_SUPPORTED_ANGLE,
      EGL_TRUE,
      EGL_NONE,
  };

  default_surface_ =
      eglCreatePbufferSurface(display, GetConfig(), pbuffer_attribs);
  if (!default_surface_) {
    DLOG(ERROR) << "eglCreatePbufferSurface failed with error "
                << ui::GetLastEGLErrorString();
    return false;
  }

  return true;
}

bool DirectCompositionChildSurfaceWin::InitializeSurface() {
  TRACE_EVENT1("gpu", "DirectCompositionChildSurfaceWin::InitializeSurface()",
               "use_dcomp_surface_", use_dcomp_surface_);
  if (!ReleaseDrawTexture(true /* will_discard */))
    return false;
  dcomp_surface_.Reset();
  swap_chain_.Reset();

  DXGI_FORMAT output_format =
      is_hdr_ ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_B8G8R8A8_UNORM;
  if (use_dcomp_surface_) {
    // Always treat as premultiplied, because an underlay could cause it to
    // become transparent.
    HRESULT hr = dcomp_device_->CreateSurface(
        size_.width(), size_.height(), output_format,
        DXGI_ALPHA_MODE_PREMULTIPLIED, dcomp_surface_.GetAddressOf());
    has_been_rendered_to_ = false;
    if (FAILED(hr)) {
      DLOG(ERROR) << "CreateSurface failed with error " << std::hex << hr;
      return false;
    }
  } else {
    DXGI_ALPHA_MODE alpha_mode =
        has_alpha_ ? DXGI_ALPHA_MODE_PREMULTIPLIED : DXGI_ALPHA_MODE_IGNORE;
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
    d3d11_device_.CopyTo(dxgi_device.GetAddressOf());
    DCHECK(dxgi_device);
    Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
    dxgi_device->GetAdapter(dxgi_adapter.GetAddressOf());
    DCHECK(dxgi_adapter);
    Microsoft::WRL::ComPtr<IDXGIFactory2> dxgi_factory;
    dxgi_adapter->GetParent(IID_PPV_ARGS(dxgi_factory.GetAddressOf()));
    DCHECK(dxgi_factory);

    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width = size_.width();
    desc.Height = size_.height();
    desc.Format = output_format;
    desc.Stereo = FALSE;
    desc.SampleDesc.Count = 1;
    desc.BufferCount = 2;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.AlphaMode = alpha_mode;
    desc.Flags = allow_tearing_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
    HRESULT hr = dxgi_factory->CreateSwapChainForComposition(
        d3d11_device_.Get(), &desc, nullptr, swap_chain_.GetAddressOf());
    has_been_rendered_to_ = false;
    first_swap_ = true;
    if (FAILED(hr)) {
      DLOG(ERROR) << "CreateSwapChainForComposition failed with error "
                  << std::hex << hr;
      return false;
    }
  }
  return true;
}

bool DirectCompositionChildSurfaceWin::ReleaseDrawTexture(bool will_discard) {
  DCHECK(!gl::GLContext::GetCurrent());
  if (real_surface_) {
    eglDestroySurface(GetDisplay(), real_surface_);
    real_surface_ = nullptr;
  }

  if (dcomp_surface_.Get() == g_current_surface)
    g_current_surface = nullptr;

  if (draw_texture_) {
    draw_texture_.Reset();
    if (dcomp_surface_) {
      HRESULT hr = dcomp_surface_->EndDraw();
      if (FAILED(hr)) {
        DLOG(ERROR) << "EndDraw failed with error " << std::hex << hr;
        return false;
      }
      dcomp_surface_serial_++;
    } else if (!will_discard) {
      UINT interval = first_swap_ || !vsync_enabled_ || allow_tearing_ ? 0 : 1;
      UINT flags = allow_tearing_ ? DXGI_PRESENT_ALLOW_TEARING : 0;
      DXGI_PRESENT_PARAMETERS params = {};
      RECT dirty_rect = swap_rect_.ToRECT();
      params.DirtyRectsCount = 1;
      params.pDirtyRects = &dirty_rect;
      HRESULT hr = swap_chain_->Present1(interval, flags, &params);
      if (FAILED(hr)) {
        DLOG(ERROR) << "Present1 failed with error " << std::hex << hr;
        return false;
      }
      if (first_swap_) {
        // Wait for the GPU to finish executing its commands before
        // committing the DirectComposition tree, or else the swapchain
        // may flicker black when it's first presented.
        first_swap_ = false;
        Microsoft::WRL::ComPtr<IDXGIDevice2> dxgi_device2;
        d3d11_device_.CopyTo(dxgi_device2.GetAddressOf());
        DCHECK(dxgi_device2);
        base::WaitableEvent event(
            base::WaitableEvent::ResetPolicy::AUTOMATIC,
            base::WaitableEvent::InitialState::NOT_SIGNALED);
        hr = dxgi_device2->EnqueueSetEvent(event.handle());
        DCHECK(SUCCEEDED(hr));
        event.Wait();
      }
    }
  }
  return true;
}

void DirectCompositionChildSurfaceWin::Destroy() {
  if (default_surface_) {
    if (!eglDestroySurface(GetDisplay(), default_surface_)) {
      DLOG(ERROR) << "eglDestroySurface failed with error "
                  << ui::GetLastEGLErrorString();
    }
    default_surface_ = nullptr;
  }
  if (real_surface_) {
    if (!eglDestroySurface(GetDisplay(), real_surface_)) {
      DLOG(ERROR) << "eglDestroySurface failed with error "
                  << ui::GetLastEGLErrorString();
    }
    real_surface_ = nullptr;
  }
  if (dcomp_surface_ && (dcomp_surface_.Get() == g_current_surface)) {
    HRESULT hr = dcomp_surface_->EndDraw();
    if (FAILED(hr))
      DLOG(ERROR) << "EndDraw failed with error " << std::hex << hr;
    g_current_surface = nullptr;
  }
  draw_texture_.Reset();
  dcomp_surface_.Reset();
}

gfx::Size DirectCompositionChildSurfaceWin::GetSize() {
  return size_;
}

bool DirectCompositionChildSurfaceWin::IsOffscreen() {
  return false;
}

void* DirectCompositionChildSurfaceWin::GetHandle() {
  return real_surface_ ? real_surface_ : default_surface_;
}

gfx::SwapResult DirectCompositionChildSurfaceWin::SwapBuffers(
    const PresentationCallback& callback) {
  // PresentationCallback is handled by DirectCompositionSurfaceWin. The child
  // surface doesn't need provide presentation feedback.
  DCHECK(!callback);
  ui::ScopedReleaseCurrent release_current;
  if (!ReleaseDrawTexture(false /* will_discard */))
    return gfx::SwapResult::SWAP_FAILED;
  return gfx::SwapResult::SWAP_ACK;
}

bool DirectCompositionChildSurfaceWin::FlipsVertically() const {
  return true;
}

bool DirectCompositionChildSurfaceWin::SupportsPostSubBuffer() {
  return true;
}

bool DirectCompositionChildSurfaceWin::OnMakeCurrent(gl::GLContext* context) {
  if (g_current_surface != dcomp_surface_.Get()) {
    if (g_current_surface) {
      HRESULT hr = g_current_surface->SuspendDraw();
      if (FAILED(hr)) {
        DLOG(ERROR) << "SuspendDraw failed with error " << std::hex << hr;
        return false;
      }
      g_current_surface = nullptr;
    }
    if (draw_texture_) {
      HRESULT hr = dcomp_surface_->ResumeDraw();
      if (FAILED(hr)) {
        DLOG(ERROR) << "ResumeDraw failed with error " << std::hex << hr;
        return false;
      }
      g_current_surface = dcomp_surface_.Get();
    }
  }
  return true;
}

bool DirectCompositionChildSurfaceWin::SupportsDCLayers() const {
  return true;
}

bool DirectCompositionChildSurfaceWin::SetDrawRectangle(
    const gfx::Rect& rectangle) {
  if (!gfx::Rect(size_).Contains(rectangle)) {
    DLOG(ERROR) << "Draw rectangle must be contained within size of surface";
    return false;
  }

  if (draw_texture_) {
    DLOG(ERROR) << "SetDrawRectangle must be called only once per swap buffers";
    return false;
  }

  DCHECK(!real_surface_);

  ui::ScopedReleaseCurrent release_current;

  if ((use_dcomp_surface_ && !dcomp_surface_) ||
      (!use_dcomp_surface_ && !swap_chain_)) {
    if (!InitializeSurface()) {
      DLOG(ERROR) << "InitializeSurface failed";
      return false;
    }
  }

  // Check this after reinitializing the surface because we reset state there.
  if (gfx::Rect(size_) != rectangle && !has_been_rendered_to_) {
    DLOG(ERROR) << "First draw to surface must draw to everything";
    return false;
  }

  DCHECK(!g_current_surface);

  swap_rect_ = rectangle;
  draw_offset_ = gfx::Vector2d();

  if (dcomp_surface_) {
    POINT update_offset;
    const RECT rect = rectangle.ToRECT();
    HRESULT hr = dcomp_surface_->BeginDraw(
        &rect, IID_PPV_ARGS(draw_texture_.GetAddressOf()), &update_offset);
    if (FAILED(hr)) {
      DLOG(ERROR) << "BeginDraw failed with error " << std::hex << hr;
      return false;
    }
    draw_offset_ = gfx::Point(update_offset) - rectangle.origin();
  } else {
    swap_chain_->GetBuffer(0, IID_PPV_ARGS(draw_texture_.GetAddressOf()));
  }
  DCHECK(draw_texture_);
  has_been_rendered_to_ = true;

  g_current_surface = dcomp_surface_.Get();

  EGLint pbuffer_attribs[] = {
      EGL_WIDTH,
      size_.width(),
      EGL_HEIGHT,
      size_.height(),
      EGL_FLEXIBLE_SURFACE_COMPATIBILITY_SUPPORTED_ANGLE,
      EGL_TRUE,
      EGL_NONE,
  };

  EGLClientBuffer buffer =
      reinterpret_cast<EGLClientBuffer>(draw_texture_.Get());
  real_surface_ =
      eglCreatePbufferFromClientBuffer(GetDisplay(), EGL_D3D_TEXTURE_ANGLE,
                                       buffer, GetConfig(), pbuffer_attribs);
  if (!real_surface_) {
    DLOG(ERROR) << "eglCreatePbufferFromClientBuffer failed with error "
                << ui::GetLastEGLErrorString();
    return false;
  }

  return true;
}

gfx::Vector2d DirectCompositionChildSurfaceWin::GetDrawOffset() const {
  return draw_offset_;
}

void DirectCompositionChildSurfaceWin::SetVSyncEnabled(bool enabled) {
  vsync_enabled_ = enabled;
}

}  // namespace gpu
