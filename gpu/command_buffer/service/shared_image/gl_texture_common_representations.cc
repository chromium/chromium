// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/gl_texture_common_representations.h"

#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "ui/gl/gl_context.h"

namespace gpu {

///////////////////////////////////////////////////////////////////////////////
// GLTextureGLCommonRepresentation

// Representation of a GLTextureImageBacking as a GL Texture.
GLTextureGLCommonRepresentation::GLTextureGLCommonRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    GLTextureImageRepresentationClient* client,
    MemoryTypeTracker* tracker,
    std::vector<raw_ptr<gles2::Texture>> textures)
    : GLTextureImageRepresentation(manager, backing, tracker),
      client_(client),
      textures_(std::move(textures)) {
  DCHECK_EQ(textures_.size(), NumPlanesExpected());
}

GLTextureGLCommonRepresentation::~GLTextureGLCommonRepresentation() = default;

gles2::Texture* GLTextureGLCommonRepresentation::GetTexture(int plane_index) {
  DCHECK(format().IsValidPlaneIndex(plane_index));
  return textures_[plane_index];
}

bool GLTextureGLCommonRepresentation::BeginAccess(GLenum mode) {
  DCHECK_EQ(mode_, 0u);
  mode_ = mode;
  bool readonly = mode_ != GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM;
  if (client_) {
    return client_->GLTextureImageRepresentationBeginAccess(readonly);
  }
  return true;
}

void GLTextureGLCommonRepresentation::EndAccess() {
  DCHECK_NE(mode_, 0u);
  bool readonly = mode_ != GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM;
  mode_ = 0;
  if (client_) {
    return client_->GLTextureImageRepresentationEndAccess(readonly);
  }
}

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

///////////////////////////////////////////////////////////////////////////////
// SkiaGLCommonRepresentation

SkiaGLCommonRepresentation::SkiaGLCommonRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    GLTextureImageRepresentationClient* client,
    scoped_refptr<SharedContextState> context_state,
    std::vector<sk_sp<SkPromiseImageTexture>> promise_textures,
    MemoryTypeTracker* tracker)
    : SkiaImageRepresentation(manager, backing, tracker),
      client_(client),
      context_state_(std::move(context_state)),
      promise_textures_(std::move(promise_textures)) {
  DCHECK_EQ(promise_textures_.size(), NumPlanesExpected());
#if DCHECK_IS_ON()
  if (context_state_->GrContextIsGL())
    context_ = gl::GLContext::GetCurrent();
#endif
}

SkiaGLCommonRepresentation::~SkiaGLCommonRepresentation() {
  DCHECK(write_surfaces_.empty());
}

std::vector<sk_sp<SkSurface>> SkiaGLCommonRepresentation::BeginWriteAccess(
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    const gfx::Rect& update_rect,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<GrBackendSurfaceMutableState>* end_state) {
  CheckContext();
  if (client_) {
    DCHECK(context_state_->GrContextIsGL());
    if (!client_->GLTextureImageRepresentationBeginAccess(
            /*readonly=*/false)) {
      return {};
    }
  }

  if (!write_surfaces_.empty()) {
    // Write access is already in progress.
    return {};
  }

  for (int plane = 0; plane < format().NumberOfPlanes(); ++plane) {
    SkColorType sk_color_type = viz::ToClosestSkColorType(
        /*gpu_compositing=*/true, format(), plane);
    // Gray is not a renderable single channel format, but alpha is.
    if (sk_color_type == kGray_8_SkColorType) {
      sk_color_type = kAlpha_8_SkColorType;
    }
    auto surface = SkSurface::MakeFromBackendTexture(
        context_state_->gr_context(),
        promise_textures_[plane]->backendTexture(), surface_origin(),
        final_msaa_count, sk_color_type,
        backing()->color_space().GetAsFullRangeRGB().ToSkColorSpace(),
        &surface_props);
    if (!surface) {
      write_surfaces_.clear();
      return {};
    }
    write_surfaces_.push_back(std::move(surface));
  }

  return write_surfaces_;
}

std::vector<sk_sp<SkPromiseImageTexture>>
SkiaGLCommonRepresentation::BeginWriteAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<GrBackendSurfaceMutableState>* end_state) {
  CheckContext();
  if (client_) {
    DCHECK(context_state_->GrContextIsGL());
    if (!client_->GLTextureImageRepresentationBeginAccess(
            /*readonly=*/false)) {
      return {};
    }
  }

  return promise_textures_;
}

void SkiaGLCommonRepresentation::EndWriteAccess() {
  if (!write_surfaces_.empty()) {
#if DCHECK_IS_ON()
    for (auto& write_surface : write_surfaces_) {
      DCHECK(write_surface->unique());
    }
    CheckContext();
#endif
    write_surfaces_.clear();
  }

  if (client_) {
    client_->GLTextureImageRepresentationEndAccess(/*readonly=*/false);
  }
}

std::vector<sk_sp<SkPromiseImageTexture>>
SkiaGLCommonRepresentation::BeginReadAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<GrBackendSurfaceMutableState>* end_state) {
  CheckContext();
  if (client_) {
    DCHECK(context_state_->GrContextIsGL());
    if (!client_->GLTextureImageRepresentationBeginAccess(
            /*readonly=*/true)) {
      return {};
    }
  }

  return promise_textures_;
}

void SkiaGLCommonRepresentation::EndReadAccess() {
  if (client_) {
    client_->GLTextureImageRepresentationEndAccess(/*readonly=*/true);
  }
}

bool SkiaGLCommonRepresentation::SupportsMultipleConcurrentReadAccess() {
  return true;
}

void SkiaGLCommonRepresentation::CheckContext() {
#if DCHECK_IS_ON()
  if (!context_state_->context_lost() && context_) {
    DCHECK(gl::GLContext::GetCurrent() == context_);
  }
#endif
}

}  // namespace gpu
