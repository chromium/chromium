// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_OZONE_IMAGE_REPRESENTATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_OZONE_IMAGE_REPRESENTATION_H_

#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/texture_manager.h"

namespace gpu {
class OzoneImageBacking;
class OzoneImageGLTexturesHolder;

// Representation of an Ozone-backed SharedImage that can be accessed as a
// GL texture with passthrough.
class GLTexturePassthroughOzoneImageRepresentation
    : public GLTexturePassthroughImageRepresentation {
 public:
  GLTexturePassthroughOzoneImageRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      scoped_refptr<OzoneImageGLTexturesHolder> textures_holder,
      bool should_mark_context_lost_textures_holder);
  ~GLTexturePassthroughOzoneImageRepresentation() override;

  // GLTexturePassthroughImageRepresentation implementation.
  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough(
      int plane_index) override;
  bool BeginAccess(GLenum mode) override;
  void EndAccess() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(OzoneImageBackingFactoryTest,
                           MarksContextLostOnContextLost2);

  OzoneImageBacking* GetOzoneBacking();

  scoped_refptr<OzoneImageGLTexturesHolder> textures_holder_;
  const bool should_mark_context_lost_textures_holder_;
  GLenum current_access_mode_ = 0;
  bool need_end_fence_ = false;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_OZONE_IMAGE_REPRESENTATION_H_
