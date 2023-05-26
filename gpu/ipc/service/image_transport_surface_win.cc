// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/image_transport_surface.h"

#include <memory>

#include "base/win/windows_version.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/ipc/service/pass_through_image_transport_surface.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/dcomp_presenter.h"
#include "ui/gl/direct_composition_support.h"
#include "ui/gl/direct_composition_surface_win.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/vsync_provider_win.h"

namespace gpu {
namespace {
gl::DirectCompositionSurfaceWin::Settings
CreateDirectCompositionSurfaceSettings(
    const GpuDriverBugWorkarounds& workarounds) {
  gl::DirectCompositionSurfaceWin::Settings settings;
  settings.no_downscaled_overlay_promotion =
      workarounds.no_downscaled_overlay_promotion;
  settings.disable_nv12_dynamic_textures =
      workarounds.disable_nv12_dynamic_textures;
  settings.disable_vp_scaling = workarounds.disable_vp_scaling;
  settings.disable_vp_super_resolution =
      workarounds.disable_vp_super_resolution;
  settings.force_dcomp_triple_buffer_video_swap_chain =
      workarounds.force_dcomp_triple_buffer_video_swap_chain;
  settings.use_angle_texture_offset = true;
  return settings;
}
}  // namespace

// static
scoped_refptr<gl::Presenter> ImageTransportSurface::CreatePresenter(
    gl::GLDisplay* display,
    base::WeakPtr<ImageTransportSurfaceDelegate> delegate,
    SurfaceHandle surface_handle,
    gl::GLSurfaceFormat format) {
  if (gl::DirectCompositionSupported() &&
      base::FeatureList::IsEnabled(features::kDCompPresenter)) {
    auto vsync_callback = delegate->GetGpuVSyncCallback();
    auto settings = CreateDirectCompositionSurfaceSettings(
        delegate->GetFeatureInfo()->workarounds());
    auto presenter = base::MakeRefCounted<gl::DCompPresenter>(
        display->GetAs<gl::GLDisplayEGL>(), std::move(vsync_callback),
        settings);
    if (!presenter->Initialize()) {
      return nullptr;
    }

    delegate->AddChildWindowToBrowser(presenter->window());
    return presenter;
  }

  return nullptr;
}

// static
scoped_refptr<gl::GLSurface> ImageTransportSurface::CreateNativeGLSurface(
    gl::GLDisplay* display,
    base::WeakPtr<ImageTransportSurfaceDelegate> delegate,
    SurfaceHandle surface_handle,
    gl::GLSurfaceFormat format) {
  DCHECK_NE(surface_handle, kNullSurfaceHandle);
  scoped_refptr<gl::GLSurface> surface;

  if (gl::GetGLImplementation() == gl::kGLImplementationEGLANGLE) {
    if (gl::DirectCompositionSupported()) {
      auto vsync_callback = delegate->GetGpuVSyncCallback();
      auto settings = CreateDirectCompositionSurfaceSettings(
          delegate->GetFeatureInfo()->workarounds());
      auto dc_surface = base::MakeRefCounted<gl::DirectCompositionSurfaceWin>(
          display->GetAs<gl::GLDisplayEGL>(), std::move(vsync_callback),
          settings);
      if (!dc_surface->Initialize(gl::GLSurfaceFormat()))
        return nullptr;

      delegate->AddChildWindowToBrowser(dc_surface->window());
      surface = std::move(dc_surface);
    } else {
      surface = gl::InitializeGLSurface(
          base::MakeRefCounted<gl::NativeViewGLSurfaceEGL>(
              display->GetAs<gl::GLDisplayEGL>(), surface_handle,
              std::make_unique<gl::VSyncProviderWin>(surface_handle)));
      if (!surface)
        return nullptr;
    }
  } else {
    surface = gl::init::CreateViewGLSurface(display, surface_handle);
    if (!surface)
      return nullptr;
  }

  // |override_vsync_for_multi_window_swap| is needed because Present() blocks
  // when multiple windows use swap interval 1 all the time.  With this flag the
  // surface forces swap interval 0 when multiple windows are presenting.
  return scoped_refptr<gl::GLSurface>(new PassThroughImageTransportSurface(
      delegate, surface.get(), /*override_vsync_for_multi_window_swap=*/true));
}

}  // namespace gpu
