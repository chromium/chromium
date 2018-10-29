// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_DIRECT_COMPOSITION_CHILD_SURFACE_WIN_H_
#define GPU_IPC_SERVICE_DIRECT_COMPOSITION_CHILD_SURFACE_WIN_H_

#include <windows.h>
#include <d3d11.h>
#include <dcomp.h>
#include <wrl/client.h>

#include "gpu/ipc/service/gpu_ipc_service_export.h"
#include "ui/gl/gl_surface_egl.h"

namespace gpu {

class GPU_IPC_SERVICE_EXPORT DirectCompositionChildSurfaceWin
    : public gl::GLSurfaceEGL {
 public:
  DirectCompositionChildSurfaceWin(const gfx::Size& size,
                                   bool is_hdr,
                                   bool has_alpha,
                                   bool use_dcomp_surface,
                                   bool allow_tearing);

  // GLSurfaceEGL implementation.
  using GLSurface::Initialize;
  bool Initialize(gl::GLSurfaceFormat format) override;
  void Destroy() override;
  gfx::Size GetSize() override;
  bool IsOffscreen() override;
  void* GetHandle() override;
  gfx::SwapResult SwapBuffers(const PresentationCallback& callback) override;
  bool FlipsVertically() const override;
  bool SupportsPostSubBuffer() override;
  bool OnMakeCurrent(gl::GLContext* context) override;
  bool SupportsDCLayers() const override;
  bool SetDrawRectangle(const gfx::Rect& rect) override;
  gfx::Vector2d GetDrawOffset() const override;
  void SetVSyncEnabled(bool enabled) override;

  const Microsoft::WRL::ComPtr<IDCompositionSurface>& dcomp_surface() const {
    return dcomp_surface_;
  }

  const Microsoft::WRL::ComPtr<IDXGISwapChain1>& swap_chain() const {
    return swap_chain_;
  }

  uint64_t dcomp_surface_serial() const { return dcomp_surface_serial_; }

 protected:
  ~DirectCompositionChildSurfaceWin() override;

 private:
  // Releases previous surface or swap chain, and initializes new surface or
  // swap chain.
  bool InitializeSurface();
  // Release the texture that's currently being drawn to. If will_discard is
  // true then the surface should be discarded without swapping any contents
  // to it. Returns false if this fails.
  bool ReleaseDrawTexture(bool will_discard);

  // This is a placeholder surface used when not rendering to the
  // DirectComposition surface.
  EGLSurface default_surface_ = 0;

  // This is the real surface representing the backbuffer. It may be null
  // outside of a BeginDraw/EndDraw pair.
  EGLSurface real_surface_ = 0;
  bool first_swap_ = true;
  const gfx::Size size_;
  const bool is_hdr_;
  const bool has_alpha_;
  const bool use_dcomp_surface_;
  const bool allow_tearing_;
  gfx::Rect swap_rect_;
  gfx::Vector2d draw_offset_;
  bool vsync_enabled_ = true;

  // This is a number that increments once for every EndDraw on a surface, and
  // is used to determine when the contents have changed so Commit() needs to
  // be called on the device.
  uint64_t dcomp_surface_serial_ = 0;

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;
  Microsoft::WRL::ComPtr<IDCompositionDevice2> dcomp_device_;
  Microsoft::WRL::ComPtr<IDCompositionSurface> dcomp_surface_;
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain_;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> draw_texture_;

  // Keep track of whether the texture has been rendered to, as the first draw
  // to it must overwrite the entire thing.
  bool has_been_rendered_to_ = false;

  DISALLOW_COPY_AND_ASSIGN(DirectCompositionChildSurfaceWin);
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_DIRECT_COMPOSITION_CHILD_SURFACE_WIN_H_
