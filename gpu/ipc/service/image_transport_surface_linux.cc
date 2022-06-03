// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/image_transport_surface.h"

#include "gpu/ipc/service/pass_through_image_transport_surface.h"
#include "ui/gl/init/gl_factory.h"

namespace gpu {

// static
scoped_refptr<gl::GLSurface> ImageTransportSurface::CreateNativeSurface(
    base::WeakPtr<ImageTransportSurfaceDelegate> delegate,
    SurfaceHandle surface_handle,
    gl::GLSurfaceFormat format) {
  DCHECK_NE(surface_handle, kNullSurfaceHandle);
  scoped_refptr<gl::GLSurface> surface;
  bool override_vsync_for_multi_window_swap = false;
#if defined(USE_OZONE)
  surface = gl::init::CreateSurfacelessViewGLSurface(surface_handle);
#endif
  if (!surface) {
    surface = gl::init::CreateViewGLSurface(surface_handle);
    if (gl::GetGLImplementation() == gl::kGLImplementationDesktopGL ||
        gl::GetGLImplementation() == gl::kGLImplementationEGLANGLE) {
      override_vsync_for_multi_window_swap = true;
    }
  }
  if (!surface)
    return surface;
  return scoped_refptr<gl::GLSurface>(new PassThroughImageTransportSurface(
      delegate, surface.get(), override_vsync_for_multi_window_swap));
}

}  // namespace gpu
