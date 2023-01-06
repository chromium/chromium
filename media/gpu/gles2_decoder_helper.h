// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_GLES2_DECODER_HELPER_H_
#define MEDIA_GPU_GLES2_DECODER_HELPER_H_

#include <stdint.h>

#include <memory>

#include "media/gpu/media_gpu_export.h"
#include "ui/gl/gl_bindings.h"

namespace gpu {
class DecoderContext;
struct Mailbox;
namespace gles2 {
class AbstractTexture;
}  // namespace gles2
}  // namespace gpu

namespace gl {
class GLContext;
}  // namespace gl

namespace media {

class CommandBufferHelperImpl;

// Utility methods to simplify working with a gpu::DecoderContext from
// inside VDAs.
class MEDIA_GPU_EXPORT GLES2DecoderHelper {
 public:
  static std::unique_ptr<GLES2DecoderHelper> Create(
      gpu::DecoderContext* decoder);

  virtual ~GLES2DecoderHelper() {}

  // TODO(sandersd): Provide scoped version?
  virtual bool MakeContextCurrent() = 0;

  // Creates a texture and configures it as a video frame (linear filtering,
  // clamp to edge). The context must be current.  It is up to the caller to
  // ensure that the entire texture is initialized before providing it to the
  // renderer.  For th
  //
  // See glTexImage2D() for parameter definitions.
  //
  // Returns nullptr on failure, but there are currently no failure paths.
  virtual std::unique_ptr<gpu::gles2::AbstractTexture> CreateTexture(
      GLenum target,
      GLenum internal_format,
      GLsizei width,
      GLsizei height,
      GLenum format,
      GLenum type) = 0;

  // Gets the associated GLContext.
  virtual gl::GLContext* GetGLContext() = 0;

 private:
  // Creates a legacy mailbox for a texture.
  // NOTE: We are in the process of eliminating this method. DO NOT ADD ANY NEW
  // USAGES - instead, reach out to shared-image-team@ with your use case. See
  // crbug.com/1273084.
  virtual gpu::Mailbox CreateLegacyMailbox(
      gpu::gles2::AbstractTexture* texture_ref) = 0;

  friend class CommandBufferHelperImpl;
};

}  // namespace media

#endif  // MEDIA_GPU_GLES2_DECODER_HELPER_H_
