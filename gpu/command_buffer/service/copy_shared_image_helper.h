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

  base::expected<void, GLError> ConvertRGBAToYUVAMailboxes(
      GLenum yuv_color_space,
      GLenum plane_config,
      GLenum subsampling,
      const volatile GLbyte* mailboxes_in);
  base::expected<void, GLError> ConvertYUVAMailboxesToRGB(
      GLenum yuv_color_space,
      GLenum plane_config,
      GLenum subsampling,
      const volatile GLbyte* mailboxes_in);
  base::expected<void, GLError> CopySharedImage(
      GLint xoffset,
      GLint yoffset,
      GLint x,
      GLint y,
      GLsizei width,
      GLsizei height,
      GLboolean unpack_flip_y,
      const volatile GLbyte* mailboxes);
  base::expected<void, GLError> ReadPixels(
      GLint src_x,
      GLint src_y,
      GLint plane_index,
      GLuint row_bytes,
      SkImageInfo dst_info,
      void* pixel_address,
      std::unique_ptr<SkiaImageRepresentation> source_shared_image);

 private:
  raw_ptr<SharedImageRepresentationFactory> representation_factory_ = nullptr;
  raw_ptr<SharedContextState> shared_context_state_ = nullptr;
  bool is_drdc_enabled_ = false;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_COPY_SHARED_IMAGE_HELPER_H_