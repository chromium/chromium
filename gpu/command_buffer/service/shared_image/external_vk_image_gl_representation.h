// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_EXTERNAL_VK_IMAGE_GL_REPRESENTATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_EXTERNAL_VK_IMAGE_GL_REPRESENTATION_H_

#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/service/external_semaphore.h"
#include "gpu/command_buffer/service/shared_image/external_vk_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"

namespace gpu {

// ExternalVkImageGLRepresentationShared implements BeginAccess and EndAccess
// methods for ExternalVkImageGLRepresentation and
// ExternalVkImageGLPassthroughRepresentation.
class ExternalVkImageGLRepresentationShared {
 public:
  static void AcquireTexture(ExternalSemaphore* semaphore,
                             const std::vector<GLuint>& texture_ids,
                             const std::vector<GLenum>& src_layouts);
  static ExternalSemaphore ReleaseTexture(
      ExternalSemaphorePool* pool,
      const std::vector<GLuint>& texture_ids,
      const std::vector<GLenum>& dst_layouts);

  ExternalVkImageGLRepresentationShared(
      SharedImageBacking* backing,
      std::vector<GLuint> texture_service_ids);

  ExternalVkImageGLRepresentationShared(
      const ExternalVkImageGLRepresentationShared&) = delete;
  ExternalVkImageGLRepresentationShared& operator=(
      const ExternalVkImageGLRepresentationShared&) = delete;

  ~ExternalVkImageGLRepresentationShared();

  bool BeginAccess(GLenum mode);
  void EndAccess();

  ExternalVkImageBacking* backing_impl() const { return backing_; }

 private:
  viz::VulkanContextProvider* context_provider() const {
    return backing_impl()->context_provider();
  }

  const raw_ptr<ExternalVkImageBacking> backing_;
  const std::vector<GLuint> texture_service_ids_;
  GLenum current_access_mode_ = 0;
  std::vector<ExternalSemaphore> begin_access_semaphores_;
};

class ExternalVkImageGLRepresentation : public GLTextureImageRepresentation {
 public:
  ExternalVkImageGLRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      std::vector<raw_ptr<gles2::Texture, VectorExperimental>> textures);

  ExternalVkImageGLRepresentation(const ExternalVkImageGLRepresentation&) =
      delete;
  ExternalVkImageGLRepresentation& operator=(
      const ExternalVkImageGLRepresentation&) = delete;

  ~ExternalVkImageGLRepresentation() override;

  // GLTextureImageRepresentation implementation.
  gles2::Texture* GetTexture(int plane_index) override;
  bool BeginAccess(GLenum mode) override;
  void EndAccess() override;

 private:
  std::vector<raw_ptr<gles2::Texture, VectorExperimental>> textures_;
  ExternalVkImageGLRepresentationShared representation_shared_;
};

class ExternalVkImageGLPassthroughRepresentation
    : public GLTexturePassthroughImageRepresentation {
 public:
  ExternalVkImageGLPassthroughRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      std::vector<scoped_refptr<gles2::TexturePassthrough>> texture);

  ExternalVkImageGLPassthroughRepresentation(
      const ExternalVkImageGLPassthroughRepresentation&) = delete;
  ExternalVkImageGLPassthroughRepresentation& operator=(
      const ExternalVkImageGLPassthroughRepresentation&) = delete;

  ~ExternalVkImageGLPassthroughRepresentation() override;

  // GLTexturePassthroughImageRepresentation implementation.
  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough(
      int plane_index) override;
  bool BeginAccess(GLenum mode) override;
  void EndAccess() override;

 private:
  std::vector<scoped_refptr<gles2::TexturePassthrough>> textures_;
  ExternalVkImageGLRepresentationShared representation_shared_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_EXTERNAL_VK_IMAGE_GL_REPRESENTATION_H_
