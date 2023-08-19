// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SKIA_GL_IMAGE_REPRESENTATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SKIA_GL_IMAGE_REPRESENTATION_H_

#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "ui/gl/gl_context.h"

namespace gpu {
// This is a wrapper class for SkiaGaneshImageRepresentation to be used in GL
// mode. For most of the SharedImageBackings, GLTextureImageRepresentation
// and SkiaGaneshImageRepresentation implementations do the same work which
// results in duplicate code. Hence instead of implementing
// SkiaGaneshImageRepresentation, this wrapper can be directly used or
// implemented by the backings.
class GPU_GLES2_EXPORT SkiaGLImageRepresentation
    : public SkiaGaneshImageRepresentation {
 public:
  static std::unique_ptr<SkiaGLImageRepresentation> Create(
      std::unique_ptr<GLTextureImageRepresentationBase> gl_representation,
      scoped_refptr<SharedContextState> context_state,
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker);

  ~SkiaGLImageRepresentation() override;

  std::vector<sk_sp<SkSurface>> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      const gfx::Rect& update_rect,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<skgpu::MutableTextureState>* end_state) override;
  std::vector<sk_sp<GrPromiseImageTexture>> BeginWriteAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<skgpu::MutableTextureState>* end_state) override;
  void EndWriteAccess() override;
  std::vector<sk_sp<GrPromiseImageTexture>> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<skgpu::MutableTextureState>* end_state) override;
  void EndReadAccess() override;

  bool SupportsMultipleConcurrentReadAccess() override;

 protected:
  SkiaGLImageRepresentation(
      std::unique_ptr<GLTextureImageRepresentationBase> gl_representation,
      std::vector<sk_sp<GrPromiseImageTexture>> promise_textures,
      scoped_refptr<SharedContextState> context_state,
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker);

  void ClearCachedSurfaces();

 private:
  void CheckContext();

  std::unique_ptr<GLTextureImageRepresentationBase> gl_representation_;
  std::vector<sk_sp<GrPromiseImageTexture>> promise_textures_;
  scoped_refptr<SharedContextState> context_state_;
  std::vector<sk_sp<SkSurface>> surfaces_;
  RepresentationAccessMode mode_ = RepresentationAccessMode::kNone;
#if DCHECK_IS_ON()
  raw_ptr<gl::GLContext> context_;
#endif
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SKIA_GL_IMAGE_REPRESENTATION_H_
