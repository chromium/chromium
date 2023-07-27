// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_TEXTURE_COMMON_REPRESENTATIONS_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_TEXTURE_COMMON_REPRESENTATIONS_H_

#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/gl_context.h"

namespace gpu {

// Interface through which a representation that has a GL texture calls into its
// backing.
class GLTextureImageRepresentationClient {
 public:
  virtual bool GLTextureImageRepresentationBeginAccess(bool readonly) = 0;
  virtual void GLTextureImageRepresentationEndAccess(bool readonly) = 0;
};

// Representation of a GLTextureImageBacking or
// GLTextureImageBackingPassthrough as a GL TexturePassthrough.
class GLTexturePassthroughGLCommonRepresentation
    : public GLTexturePassthroughImageRepresentation {
 public:
  class Client {
   public:
    virtual bool OnGLTexturePassthroughBeginAccess(GLenum mode) = 0;
  };
  GLTexturePassthroughGLCommonRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      GLTextureImageRepresentationClient* client,
      MemoryTypeTracker* tracker,
      std::vector<scoped_refptr<gles2::TexturePassthrough>> textures);
  ~GLTexturePassthroughGLCommonRepresentation() override;

 private:
  // GLTexturePassthroughImageRepresentation:
  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough(
      int plane_index) override;
  bool BeginAccess(GLenum mode) override;
  void EndAccess() override;

  const raw_ptr<GLTextureImageRepresentationClient> client_ = nullptr;
  std::vector<scoped_refptr<gles2::TexturePassthrough>> textures_;
  GLenum mode_ = 0;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_TEXTURE_COMMON_REPRESENTATIONS_H_
