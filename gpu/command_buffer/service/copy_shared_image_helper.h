// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_COPY_SHARED_IMAGE_HELPER_H_
#define GPU_COMMAND_BUFFER_SERVICE_COPY_SHARED_IMAGE_HELPER_H_

#include <stdint.h>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/types/expected.h"
#include "gpu/command_buffer/common/gl2_types.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/gpu_gles2_export.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkYUVAInfo.h"

namespace gpu {

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

  CopySharedImageHelper(
      SharedImageRepresentationFactory* representation_factory,
      SharedContextState* shared_context_state);
  ~CopySharedImageHelper();

  base::expected<void, GLError> CopySharedImage(
      GLint xoffset,
      GLint yoffset,
      GLint x,
      GLint y,
      GLsizei width,
      GLsizei height,
      GLboolean unpack_flip_y,
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
      GLboolean flip_y,
      const volatile GLbyte* src_mailbox);
  base::expected<void, GLError> ReadPixels(
      GLint src_x,
      GLint src_y,
      GLint plane_index,
      GLuint row_bytes,
      SkImageInfo dst_info,
      void* pixel_address,
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
  raw_ptr<SharedImageRepresentationFactory> representation_factory_ = nullptr;
  raw_ptr<SharedContextState> shared_context_state_ = nullptr;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_COPY_SHARED_IMAGE_HELPER_H_
