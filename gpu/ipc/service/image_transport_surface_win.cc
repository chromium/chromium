// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/image_transport_surface.h"

#include <memory>

#include "base/win/windows_version.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/dcomp_presenter.h"
#include "ui/gl/direct_composition_support.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/vsync_provider_win.h"

namespace gpu {
namespace {
gl::DCompPresenter::Settings CreatDCompPresenterSettings(
    const GpuDriverBugWorkarounds& workarounds) {
  gl::DCompPresenter::Settings settings;
  settings.no_downscaled_overlay_promotion =
      workarounds.no_downscaled_overlay_promotion;
  settings.disable_nv12_dynamic_textures =
      workarounds.disable_nv12_dynamic_textures;
  settings.disable_vp_auto_hdr = workarounds.disable_vp_auto_hdr;
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
    const GpuDriverBugWorkarounds& workarounds,
    const GpuFeatureInfo& gpu_feature_info,
    SurfaceHandle surface_handle) {
  if (gl::DirectCompositionSupported()) {
    auto settings = CreatDCompPresenterSettings(workarounds);
    auto presenter = base::MakeRefCounted<gl::DCompPresenter>(settings);
    if (!presenter->Initialize()) {
      return nullptr;
    }
    return presenter;
  }

  return nullptr;
}

// static
scoped_refptr<gl::GLSurface> ImageTransportSurface::CreateNativeGLSurface(
    gl::GLDisplay* display,
    SurfaceHandle surface_handle,
    gl::GLSurfaceFormat format) {
  DCHECK_NE(surface_handle, kNullSurfaceHandle);
  scoped_refptr<gl::GLSurface> surface;

  if (gl::GetGLImplementation() == gl::kGLImplementationEGLANGLE) {
    CHECK(!gl::DirectCompositionSupported());
    surface = gl::InitializeGLSurface(
        base::MakeRefCounted<gl::NativeViewGLSurfaceEGL>(
            display->GetAs<gl::GLDisplayEGL>(), surface_handle,
            std::make_unique<gl::VSyncProviderWin>(surface_handle)));
    if (!surface) {
      return nullptr;
    }
  } else {
    surface = gl::init::CreateViewGLSurface(display, surface_handle);
    if (!surface)
      return nullptr;
  }

  return surface;
}

}  // namespace gpu
