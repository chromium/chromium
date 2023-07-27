// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/gl_texture_common_representations.h"

namespace gpu {

///////////////////////////////////////////////////////////////////////////////
// GLTexturePassthroughGLCommonRepresentation

GLTexturePassthroughGLCommonRepresentation::
    GLTexturePassthroughGLCommonRepresentation(
        SharedImageManager* manager,
        SharedImageBacking* backing,
        GLTextureImageRepresentationClient* client,
        MemoryTypeTracker* tracker,
        std::vector<scoped_refptr<gles2::TexturePassthrough>> textures)
    : GLTexturePassthroughImageRepresentation(manager, backing, tracker),
      client_(client),
      textures_(std::move(textures)) {
  DCHECK_EQ(textures_.size(), NumPlanesExpected());
}

GLTexturePassthroughGLCommonRepresentation::
    ~GLTexturePassthroughGLCommonRepresentation() = default;

const scoped_refptr<gles2::TexturePassthrough>&
GLTexturePassthroughGLCommonRepresentation::GetTexturePassthrough(
    int plane_index) {
  DCHECK(format().IsValidPlaneIndex(plane_index));
  return textures_[plane_index];
}

bool GLTexturePassthroughGLCommonRepresentation::BeginAccess(GLenum mode) {
  DCHECK(mode_ == 0);
  mode_ = mode;
  bool readonly = mode_ != GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM;
  if (client_) {
    return client_->GLTextureImageRepresentationBeginAccess(readonly);
  }
  return true;
}

void GLTexturePassthroughGLCommonRepresentation::EndAccess() {
  DCHECK(mode_ != 0);
  bool readonly = mode_ != GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM;
  mode_ = 0;
  if (client_) {
    return client_->GLTextureImageRepresentationEndAccess(readonly);
  }
}

}  // namespace gpu
