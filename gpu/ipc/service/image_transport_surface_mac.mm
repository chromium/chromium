// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/image_transport_surface.h"

#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "gpu/ipc/service/image_transport_surface_overlay_mac.h"
#include "gpu/ipc/service/pass_through_image_transport_surface.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_surface_stub.h"

namespace gpu {

// static
scoped_refptr<gl::GLSurface> ImageTransportSurface::CreateNativeSurface(
    base::WeakPtr<ImageTransportSurfaceDelegate> delegate,
    SurfaceHandle surface_handle,
    gl::GLSurfaceFormat format) {
  DCHECK_NE(surface_handle, kNullSurfaceHandle);

  switch (gl::GetGLImplementation()) {
    case gl::kGLImplementationDesktopGL:
    case gl::kGLImplementationDesktopGLCoreProfile:
    case gl::kGLImplementationAppleGL:
      return base::WrapRefCounted<gl::GLSurface>(
          new ImageTransportSurfaceOverlayMac(delegate));
#if defined(USE_EGL)
    case gl::kGLImplementationEGLGLES2:
    case gl::kGLImplementationEGLANGLE:
    case gl::kGLImplementationSwiftShaderGL:
      return base::WrapRefCounted<gl::GLSurface>(
          new ImageTransportSurfaceOverlayMacEGL(delegate));
#endif
    case gl::kGLImplementationMockGL:
    case gl::kGLImplementationStubGL:
      return base::WrapRefCounted<gl::GLSurface>(new gl::GLSurfaceStub);
    default:
      return nullptr;
  }
}

}  // namespace gpu
