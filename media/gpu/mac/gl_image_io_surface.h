// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_MAC_GL_IMAGE_IO_SURFACE_H_
#define MEDIA_GPU_MAC_GL_IMAGE_IO_SURFACE_H_

#include <CoreVideo/CVPixelBuffer.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <IOSurface/IOSurfaceRef.h>
#include <stdint.h>

#include "base/mac/scoped_cftyperef.h"
#include "base/threading/thread_checker.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/generic_shared_memory_id.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_image.h"

namespace media {

class VTVideoDecodeAccelerator;

// GLImage subclass that is used by VTVideoDecodeAccelerator.
// NOTE: No new usage of this class should be introduced, as it is in the
// process of being eliminated.
class MEDIA_GPU_EXPORT GLImageIOSurface : public gl::GLImage {
 private:
  friend VTVideoDecodeAccelerator;

  static GLImageIOSurface* Create(const gfx::Size& size);

  GLImageIOSurface(const GLImageIOSurface&) = delete;
  GLImageIOSurface& operator=(const GLImageIOSurface&) = delete;

  // IOSurfaces coming from video decode are wrapped in a CVPixelBuffer
  // and may be discarded if the owning CVPixelBuffer is destroyed. This
  // initialization will ensure that the CVPixelBuffer be retained for the
  // lifetime of the GLImage. The color space specified in `color_space` must be
  // set to the color space specified by `cv_pixel_buffer`'s attachments.
  bool InitializeWithCVPixelBuffer(CVPixelBufferRef cv_pixel_buffer,
                                   uint32_t io_surface_plane,
                                   gfx::GenericSharedMemoryId io_surface_id,
                                   gfx::BufferFormat format);

  // Dumps information about the memory backing this instance to a dump named
  // |dump_name|.
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t process_tracing_id,
                    const std::string& dump_name);

  // Overridden from GLImage:
  gfx::Size GetSize() override;

  GLImageIOSurface(const gfx::Size& size);
  ~GLImageIOSurface() override;

  // Initialize to wrap of |io_surface|. The format of the plane to wrap is
  // specified in |format|. The index of the plane to wrap is
  // |io_surface_plane|. If |format| is a multi-planar format (e.g,
  // YUV_420_BIPLANAR or P010), then this will automatically convert from YUV
  // to RGB, and |io_surface_plane| is ignored.
  bool Initialize(IOSurfaceRef io_surface,
                  uint32_t io_surface_plane,
                  gfx::GenericSharedMemoryId io_surface_id,
                  gfx::BufferFormat format);

  const gfx::Size size_;
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface_;
  base::ScopedCFTypeRef<CVPixelBufferRef> cv_pixel_buffer_;
  gfx::GenericSharedMemoryId io_surface_id_;

  // The plane that is bound for this image. If the plane is invalid, then
  // this is a multi-planar IOSurface, which will be copied instead of bound.
  static constexpr uint32_t kInvalidIOSurfacePlane = -1;
  uint32_t io_surface_plane_ = kInvalidIOSurfacePlane;

  base::ThreadChecker thread_checker_;
};

}  // namespace media

#endif  // MEDIA_GPU_MAC_GL_IMAGE_IO_SURFACE_H_
