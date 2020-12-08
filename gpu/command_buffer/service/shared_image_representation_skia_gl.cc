// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_representation_skia_gl.h"

#include "base/memory/ptr_util.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "ui/gl/gl_bindings.h"

namespace gpu {

std::ostream& operator<<(std::ostream& os, RepresentationAccessMode mode) {
  switch (mode) {
    case RepresentationAccessMode::kNone:
      os << "kNone";
      break;
    case RepresentationAccessMode::kRead:
      os << "kRead";
      break;
    case RepresentationAccessMode::kWrite:
      os << "kWrite";
      break;
  }
  return os;
}

// static method.
std::unique_ptr<SharedImageRepresentationSkiaGL>
SharedImageRepresentationSkiaGL::Create(
    std::unique_ptr<SharedImageRepresentationGLTextureBase> gl_representation,
    scoped_refptr<SharedContextState> context_state,
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker) {
  GrBackendTexture backend_texture;
  if (!GetGrBackendTexture(context_state->feature_info(),
                           gl_representation->GetTextureBase()->target(),
                           backing->size(),
                           gl_representation->GetTextureBase()->service_id(),
                           backing->format(), &backend_texture)) {
    return nullptr;
  }
  auto promise_texture = SkPromiseImageTexture::Make(backend_texture);
  if (!promise_texture)
    return nullptr;
  return base::WrapUnique(new SharedImageRepresentationSkiaGL(
      std::move(gl_representation), std::move(promise_texture),
      std::move(context_state), manager, backing, tracker));
}

SharedImageRepresentationSkiaGL::SharedImageRepresentationSkiaGL(
    std::unique_ptr<SharedImageRepresentationGLTextureBase> gl_representation,
    sk_sp<SkPromiseImageTexture> promise_texture,
    scoped_refptr<SharedContextState> context_state,
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker)
    : SharedImageRepresentationSkia(manager, backing, tracker),
      gl_representation_(std::move(gl_representation)),
      promise_texture_(std::move(promise_texture)),
      context_state_(std::move(context_state)) {
  DCHECK(gl_representation_);
#if DCHECK_IS_ON()
  context_ = gl::GLContext::GetCurrent();
#endif
}

SharedImageRepresentationSkiaGL::~SharedImageRepresentationSkiaGL() {
  DCHECK_EQ(RepresentationAccessMode::kNone, mode_);
  surface_.reset();
}

sk_sp<SkSurface> SharedImageRepresentationSkiaGL::BeginWriteAccess(
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores) {
  DCHECK_EQ(mode_, RepresentationAccessMode::kNone);
  CheckContext();

  if (!gl_representation_->BeginAccess(
          GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM)) {
    return nullptr;
  }

  mode_ = RepresentationAccessMode::kWrite;

  if (surface_)
    return surface_;

  SkColorType sk_color_type = viz::ResourceFormatToClosestSkColorType(
      /*gpu_compositing=*/true, format());
  auto surface = SkSurface::MakeFromBackendTexture(
      context_state_->gr_context(), promise_texture_->backendTexture(),
      surface_origin(), final_msaa_count, sk_color_type,
      backing()->color_space().ToSkColorSpace(), &surface_props);
  surface_ = surface;
  return surface;
}

sk_sp<SkPromiseImageTexture> SharedImageRepresentationSkiaGL::BeginWriteAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<GrBackendSurfaceMutableState>* end_state) {
  DCHECK_EQ(mode_, RepresentationAccessMode::kNone);
  CheckContext();

  if (!gl_representation_->BeginAccess(
          GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM)) {
    return nullptr;
  }
  mode_ = RepresentationAccessMode::kWrite;
  return promise_texture_;
}

void SharedImageRepresentationSkiaGL::EndWriteAccess(sk_sp<SkSurface> surface) {
  DCHECK_EQ(mode_, RepresentationAccessMode::kWrite);
  if (surface) {
    DCHECK(surface_);
    DCHECK_EQ(surface.get(), surface_.get());
    surface.reset();
    DCHECK(surface_->unique());
  }

  gl_representation_->EndAccess();
  mode_ = RepresentationAccessMode::kNone;
}

sk_sp<SkPromiseImageTexture> SharedImageRepresentationSkiaGL::BeginReadAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores) {
  DCHECK_EQ(mode_, RepresentationAccessMode::kNone);
  CheckContext();

  if (!gl_representation_->BeginAccess(
          GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM)) {
    return nullptr;
  }
  mode_ = RepresentationAccessMode::kRead;
  return promise_texture_;
}

void SharedImageRepresentationSkiaGL::EndReadAccess() {
  DCHECK_EQ(mode_, RepresentationAccessMode::kRead);
  CheckContext();

  gl_representation_->EndAccess();
  mode_ = RepresentationAccessMode::kNone;
}

void SharedImageRepresentationSkiaGL::CheckContext() {
#if DCHECK_IS_ON()
  DCHECK(gl::GLContext::GetCurrent() == context_);
#endif
}

bool SharedImageRepresentationSkiaGL::SupportsMultipleConcurrentReadAccess() {
  return gl_representation_->SupportsMultipleConcurrentReadAccess();
}

}  // namespace gpu
