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
// GLImage backing.
class GLTextureImageRepresentationClient {
 public:
  virtual bool GLTextureImageRepresentationBeginAccess(bool readonly) = 0;
  virtual void GLTextureImageRepresentationEndAccess(bool readonly) = 0;
};

// Representation of a GLTextureImageBacking or GLImageBacking
// as a GL Texture.
class GLTextureGLCommonRepresentation : public GLTextureImageRepresentation {
 public:
  GLTextureGLCommonRepresentation(SharedImageManager* manager,
                                  SharedImageBacking* backing,
                                  GLTextureImageRepresentationClient* client,
                                  MemoryTypeTracker* tracker,
                                  std::vector<raw_ptr<gles2::Texture>> texture);
  ~GLTextureGLCommonRepresentation() override;

 private:
  // GLTextureImageRepresentation:
  gles2::Texture* GetTexture(int plane_index) override;
  bool BeginAccess(GLenum mode) override;
  void EndAccess() override;

  const raw_ptr<GLTextureImageRepresentationClient> client_ = nullptr;
  std::vector<raw_ptr<gles2::Texture>> textures_;
  GLenum mode_ = 0;
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

// Skia representation for both GLTextureImageBackingHelper.
class SkiaGLCommonRepresentation : public SkiaGaneshImageRepresentation {
 public:
  class Client {
   public:
    virtual bool OnSkiaBeginReadAccess() = 0;
    virtual bool OnSkiaBeginWriteAccess() = 0;
  };
  SkiaGLCommonRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      GLTextureImageRepresentationClient* client,
      scoped_refptr<SharedContextState> context_state,
      std::vector<sk_sp<SkPromiseImageTexture>> promise_texture,
      MemoryTypeTracker* tracker);
  ~SkiaGLCommonRepresentation() override;

  void SetBeginReadAccessCallback(
      base::RepeatingClosure begin_read_access_callback);

 private:
  // SkiaImageRepresentation:
  std::vector<sk_sp<SkSurface>> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      const gfx::Rect& update_rect,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override;
  std::vector<sk_sp<SkPromiseImageTexture>> BeginWriteAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphore,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override;
  void EndWriteAccess() override;
  std::vector<sk_sp<SkPromiseImageTexture>> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override;
  void EndReadAccess() override;
  bool SupportsMultipleConcurrentReadAccess() override;

  void CheckContext();

  const raw_ptr<GLTextureImageRepresentationClient> client_ = nullptr;
  scoped_refptr<SharedContextState> context_state_;
  std::vector<sk_sp<SkPromiseImageTexture>> promise_textures_;
  std::vector<sk_sp<SkSurface>> write_surfaces_;
#if DCHECK_IS_ON()
  raw_ptr<gl::GLContext> context_ = nullptr;
#endif
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_TEXTURE_COMMON_REPRESENTATIONS_H_
