// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/image_transport_surface.h"

#include <android/native_window_jni.h>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/overloaded.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/ipc/common/gpu_surface_lookup.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/gl/android/scoped_a_native_window.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_surface_egl_surface_control.h"
#include "ui/gl/gl_surface_stub.h"

namespace gpu {

// static
scoped_refptr<gl::Presenter> ImageTransportSurface::CreatePresenter(
    gl::GLDisplay* display,
    const GpuDriverBugWorkarounds& workarounds,
    const GpuFeatureInfo& gpu_feature_info,
    SurfaceHandle surface_handle,
    DawnContextProvider* dawn_context_provider) {
  if (gl::GetGLImplementation() == gl::kGLImplementationMockGL ||
      gl::GetGLImplementation() == gl::kGLImplementationStubGL)
    return nullptr;

  if (gpu_feature_info
          .status_values[GPU_FEATURE_TYPE_ANDROID_SURFACE_CONTROL] !=
      gpu::kGpuFeatureStatusEnabled) {
    return nullptr;
  }

  DCHECK(GpuSurfaceLookup::GetInstance());
  DCHECK_NE(surface_handle, kNullSurfaceHandle);
  // On Android, the surface_handle is the id of the surface in the
  // GpuSurfaceTracker/GpuSurfaceLookup
  auto surface_record =
      GpuSurfaceLookup::GetInstance()->AcquireJavaSurface(surface_handle);

  if (!surface_record.can_be_used_with_surface_control) {
    return nullptr;
  }

  scoped_refptr<gl::Presenter> presenter;
  absl::visit(
      base::Overloaded{[&](gl::ScopedJavaSurface&& scoped_java_surface) {
                         gl::ScopedANativeWindow window(scoped_java_surface);
                         if (!window) {
                           LOG(WARNING) << "Failed to acquire ANativeWindow";
                           return;
                         }
                         presenter = new gl::GLSurfaceEGLSurfaceControl(
                             std::move(window),
                             base::SingleThreadTaskRunner::GetCurrentDefault());
                       },
                       [&](gl::ScopedJavaSurfaceControl&& surface_control) {
                         presenter = new gl::GLSurfaceEGLSurfaceControl(
                             std::move(surface_control),
                             base::SingleThreadTaskRunner::GetCurrentDefault());
                       }},
      std::move(surface_record.surface_variant));

  return presenter;
}

// static
scoped_refptr<gl::GLSurface> ImageTransportSurface::CreateNativeGLSurface(
    gl::GLDisplay* display,
    SurfaceHandle surface_handle,
    gl::GLSurfaceFormat format) {
  if (gl::GetGLImplementation() == gl::kGLImplementationMockGL ||
      gl::GetGLImplementation() == gl::kGLImplementationStubGL) {
    return base::MakeRefCounted<gl::GLSurfaceStub>();
  }

  // For some unittests, we will using ANGLE with Null ANGLE backend, in this
  // case, we need to use SurfacelessEGL.
  if (gl::GetGLImplementation() == gl::kGLImplementationEGLANGLE &&
      gl::GetANGLEImplementation() == gl::ANGLEImplementation::kNull) {
    auto surface = base::MakeRefCounted<gl::SurfacelessEGL>(
        display->GetAs<gl::GLDisplayEGL>(), gfx::Size());
    if (!surface->Initialize(format)) {
      return nullptr;
    }
    return surface;
  }

  DCHECK(GpuSurfaceLookup::GetInstance());
  DCHECK_NE(surface_handle, kNullSurfaceHandle);

  // On Android, the surface_handle is the id of the surface in the
  // GpuSurfaceTracker/GpuSurfaceLookup
  auto surface_record =
      GpuSurfaceLookup::GetInstance()->AcquireJavaSurface(surface_handle);
  if (!absl::holds_alternative<gl::ScopedJavaSurface>(
          surface_record.surface_variant)) {
    LOG(WARNING) << "Expected Java Surface";
    return nullptr;
  }
  gl::ScopedJavaSurface& scoped_java_surface =
      absl::get<gl::ScopedJavaSurface>(surface_record.surface_variant);
  gl::ScopedANativeWindow window(scoped_java_surface);

  if (!window) {
    LOG(WARNING) << "Failed to acquire ANativeWindow";
    return nullptr;
  }

  scoped_refptr<gl::GLSurface> surface = new gl::NativeViewGLSurfaceEGL(
      display->GetAs<gl::GLDisplayEGL>(), std::move(window), nullptr);

  bool initialize_success = surface->Initialize(format);
  if (!initialize_success)
    return scoped_refptr<gl::GLSurface>();

  return surface;
}

}  // namespace gpu
