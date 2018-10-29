// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SKIA_UTILS_H_
#define GPU_COMMAND_BUFFER_SERVICE_SKIA_UTILS_H_

#include "gpu/gpu_gles2_export.h"

// Forwardly declare a few GL types to avoid including GL header files.
typedef int GLint;
typedef unsigned int GLenum;
typedef unsigned int GLuint;

class GrBackendTexture;

namespace gfx {
class Size;
}  // namespace gfx

namespace gl {
struct GLVersionInfo;
}  // namespace gl

namespace gpu {
class TextureBase;

// Wraps a gpu::TextureBase into Skia API as a GrBackendTexture. Skia does not
// take ownership.  Returns true on success.
// TODO(ericrk): Remove this once all paths migrate to SharedImage.
GPU_GLES2_EXPORT bool GetGrBackendTexture(const gl::GLVersionInfo& version_info,
                                          const TextureBase& texture,
                                          GLint sk_color_type,
                                          GrBackendTexture* gr_texture);

// Creates a GrBackendTexture from a service ID. Skia does not take ownership.
// Returns true on success.
GPU_GLES2_EXPORT bool GetGrBackendTexture(GLenum target,
                                          const gfx::Size& size,
                                          GLenum internal_format,
                                          GLenum driver_internal_format,
                                          GLuint service_id,
                                          GLint sk_color_type,
                                          GrBackendTexture* gr_texture);

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SKIA_UTILS_H_
