// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/image_transport_surface.h"

#include "build/build_config.h"
#include "ui/gl/init/gl_factory.h"

namespace gpu {

// static
scoped_refptr<gl::Presenter> ImageTransportSurface::CreatePresenter(
    gl::GLDisplay* display,
    const GpuDriverBugWorkarounds& workarounds,
    const GpuFeatureInfo& gpu_feature_info,
    SurfaceHandle surface_handle,
    DawnContextProvider* dawn_context_provider) {
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
    SurfaceHandle surface_handle,
    gl::GLSurfaceFormat format) {
  DCHECK_NE(surface_handle, kNullSurfaceHandle);
  return gl::init::CreateViewGLSurface(display, surface_handle);
}

}  // namespace gpu
