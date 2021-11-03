// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/image_transport_surface.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/ipc/common/gpu_surface_lookup.h"
#include "gpu/ipc/service/pass_through_image_transport_surface.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_surface_egl_surface_control.h"
#include "ui/gl/gl_surface_stub.h"

namespace gpu {

// static
scoped_refptr<gl::GLSurface> ImageTransportSurface::CreateNativeSurface(
    base::WeakPtr<ImageTransportSurfaceDelegate> delegate,
    SurfaceHandle surface_handle,
    gl::GLSurfaceFormat format) {
  if (gl::GetGLImplementation() == gl::kGLImplementationMockGL ||
      gl::GetGLImplementation() == gl::kGLImplementationStubGL)
    return new gl::GLSurfaceStub;
  DCHECK(GpuSurfaceLookup::GetInstance());
  DCHECK_NE(surface_handle, kNullSurfaceHandle);
  // On Android, the surface_handle is the id of the surface in the
  // GpuSurfaceTracker/GpuSurfaceLookup
  bool can_be_used_with_surface_control = false;
  ANativeWindow* window = GpuSurfaceLookup::GetInstance()->AcquireNativeWidget(
      surface_handle, &can_be_used_with_surface_control);
  if (!window) {
    LOG(WARNING) << "Failed to acquire native widget.";
    return nullptr;
  }
  scoped_refptr<gl::GLSurface> surface;

  if (delegate &&
      delegate->GetFeatureInfo()->feature_flags().android_surface_control &&
      can_be_used_with_surface_control) {
    surface = new gl::GLSurfaceEGLSurfaceControl(
        window, base::ThreadTaskRunnerHandle::Get());
  } else {
    surface = new gl::NativeViewGLSurfaceEGL(window, nullptr);
  }

  bool initialize_success = surface->Initialize(format);
  ANativeWindow_release(window);
  if (!initialize_success)
    return scoped_refptr<gl::GLSurface>();

  return scoped_refptr<gl::GLSurface>(
      new PassThroughImageTransportSurface(delegate, surface.get(), false));
}

}  // namespace gpu
