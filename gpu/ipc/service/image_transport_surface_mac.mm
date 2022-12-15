// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/image_transport_surface.h"

#include "base/threading/thread_task_runner_handle.h"
#include "gpu/ipc/service/image_transport_surface_overlay_mac.h"
#include "gpu/ipc/service/pass_through_image_transport_surface.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_surface_stub.h"

namespace gpu {

// static
scoped_refptr<gl::Presenter> ImageTransportSurface::CreatePresenter(
    gl::GLDisplay* display,
    base::WeakPtr<ImageTransportSurfaceDelegate> delegate,
    SurfaceHandle surface_handle,
    gl::GLSurfaceFormat format) {
  DCHECK_NE(surface_handle, kNullSurfaceHandle);
  if (gl::GetGLImplementation() == gl::kGLImplementationEGLGLES2 ||
      gl::GetGLImplementation() == gl::kGLImplementationEGLANGLE) {
    return base::WrapRefCounted<gl::Presenter>(
        new ImageTransportSurfaceOverlayMacEGL(
            display->GetAs<gl::GLDisplayEGL>(), delegate));
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

  if (gl::GetGLImplementation() == gl::kGLImplementationMockGL ||
      gl::GetGLImplementation() == gl::kGLImplementationStubGL) {
    return base::WrapRefCounted<gl::GLSurface>(new gl::GLSurfaceStub);
  }

  return nullptr;
}

}  // namespace gpu
