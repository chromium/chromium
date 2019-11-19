// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_REPRESENTATION_SKIA_GL_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_REPRESENTATION_SKIA_GL_H_

#include "gpu/command_buffer/service/shared_image_representation.h"
#include "ui/gl/gl_context.h"

namespace gpu {
// This is a wrapper class for SharedImageRepresentationSkia to be used in GL
// mode. For most of the SharedImageBackings, SharedImageRepresentationGLTexture
// and SharedImageRepresentationSkia implementations do the same work which
// results in duplicate code. Hence instead of implementing
// SharedImageRepresentationSkia, this wrapper can be directly used or
// implemented by the backings.
class GPU_GLES2_EXPORT SharedImageRepresentationSkiaGL
    : public SharedImageRepresentationSkia {
 public:
  static std::unique_ptr<SharedImageRepresentationSkiaGL> Create(
      std::unique_ptr<SharedImageRepresentationGLTexture> gl_representation,
      scoped_refptr<SharedContextState> context_state,
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker);

  ~SharedImageRepresentationSkiaGL() override;

  sk_sp<SkSurface> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores) override;
  void EndWriteAccess(sk_sp<SkSurface> surface) override;
  sk_sp<SkPromiseImageTexture> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores) override;
  void EndReadAccess() override;

 private:
  SharedImageRepresentationSkiaGL(
      std::unique_ptr<SharedImageRepresentationGLTexture> gl_representation,
      sk_sp<SkPromiseImageTexture> promise_texture,
      scoped_refptr<SharedContextState> context_state,
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker);
  void CheckContext();

  std::unique_ptr<SharedImageRepresentationGLTexture> gl_representation_;
  sk_sp<SkPromiseImageTexture> promise_texture_;
  scoped_refptr<SharedContextState> context_state_;
  SkSurface* surface_ = nullptr;
  RepresentationAccessMode mode_ = RepresentationAccessMode::kNone;
#if DCHECK_IS_ON()
  gl::GLContext* context_;
#endif
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_REPRESENTATION_SKIA_GL_H_
