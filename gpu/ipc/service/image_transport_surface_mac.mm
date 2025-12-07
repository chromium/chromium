// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/image_transport_surface.h"

#include "gpu/ipc/service/image_transport_surface_overlay_mac.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_surface_stub.h"

namespace gpu {

// static
scoped_refptr<gl::Presenter> ImageTransportSurface::CreatePresenter(
    scoped_refptr<SharedContextState> context_state,
    const GpuDriverBugWorkarounds& workarounds,
    const GpuFeatureInfo& gpu_feature_info,
    SurfaceHandle surface_handle) {
  DCHECK_NE(surface_handle, kNullSurfaceHandle);
  if (gl::GetGLImplementation() == gl::kGLImplementationEGLGLES2 ||
      gl::GetGLImplementation() == gl::kGLImplementationEGLANGLE) {
    return base::MakeRefCounted<ImageTransportSurfaceOverlayMacEGL>(
        std::move(context_state), surface_handle);
  }

  return nullptr;
}

// static
scoped_refptr<gl::GLSurface> ImageTransportSurface::CreateNativeGLSurface(
    gl::GLDisplay* display,
    SurfaceHandle surface_handle,
    gl::GLSurfaceFormat format) {
  DCHECK_NE(surface_handle, kNullSurfaceHandle);

  if (gl::GetGLImplementation() == gl::kGLImplementationMockGL ||
      gl::GetGLImplementation() == gl::kGLImplementationStubGL) {
    return base::WrapRefCounted<gl::GLSurface>(new gl::GLSurfaceStub);
  }

  return nullptr;
}

}  // namespace gpu
