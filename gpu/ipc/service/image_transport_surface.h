// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_IMAGE_TRANSPORT_SURFACE_H_
#define GPU_IPC_SERVICE_IMAGE_TRANSPORT_SURFACE_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/dawn_context_provider.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/service/gpu_ipc_service_export.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/presenter.h"

namespace gpu {
class GpuDriverBugWorkarounds;
struct GpuFeatureInfo;

// The GPU process is agnostic as to how it displays results. On some platforms
// it renders directly to window. On others it renders offscreen and transports
// the results to the browser process to display. This file provides a simple
// framework for making the offscreen path seem more like the onscreen path.

class GPU_IPC_SERVICE_EXPORT ImageTransportSurface {
 public:
  // Creates the appropriate presenter if surfaceless presentation is supported.
  // This will be implemented separately by each platform. On failure, a null
  // scoped_refptr should be returned. Callers should try to fallback to
  // presentation using GLSurface by calling `CreateNativeGLSurface` below.
  static scoped_refptr<gl::Presenter> CreatePresenter(
      gl::GLDisplay* display,
      const GpuDriverBugWorkarounds& workarounds,
      const GpuFeatureInfo& gpu_feature_info,
      SurfaceHandle surface_handle,
      DawnContextProvider* dawn_context_provider);

  // Creates the appropriate native surface depending on the GL implementation.
  // This will be implemented separately by each platform. On failure, a null
  // scoped_refptr should be returned.
  static scoped_refptr<gl::GLSurface> CreateNativeGLSurface(
      gl::GLDisplay* display,
      SurfaceHandle surface_handle,
      gl::GLSurfaceFormat format);

  ImageTransportSurface(const ImageTransportSurface&) = delete;
  ImageTransportSurface& operator=(const ImageTransportSurface&) = delete;
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_IMAGE_TRANSPORT_SURFACE_H_
