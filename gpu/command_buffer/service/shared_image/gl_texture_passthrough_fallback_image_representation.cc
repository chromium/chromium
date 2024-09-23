// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/gl_texture_passthrough_fallback_image_representation.h"

#include "base/bits.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_gl_utils.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/progress_reporter.h"
#include "ui/gl/scoped_restore_texture.h"

namespace gpu {
namespace {
GLFormatDesc GetGLFormatDesc(viz::SharedImageFormat format,
                             int plane_index,
                             const GLFormatCaps& gl_format_caps) {
  GLFormatDesc gl_format_desc;
  if (format.is_multi_plane()) {
    gl_format_desc = gl_format_caps.ToGLFormatDesc(format, plane_index);
  } else {
    // For legacy multiplanar formats, `format` is already plane format (eg.
    // RED, RG), so we pass plane_index=0.
    gl_format_desc = gl_format_caps.ToGLFormatDesc(format, /*plane_index=*/0);
  }
  return gl_format_desc;
}

scoped_refptr<gles2::TexturePassthrough> CreateGLTexture(
    const GLFormatDesc& format_desc,
    const gfx::Size& size,
    gl::ProgressReporter* progress_reporter) {
  gl::GLApi* const api = gl::g_current_gl_context;
  const GLenum target = format_desc.target;
  gl::ScopedRestoreTexture scoped_restore(api, target);

  GLuint service_id = 0;
  api->glGenTexturesFn(1, &service_id);
  api->glBindTextureFn(target, service_id);

  // These need to be set for the texture to be considered mipmap complete.
  api->glTexParameteriFn(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  api->glTexParameteriFn(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  // These are not strictly required but guard against some checks if NPOT
  // texture support is disabled.
  api->glTexParameteriFn(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  api->glTexParameteriFn(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  gl::ScopedProgressReporter scoped_progress_reporter(progress_reporter);

  // Use glTexImage2D instead of glTexStorage2D so that we can later use
  // glTexSubImage2D in GLTextureHolder::UploadFromMemory.
  api->glTexImage2DFn(target, 0, format_desc.image_internal_format,
                      size.width(), size.height(), 0, format_desc.data_format,
                      format_desc.data_type, nullptr);

  return base::MakeRefCounted<gles2::TexturePassthrough>(service_id, target);
}
}  // namespace

GLTexturePassthroughFallbackImageRepresentation::
    GLTexturePassthroughFallbackImageRepresentation(
        SharedImageManager* manager,
        SharedImageBacking* backing,
        MemoryTypeTracker* tracker,
        gl::ProgressReporter* progress_reporter,
        const GLFormatCaps& gl_format_caps)
    : GLTexturePassthroughImageRepresentation(manager, backing, tracker) {
  for (int plane = 0; plane < format().NumberOfPlanes(); plane++) {
    const gfx::Size plane_size = format().GetPlaneSize(plane, size());
    const SkColorType plane_ct = viz::ToClosestSkColorType(
        /*gpu_compositing=*/true, format(), plane);
    const SkImageInfo plane_info = SkImageInfo::Make(
        plane_size.width(), plane_size.height(), plane_ct, kPremul_SkAlphaType);
    static constexpr size_t kDefaultGLAlignment = 4;
    plane_bitmaps_.emplace_back().allocPixels(
        plane_info,
        base::bits::AlignUp(plane_info.minRowBytes(), kDefaultGLAlignment));
    plane_pixmaps_.push_back(plane_bitmaps_.back().pixmap());

    const GLFormatDesc format_desc =
        GetGLFormatDesc(format(), plane, gl_format_caps);
    plane_textures_
        .emplace_back(viz::SkColorTypeToSinglePlaneSharedImageFormat(plane_ct),
                      plane_size, /*is_passthrough=*/true, progress_reporter)
        .InitializeWithTexture(
            format_desc,
            CreateGLTexture(format_desc, plane_size, progress_reporter));
  }
}

GLTexturePassthroughFallbackImageRepresentation::
    ~GLTexturePassthroughFallbackImageRepresentation() = default;

const scoped_refptr<gles2::TexturePassthrough>&
GLTexturePassthroughFallbackImageRepresentation::GetTexturePassthrough(
    int plane_index) {
  CHECK_GE(plane_index, 0);
  CHECK_LT(static_cast<size_t>(plane_index), plane_textures_.size());
  return plane_textures_[plane_index].passthrough_texture();
}

bool GLTexturePassthroughFallbackImageRepresentation::BeginAccess(GLenum mode) {
  // Only readback from backing if already cleared.
  if (IsCleared()) {
    if (!backing()->ReadbackToMemory(plane_pixmaps_)) {
      LOG(ERROR) << "Backing ReadbackToMemory failed";
      return false;
    }

    for (int plane = 0; plane < format().NumberOfPlanes(); plane++) {
      if (!plane_textures_[plane].UploadFromMemory(plane_pixmaps_[plane])) {
        LOG(ERROR) << "GL UploadFromMemory failed";
        return false;
      }
    }
  }
  return true;
}

void GLTexturePassthroughFallbackImageRepresentation::EndAccess() {
  for (int plane = 0; plane < format().NumberOfPlanes(); plane++) {
    if (!plane_textures_[plane].ReadbackToMemory(plane_pixmaps_[plane])) {
      LOG(ERROR) << "GL ReadbackToMemory failed";
      return;
    }
  }

  if (!backing()->UploadFromMemory(plane_pixmaps_)) {
    LOG(ERROR) << "Backing UploadFromMemory failed";
  }
}

}  // namespace gpu
