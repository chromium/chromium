// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/skia_gl_image_representation.h"

#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrContextThreadSafeProxy.h"
#include "third_party/skia/include/gpu/ganesh/SkSurfaceGanesh.h"
#include "third_party/skia/include/private/chromium/GrPromiseImageTexture.h"
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
  std::vector<sk_sp<GrPromiseImageTexture>> promise_textures;
  auto format = backing->format();
  GLFormatCaps caps = context_state->GetGLFormatCaps();

  if (format.is_single_plane() || format.PrefersExternalSampler()) {
    GrBackendTexture backend_texture;
    GLFormatDesc gl_format_desc =
        format.PrefersExternalSampler()
            ? caps.ToGLFormatDescExternalSampler(format)
            : caps.ToGLFormatDesc(format, /*plane_index=*/0);
    if (!GetGrBackendTexture(
            context_state->feature_info(),
            gl_representation->GetTextureBase()->target(), backing->size(),
            gl_representation->GetTextureBase()->service_id(),
            gl_format_desc.storage_internal_format,
            context_state->gr_context()->threadSafeProxy(), &backend_texture)) {
      return nullptr;
    }
    auto promise_texture = GrPromiseImageTexture::Make(backend_texture);
    if (!promise_texture)
      return nullptr;
    promise_textures.push_back(std::move(promise_texture));
  } else {
    for (int plane_index = 0; plane_index < format.NumberOfPlanes();
         plane_index++) {
      GrBackendTexture backend_texture;
      // Use the format and size per plane for multiplanar formats.
      GLFormatDesc format_desc = caps.ToGLFormatDesc(format, plane_index);
      gfx::Size plane_size = format.GetPlaneSize(plane_index, backing->size());
      if (!GetGrBackendTexture(
              context_state->feature_info(),
              gl_representation->GetTextureBase(plane_index)->target(),
              plane_size,
              gl_representation->GetTextureBase(plane_index)->service_id(),
              format_desc.storage_internal_format,
              context_state->gr_context()->threadSafeProxy(),
              &backend_texture)) {
        return nullptr;
      }
      auto promise_texture = GrPromiseImageTexture::Make(backend_texture);
      if (!promise_texture)
        return nullptr;
      promise_textures.push_back(std::move(promise_texture));
    }
  }

  return base::WrapUnique(new SkiaGLImageRepresentation(
      std::move(gl_representation), std::move(promise_textures),
      std::move(context_state), manager, backing, tracker));
}

SkiaGLImageRepresentation::SkiaGLImageRepresentation(
    std::unique_ptr<GLTextureImageRepresentationBase> gl_representation,
    std::vector<sk_sp<GrPromiseImageTexture>> promise_textures,
    scoped_refptr<SharedContextState> context_state,
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker)
    : SkiaGaneshImageRepresentation(context_state->gr_context(),
                                    manager,
                                    backing,
                                    tracker),
      gl_representation_(std::move(gl_representation)),
      promise_textures_(std::move(promise_textures)),
      context_state_(std::move(context_state)) {
  DCHECK(gl_representation_);
#if DCHECK_IS_ON()
  context_ = gl::GLContext::GetCurrent();
#endif
}

SkiaGLImageRepresentation::~SkiaGLImageRepresentation() {
  DCHECK_EQ(RepresentationAccessMode::kNone, mode_);
  ClearCachedSurfaces();

  DCHECK_EQ(!has_context(), context_state_->context_lost());
  if (!has_context())
    gl_representation_->OnContextLost();
}

std::vector<sk_sp<SkSurface>> SkiaGLImageRepresentation::BeginWriteAccess(
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    const gfx::Rect& update_rect,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<skgpu::MutableTextureState>* end_state) {
  DCHECK_EQ(mode_, RepresentationAccessMode::kNone);
  CheckContext();

  if (!gl_representation_->BeginAccess(
          GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM)) {
    return {};
  }

  if (!surfaces_.empty()) {
    mode_ = RepresentationAccessMode::kWrite;
    return surfaces_;
  }

  DCHECK_EQ(static_cast<int>(promise_textures_.size()),
            format().NumberOfPlanes());
  std::vector<sk_sp<SkSurface>> surfaces;
  for (int plane_index = 0; plane_index < format().NumberOfPlanes();
       plane_index++) {
    // Use the color type per plane for multiplanar formats.
    SkColorType sk_color_type = viz::ToClosestSkColorType(
        /*gpu_compositing=*/true, format(), plane_index);
    auto surface = SkSurfaces::WrapBackendTexture(
        context_state_->gr_context(),
        promise_textures_[plane_index]->backendTexture(), surface_origin(),
        final_msaa_count, sk_color_type,
        backing()->color_space().ToSkColorSpace(), &surface_props);
    if (!surface)
      return {};
    surfaces.push_back(surface);
  }

  mode_ = RepresentationAccessMode::kWrite;
  surfaces_ = surfaces;
  return surfaces;
}

std::vector<sk_sp<GrPromiseImageTexture>>
SkiaGLImageRepresentation::BeginWriteAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<skgpu::MutableTextureState>* end_state) {
  DCHECK_EQ(mode_, RepresentationAccessMode::kNone);
  CheckContext();

  if (!gl_representation_->BeginAccess(
          GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM)) {
    return {};
  }
  mode_ = RepresentationAccessMode::kWrite;
  return promise_textures_;
}

void SkiaGLImageRepresentation::EndWriteAccess() {
  DCHECK_EQ(mode_, RepresentationAccessMode::kWrite);
  for (auto& surface : surfaces_)
    DCHECK(surface->unique());

  gl_representation_->EndAccess();
  mode_ = RepresentationAccessMode::kNone;
}

std::vector<sk_sp<GrPromiseImageTexture>>
SkiaGLImageRepresentation::BeginReadAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<skgpu::MutableTextureState>* end_state) {
  DCHECK_EQ(mode_, RepresentationAccessMode::kNone);
  CheckContext();

  if (!gl_representation_->BeginAccess(
          GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM)) {
    return {};
  }
  mode_ = RepresentationAccessMode::kRead;
  return promise_textures_;
}

void SkiaGLImageRepresentation::EndReadAccess() {
  DCHECK_EQ(mode_, RepresentationAccessMode::kRead);
  CheckContext();

  gl_representation_->EndAccess();
  mode_ = RepresentationAccessMode::kNone;
}

void SkiaGLImageRepresentation::ClearCachedSurfaces() {
  surfaces_.clear();
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
