// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/gl_texture_common_representations.h"

#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_sizes.h"
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
    gles2::Texture* texture)
    : GLTextureImageRepresentation(manager, backing, tracker),
      client_(client),
      texture_(texture) {}

GLTextureGLCommonRepresentation::~GLTextureGLCommonRepresentation() {
  texture_ = nullptr;
  if (client_)
    client_->GLTextureImageRepresentationRelease(has_context());
}

gles2::Texture* GLTextureGLCommonRepresentation::GetTexture(int plane_index) {
  DCHECK_EQ(plane_index, 0);
  return texture_;
}

bool GLTextureGLCommonRepresentation::BeginAccess(GLenum mode) {
  DCHECK(mode_ == 0);
  mode_ = mode;
  bool readonly = mode_ != GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM;
  if (client_ && mode != GL_SHARED_IMAGE_ACCESS_MODE_OVERLAY_CHROMIUM)
    return client_->GLTextureImageRepresentationBeginAccess(readonly);
  return true;
}

void GLTextureGLCommonRepresentation::EndAccess() {
  DCHECK(mode_ != 0);
  GLenum current_mode = mode_;
  mode_ = 0;
  if (client_)
    return client_->GLTextureImageRepresentationEndAccess(
        current_mode != GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
}

///////////////////////////////////////////////////////////////////////////////
// GLTexturePassthroughGLCommonRepresentation

GLTexturePassthroughGLCommonRepresentation::
    GLTexturePassthroughGLCommonRepresentation(
        SharedImageManager* manager,
        SharedImageBacking* backing,
        GLTextureImageRepresentationClient* client,
        MemoryTypeTracker* tracker,
        scoped_refptr<gles2::TexturePassthrough> texture_passthrough)
    : GLTexturePassthroughImageRepresentation(manager, backing, tracker),
      client_(client),
      texture_passthrough_(std::move(texture_passthrough)) {
  // TODO(https://crbug.com/1172769): Remove this CHECK.
  CHECK(texture_passthrough_);
}

GLTexturePassthroughGLCommonRepresentation::
    ~GLTexturePassthroughGLCommonRepresentation() {
  texture_passthrough_.reset();
  if (client_)
    client_->GLTextureImageRepresentationRelease(has_context());
}

const scoped_refptr<gles2::TexturePassthrough>&
GLTexturePassthroughGLCommonRepresentation::GetTexturePassthrough(
    int plane_index) {
  DCHECK_EQ(plane_index, 0);
  return texture_passthrough_;
}

bool GLTexturePassthroughGLCommonRepresentation::BeginAccess(GLenum mode) {
  DCHECK(mode_ == 0);
  mode_ = mode;
  bool readonly = mode_ != GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM;
  if (client_ && mode != GL_SHARED_IMAGE_ACCESS_MODE_OVERLAY_CHROMIUM)
    return client_->GLTextureImageRepresentationBeginAccess(readonly);
  return true;
}

void GLTexturePassthroughGLCommonRepresentation::EndAccess() {
  DCHECK(mode_ != 0);
  GLenum current_mode = mode_;
  mode_ = 0;
  if (client_)
    return client_->GLTextureImageRepresentationEndAccess(
        current_mode != GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
}

///////////////////////////////////////////////////////////////////////////////
// SkiaGLCommonRepresentation

SkiaGLCommonRepresentation::SkiaGLCommonRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    GLTextureImageRepresentationClient* client,
    scoped_refptr<SharedContextState> context_state,
    sk_sp<SkPromiseImageTexture> promise_texture,
    MemoryTypeTracker* tracker)
    : SkiaImageRepresentation(manager, backing, tracker),
      client_(client),
      context_state_(std::move(context_state)),
      promise_texture_(promise_texture) {
  DCHECK(promise_texture_);
#if DCHECK_IS_ON()
  if (context_state_->GrContextIsGL())
    context_ = gl::GLContext::GetCurrent();
#endif
}

SkiaGLCommonRepresentation::~SkiaGLCommonRepresentation() {
  if (write_surface_) {
    DLOG(ERROR) << "SkiaImageRepresentation was destroyed while still "
                << "open for write access.";
  }
  promise_texture_.reset();
  if (client_) {
    DCHECK(context_state_->GrContextIsGL());
    client_->GLTextureImageRepresentationRelease(has_context());
  }
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

  if (write_surface_)
    return {};

  if (!promise_texture_)
    return {};

  SkColorType sk_color_type = viz::ToClosestSkColorType(
      /*gpu_compositing=*/true, format());
  // Gray is not a renderable single channel format, but alpha is.
  if (sk_color_type == kGray_8_SkColorType)
    sk_color_type = kAlpha_8_SkColorType;
  auto surface = SkSurface::MakeFromBackendTexture(
      context_state_->gr_context(), promise_texture_->backendTexture(),
      surface_origin(), final_msaa_count, sk_color_type,
      backing()->color_space().GetAsFullRangeRGB().ToSkColorSpace(),
      &surface_props);
  write_surface_ = surface;

  if (!surface)
    return {};
  return {surface};
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

  if (!promise_texture_)
    return {};
  return {promise_texture_};
}

void SkiaGLCommonRepresentation::EndWriteAccess() {
  if (write_surface_) {
    DCHECK(write_surface_->unique());
    CheckContext();
    // TODO(ericrk): Keep the surface around for re-use.
    write_surface_.reset();
  }

  if (client_)
    client_->GLTextureImageRepresentationEndAccess(false /* readonly */);
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

  if (!promise_texture_)
    return {};
  return {promise_texture_};
}

void SkiaGLCommonRepresentation::EndReadAccess() {
  if (client_)
    client_->GLTextureImageRepresentationEndAccess(true /* readonly */);
}

bool SkiaGLCommonRepresentation::SupportsMultipleConcurrentReadAccess() {
  return true;
}

void SkiaGLCommonRepresentation::CheckContext() {
#if DCHECK_IS_ON()
  if (!context_state_->context_lost() && context_)
    DCHECK(gl::GLContext::GetCurrent() == context_);
#endif
}

}  // namespace gpu
