// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/image_transport_surface.h"

namespace gpu {
scoped_refptr<gl::GLSurface>
ImageTransportSurface::CreatePresenterOrNativeSurface(
    gl::GLDisplay* display,
    base::WeakPtr<ImageTransportSurfaceDelegate> delegate,
    SurfaceHandle surface_handle,
    gl::GLSurfaceFormat format) {
  if (auto surface = CreatePresenter(display, delegate, surface_handle, format))
    return surface;

  return CreateNativeGLSurface(display, delegate, surface_handle, format);
}
}  // namespace gpu