// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/image_transport_surface.h"

#include "gpu/ipc/service/pass_through_image_transport_surface.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_surface_stub.h"
#include "ui/gl/init/gl_factory.h"

namespace gpu {

// static
scoped_refptr<gl::GLSurface> ImageTransportSurface::CreateNativeSurface(
    base::WeakPtr<ImageTransportSurfaceDelegate> delegate,
    SurfaceHandle surface_handle,
    gl::GLSurfaceFormat format) {
  if (gl::GetGLImplementation() == gl::kGLImplementationMockGL ||
      gl::GetGLImplementation() == gl::kGLImplementationStubGL) {
    return new gl::GLSurfaceStub;
  }

  scoped_refptr<gl::GLSurface> surface =
      gl::init::CreateViewGLSurface(surface_handle);

  if (!surface)
    return surface;
  return base::MakeRefCounted<PassThroughImageTransportSurface>(
      delegate, surface.get(), false);
}

}  // namespace gpu
