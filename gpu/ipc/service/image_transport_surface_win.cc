// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/image_transport_surface.h"

#include <memory>

#include "base/win/windows_version.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/ipc/service/pass_through_image_transport_surface.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/direct_composition_surface_win.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/vsync_provider_win.h"

namespace gpu {

// static
scoped_refptr<gl::GLSurface> ImageTransportSurface::CreateNativeSurface(
    base::WeakPtr<ImageTransportSurfaceDelegate> delegate,
    SurfaceHandle surface_handle,
    gl::GLSurfaceFormat format) {
  DCHECK_NE(surface_handle, kNullSurfaceHandle);

  scoped_refptr<gl::GLSurface> surface;
  bool override_vsync_for_multi_window_swap = false;

  if (gl::GetGLImplementation() == gl::kGLImplementationEGLANGLE) {
    auto vsync_provider =
        std::make_unique<gl::VSyncProviderWin>(surface_handle);

    if (gl::DirectCompositionSurfaceWin::IsDirectCompositionSupported()) {
      const auto& workarounds = delegate->GetFeatureInfo()->workarounds();
      gl::DirectCompositionSurfaceWin::Settings settings;
      settings.disable_nv12_dynamic_textures =
          workarounds.disable_nv12_dynamic_textures;
      settings.disable_larger_than_screen_overlays =
          workarounds.disable_larger_than_screen_overlays;
      settings.disable_vp_scaling = workarounds.disable_vp_scaling;
      auto vsync_callback = delegate->GetGpuVSyncCallback();
      auto dc_surface = base::MakeRefCounted<gl::DirectCompositionSurfaceWin>(
          std::move(vsync_provider), std::move(vsync_callback), surface_handle,
          settings);
      if (!dc_surface->Initialize(gl::GLSurfaceFormat()))
        return nullptr;
      delegate->DidCreateAcceleratedSurfaceChildWindow(surface_handle,
                                                       dc_surface->window());
      surface = std::move(dc_surface);
    } else {
      surface = gl::InitializeGLSurface(
          base::MakeRefCounted<gl::NativeViewGLSurfaceEGL>(
              surface_handle, std::move(vsync_provider)));
      if (!surface)
        return nullptr;
      // This is unnecessary with DirectComposition because that doesn't block
      // swaps, but instead blocks the first draw into a surface during the next
      // frame.
      override_vsync_for_multi_window_swap = true;
    }
  } else {
    surface = gl::init::CreateViewGLSurface(surface_handle);
    if (!surface)
      return nullptr;
  }

  return scoped_refptr<gl::GLSurface>(new PassThroughImageTransportSurface(
      delegate, surface.get(), override_vsync_for_multi_window_swap));
}

}  // namespace gpu
