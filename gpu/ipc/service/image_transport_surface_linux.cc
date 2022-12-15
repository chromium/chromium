// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/image_transport_surface.h"

#include "build/build_config.h"
#include "gpu/ipc/service/pass_through_image_transport_surface.h"
#include "ui/gl/init/gl_factory.h"

namespace gpu {

// static
scoped_refptr<gl::Presenter> ImageTransportSurface::CreatePresenter(
    gl::GLDisplay* display,
    base::WeakPtr<ImageTransportSurfaceDelegate> delegate,
    SurfaceHandle surface_handle,
    gl::GLSurfaceFormat format) {
  DCHECK_NE(surface_handle, kNullSurfaceHandle);
#if BUILDFLAG(IS_OZONE)
  return gl::init::CreateSurfacelessViewGLSurface(display, surface_handle);
#else
  return nullptr;
#endif
}

// static
scoped_refptr<gl::GLSurface> ImageTransportSurface::CreateNativeGLSurface(
    gl::GLDisplay* display,
    base::WeakPtr<ImageTransportSurfaceDelegate> delegate,
    SurfaceHandle surface_handle,
    gl::GLSurfaceFormat format) {
  DCHECK_NE(surface_handle, kNullSurfaceHandle);
  scoped_refptr<gl::GLSurface> surface =
      gl::init::CreateViewGLSurface(display, surface_handle);
  bool override_vsync_for_multi_window_swap = false;
  if (gl::GetGLImplementation() == gl::kGLImplementationDesktopGL ||
      gl::GetGLImplementation() == gl::kGLImplementationEGLANGLE) {
    override_vsync_for_multi_window_swap = true;
  }
  if (!surface)
    return surface;
  return scoped_refptr<gl::GLSurface>(new PassThroughImageTransportSurface(
      delegate, surface.get(), override_vsync_for_multi_window_swap));
}

}  // namespace gpu
