// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_COPY_SHARED_IMAGE_HELPER_H_
#define GPU_COMMAND_BUFFER_SERVICE_COPY_SHARED_IMAGE_HELPER_H_

#include <stdint.h>

#include <array>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/types/expected.h"
#include "gpu/command_buffer/common/gl2_types.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/gpu_gles2_export.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkYUVAInfo.h"

struct SkIRect;
struct SkISize;

namespace gpu {

class GpuDriverBugWorkarounds;
class SharedContextState;
class SharedImageRepresentationFactory;

// A helper class implementing the common functions for raster and gl
// passthrough command buffer decoders.
class GPU_GLES2_EXPORT CopySharedImageHelper {
 public:
  struct GLError {
    GLError(GLenum gl_error, std::string function_name, std::string msg);

    GLenum gl_error = 0;
    std::string function_name = "";
    std::string msg = "";
  };

  // Decoder facilities needed by the readback functions.
  class ReadbackDelegate {
   public:
    virtual const GpuDriverBugWorkarounds& workarounds() const = 0;
    virtual bool WasContextLost() const = 0;
    virtual bool CheckResetStatus() = 0;
    virtual void DoFinish() = 0;

   protected:
    virtual ~ReadbackDelegate() = default;
  };

  CopySharedImageHelper(
      SharedImageRepresentationFactory* representation_factory,
      SharedContextState* shared_context_state);
  ~CopySharedImageHelper();

  base::expected<void, GLError> CopySharedImage(
      GLint xoffset,
      GLint yoffset,
      GLint x,
      GLint y,
      GLsizei src_width,
      GLsizei src_height,
      GLsizei dst_width,
      GLsizei dst_height,
      const volatile GLbyte* mailboxes);
  // Only used by passthrough decoder.
  // TODO(crbug.com/40064510): Handle this use-case for graphite.
  base::expected<void, GLError> CopySharedImageToGLTexture(
      GLuint texture_service_id,
      GLenum target,
      GLuint internal_format,
      GLenum type,
      GLint src_x,
      GLint src_y,
      GLsizei width,
      GLsizei height,
      GrSurfaceOrigin dst_origin,
      const volatile GLbyte* src_mailbox);
  base::expected<void, GLError> ReadPixels(
      GLint src_x,
      GLint src_y,
      GLint plane_index,
      GLuint row_bytes,
      SkImageInfo dst_info,
      void* pixel_address,
      std::unique_ptr<SkiaImageRepresentation> source_shared_image);
  // Reads back the `src_width` x `src_height` rect at (`src_x`, `src_y`) of
  // `source_shared_image`, rescaled to `dst_width` x `dst_height`, as I420 into
  // the given planes, which must be large enough for that size.
  base::expected<void, GLError> ReadPixelsYUV(
      ReadbackDelegate* delegate,
      GLuint src_x,
      GLuint src_y,
      GLuint src_width,
      GLuint src_height,
      GLuint dst_width,
      GLuint dst_height,
      uint8_t* y_out,
      GLuint y_stride,
      uint8_t* u_out,
      GLuint u_stride,
      uint8_t* v_out,
      GLuint v_stride,
      std::unique_ptr<SkiaImageRepresentation> source_shared_image);
  base::expected<void, GLError> WritePixelsYUV(
      GLuint src_width,
      GLuint src_height,
      std::array<SkPixmap, SkYUVAInfo::kMaxPlanes> pixmaps,
      std::vector<GrBackendSemaphore> end_semaphores,
      std::unique_ptr<SkiaImageRepresentation> dest_shared_image,
      std::unique_ptr<SkiaImageRepresentation::ScopedWriteAccess>
          dest_scoped_access);

 private:
  // The two implementations of ReadPixelsYUV(): NV12 sources are read plane by
  // plane (Y as R8, UV as RG8), everything else is read as one RGB image that
  // Skia converts to YUV420. Both take over `end_semaphores` and, on failure
  // and on Ganesh, end the read access; on Graphite the read is submitted and
  // the access is ended when `source_scoped_access` is destroyed. `src_rect`
  // and `dst_size` must be even for NV12.
  base::expected<void, GLError> ReadPixelsYUVInternalNV12(
      ReadbackDelegate* delegate,
      SkiaImageRepresentation::ScopedReadAccess* source_scoped_access,
      std::vector<GrBackendSemaphore> end_semaphores,
      const SkIRect& src_rect,
      const SkISize& dst_size,
      uint8_t* y_out,
      GLuint y_stride,
      uint8_t* u_out,
      GLuint u_stride,
      uint8_t* v_out,
      GLuint v_stride);
  base::expected<void, GLError> ReadPixelsYUVInternalRGB(
      ReadbackDelegate* delegate,
      const SkiaImageRepresentation& source_shared_image,
      SkiaImageRepresentation::ScopedReadAccess* source_scoped_access,
      std::vector<GrBackendSemaphore> end_semaphores,
      const SkIRect& src_rect,
      const SkISize& dst_size,
      uint8_t* y_out,
      GLuint y_stride,
      uint8_t* u_out,
      GLuint u_stride,
      uint8_t* v_out,
      GLuint v_stride);
  // Ends `source_scoped_access`, submits `end_semaphores` and returns the
  // error; for failures before a readback was issued.
  base::unexpected<GLError> EndReadAccessWithError(
      SkiaImageRepresentation::ScopedReadAccess* source_scoped_access,
      std::vector<GrBackendSemaphore> end_semaphores,
      GLenum error,
      const char* msg);
  // Ends `source_scoped_access`, flushes with `end_semaphores` and waits for
  // the GPU, which runs the callbacks of the async reads issued before (Ganesh
  // only runs them from DoFinish()).
  void FinishGaneshReadback(
      ReadbackDelegate* delegate,
      SkiaImageRepresentation::ScopedReadAccess* source_scoped_access,
      std::vector<GrBackendSemaphore> end_semaphores);

  raw_ptr<SharedImageRepresentationFactory> representation_factory_ = nullptr;
  raw_ptr<SharedContextState> shared_context_state_ = nullptr;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_COPY_SHARED_IMAGE_HELPER_H_
