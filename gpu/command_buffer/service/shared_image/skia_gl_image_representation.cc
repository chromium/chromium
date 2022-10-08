// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/skia_gl_image_representation.h"

#include "base/memory/ptr_util.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrContextThreadSafeProxy.h"
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
std::unique_ptr<SkiaGLImageRepresentation> SkiaGLImageRepresentation::Create(
    std::unique_ptr<GLTextureImageRepresentationBase> gl_representation,
    scoped_refptr<SharedContextState> context_state,
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker) {
  GrBackendTexture backend_texture;
  if (!GetGrBackendTexture(
          context_state->feature_info(),
          gl_representation->GetTextureBase()->target(), backing->size(),
          gl_representation->GetTextureBase()->service_id(),
          (backing->format()).resource_format(),
          context_state->gr_context()->threadSafeProxy(), &backend_texture)) {
    return nullptr;
  }
  auto promise_texture = SkPromiseImageTexture::Make(backend_texture);
  if (!promise_texture)
    return nullptr;
  return base::WrapUnique(new SkiaGLImageRepresentation(
      std::move(gl_representation), std::move(promise_texture),
      std::move(context_state), manager, backing, tracker));
}

SkiaGLImageRepresentation::SkiaGLImageRepresentation(
    std::unique_ptr<GLTextureImageRepresentationBase> gl_representation,
    sk_sp<SkPromiseImageTexture> promise_texture,
    scoped_refptr<SharedContextState> context_state,
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker)
    : SkiaImageRepresentation(manager, backing, tracker),
      gl_representation_(std::move(gl_representation)),
      promise_texture_(std::move(promise_texture)),
      context_state_(std::move(context_state)) {
  DCHECK(gl_representation_);
#if DCHECK_IS_ON()
  context_ = gl::GLContext::GetCurrent();
#endif
}

SkiaGLImageRepresentation::~SkiaGLImageRepresentation() {
  DCHECK_EQ(RepresentationAccessMode::kNone, mode_);
  surface_.reset();

  DCHECK_EQ(!has_context(), context_state_->context_lost());
  if (!has_context())
    gl_representation_->OnContextLost();
}

sk_sp<SkSurface> SkiaGLImageRepresentation::BeginWriteAccess(
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
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

sk_sp<SkPromiseImageTexture> SkiaGLImageRepresentation::BeginWriteAccess(
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

void SkiaGLImageRepresentation::EndWriteAccess(sk_sp<SkSurface> surface) {
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

sk_sp<SkPromiseImageTexture> SkiaGLImageRepresentation::BeginReadAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<GrBackendSurfaceMutableState>* end_state) {
  DCHECK_EQ(mode_, RepresentationAccessMode::kNone);
  CheckContext();

  if (!gl_representation_->BeginAccess(
          GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM)) {
    return nullptr;
  }
  mode_ = RepresentationAccessMode::kRead;
  return promise_texture_;
}

void SkiaGLImageRepresentation::EndReadAccess() {
  DCHECK_EQ(mode_, RepresentationAccessMode::kRead);
  CheckContext();

  gl_representation_->EndAccess();
  mode_ = RepresentationAccessMode::kNone;
}

void SkiaGLImageRepresentation::CheckContext() {
#if DCHECK_IS_ON()
  DCHECK_EQ(gl::GLContext::GetCurrent(), context_);
#endif
}

bool SkiaGLImageRepresentation::SupportsMultipleConcurrentReadAccess() {
  return gl_representation_->SupportsMultipleConcurrentReadAccess();
}

}  // namespace gpu
