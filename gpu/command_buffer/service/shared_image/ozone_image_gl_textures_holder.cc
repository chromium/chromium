// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/ozone_image_gl_textures_holder.h"

#include <memory>

#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/scoped_binders.h"
#include "ui/ozone/public/gl_ozone.h"
#include "ui/ozone/public/native_pixmap_gl_binding.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace gpu {

namespace {

// Returns BufferFormat for given `format` and `plane_index`.
gfx::BufferFormat GetBufferFormatForPlane(viz::SharedImageFormat format,
                                          int plane_index) {
  DCHECK(format.IsValidPlaneIndex(plane_index));
  int num_channels = format.NumChannelsInPlane(plane_index);
  DCHECK_LE(num_channels, 2);
  switch (format.channel_format()) {
    case viz::SharedImageFormat::ChannelFormat::k8:
      return num_channels == 2 ? gfx::BufferFormat::RG_88
                               : gfx::BufferFormat::R_8;
    case viz::SharedImageFormat::ChannelFormat::k10:
    case viz::SharedImageFormat::ChannelFormat::k16:
    case viz::SharedImageFormat::ChannelFormat::k16F:
      return num_channels == 2 ? gfx::BufferFormat::RG_1616
                               : gfx::BufferFormat::R_16;
  }
  NOTREACHED_IN_MIGRATION();
  return gfx::BufferFormat::RGBA_8888;
}

gfx::BufferPlane GetBufferPlane(viz::SharedImageFormat format,
                                int plane_index) {
  DCHECK(format.IsValidPlaneIndex(plane_index));
  switch (format.plane_config()) {
    case viz::SharedImageFormat::PlaneConfig::kY_U_V:
      switch (plane_index) {
        case 0:
          return gfx::BufferPlane::Y;
        case 1:
          return gfx::BufferPlane::U;
        case 2:
          return gfx::BufferPlane::V;
      }
    case viz::SharedImageFormat::PlaneConfig::kY_V_U:
      switch (plane_index) {
        case 0:
          return gfx::BufferPlane::Y;
        case 1:
          return gfx::BufferPlane::V;
        case 2:
          return gfx::BufferPlane::U;
      }
    case viz::SharedImageFormat::PlaneConfig::kY_UV:
      switch (plane_index) {
        case 0:
          return gfx::BufferPlane::Y;
        case 1:
          return gfx::BufferPlane::UV;
      }
    case viz::SharedImageFormat::PlaneConfig::kY_UV_A:
      switch (plane_index) {
        case 0:
          return gfx::BufferPlane::Y;
        case 1:
          return gfx::BufferPlane::UV;
        case 2:
          return gfx::BufferPlane::A;
      }
    case viz::SharedImageFormat::PlaneConfig::kY_U_V_A:
      switch (plane_index) {
        case 0:
          return gfx::BufferPlane::Y;
        case 1:
          return gfx::BufferPlane::U;
        case 2:
          return gfx::BufferPlane::V;
        case 3:
          return gfx::BufferPlane::A;
      }
  }
  NOTREACHED_IN_MIGRATION();
  return gfx::BufferPlane::DEFAULT;
}

// Create a NativePixmapGLBinding for the given `pixmap`. On failure, returns
// nullptr.
std::unique_ptr<ui::NativePixmapGLBinding> GetBinding(
    scoped_refptr<gfx::NativePixmap> pixmap,
    gfx::BufferFormat buffer_format,
    gfx::BufferPlane buffer_plane,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GLuint& gl_texture_service_id,
    GLenum& target) {
  ui::GLOzone* gl_ozone = ui::OzonePlatform::GetInstance()
                              ->GetSurfaceFactoryOzone()
                              ->GetCurrentGLOzone();
  if (!gl_ozone) {
    LOG(FATAL) << "Failed to get GLOzone.";
  }

  // The target should be GL_TEXTURE_2D unless external sampling is being
  // used, which in this context is equivalent to the passed-in buffer format
  // being multiplanar (if using per-plane sampling of a multiplanar texture,
  // the buffer format passed in here must be the single-planar format of the
  // plane).
  if (gfx::BufferFormatIsMultiplanar(buffer_format)) {
    CHECK_EQ(buffer_plane, gfx::BufferPlane::DEFAULT);
    target = GL_TEXTURE_EXTERNAL_OES;
  } else {
    target = GL_TEXTURE_2D;
  }

  gl::GLApi* api = gl::g_current_gl_context;
  DCHECK(api);
  api->glGenTexturesFn(1, &gl_texture_service_id);

  gl::ScopedTextureBinder binder(target, gl_texture_service_id);
  api->glTexParameteriFn(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  api->glTexParameteriFn(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  api->glTexParameteriFn(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  api->glTexParameteriFn(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  std::unique_ptr<ui::NativePixmapGLBinding> np_gl_binding =
      gl_ozone->ImportNativePixmap(pixmap, buffer_format, buffer_plane, size,
                                   color_space, target, gl_texture_service_id);
  if (!np_gl_binding) {
    DLOG(ERROR) << "Failed to create NativePixmapGLBinding.";
    api->glDeleteTexturesFn(1, &gl_texture_service_id);
    return nullptr;
  }

  return np_gl_binding;
}

}  // namespace

// static
scoped_refptr<OzoneImageGLTexturesHolder>
OzoneImageGLTexturesHolder::CreateAndInitTexturesHolder(
    SharedImageBacking* backing,
    scoped_refptr<gfx::NativePixmap> pixmap) {
  scoped_refptr<OzoneImageGLTexturesHolder> holder =
      base::WrapRefCounted(new OzoneImageGLTexturesHolder());
  if (!holder->Initialize(backing, std::move(pixmap))) {
    holder.reset();
  }
  return holder;
}

OzoneImageGLTexturesHolder::OzoneImageGLTexturesHolder() = default;

OzoneImageGLTexturesHolder::~OzoneImageGLTexturesHolder() = default;

void OzoneImageGLTexturesHolder::MarkContextLost() {
  if (WasContextLost()) {
    return;
  }

  context_lost_ = true;
  for (auto& texture : textures_) {
    texture->MarkContextLost();
  }
}

bool OzoneImageGLTexturesHolder::WasContextLost() {
  return context_lost_;
}

void OzoneImageGLTexturesHolder::OnAddedToCache() {
  (++cache_count_).ValueOrDie();
}

void OzoneImageGLTexturesHolder::OnRemovedFromCache() {
  (--cache_count_).ValueOrDie();
}

size_t OzoneImageGLTexturesHolder::GetCacheCount() const {
  return cache_count_.ValueOrDie();
}

void OzoneImageGLTexturesHolder::DestroyTextures() {
  textures_.clear();
  bindings_.clear();
}

size_t OzoneImageGLTexturesHolder::GetNumberOfTextures() const {
  return textures_.size();
}

bool OzoneImageGLTexturesHolder::Initialize(
    SharedImageBacking* backing,
    scoped_refptr<gfx::NativePixmap> pixmap) {
  DCHECK(backing && pixmap);
  const viz::SharedImageFormat format = backing->format();
  if (format.is_single_plane() || format.PrefersExternalSampler()) {
    // Initialize the holder with a single texture with format of the
    // NativePixmap, size of the backing and DEFAULT plane.
    auto size = backing->size();
    auto buffer_format = pixmap->GetBufferFormat();
    return CreateAndStoreTexture(backing, std::move(pixmap), buffer_format,
                                 gfx::BufferPlane::DEFAULT, size);
  } else {
    // Initialize the holder with N textures with format using
    // GetBufferFormatForPlane(), size using GetPlaneSize() and plane using
    // GetBufferPlane()
    for (int plane_index = 0; plane_index < format.NumberOfPlanes();
         plane_index++) {
      auto size = format.GetPlaneSize(plane_index, backing->size());
      auto buffer_format = GetBufferFormatForPlane(format, plane_index);
      auto buffer_plane = GetBufferPlane(format, plane_index);
      if (!CreateAndStoreTexture(backing, pixmap, buffer_format, buffer_plane,
                                 size)) {
        return false;
      }
    }
  }
  return true;
}

bool OzoneImageGLTexturesHolder::CreateAndStoreTexture(
    SharedImageBacking* backing,
    scoped_refptr<gfx::NativePixmap> pixmap,
    gfx::BufferFormat buffer_format,
    gfx::BufferPlane buffer_plane,
    const gfx::Size& size) {
  GLenum target;
  GLuint gl_texture_service_id;
  std::unique_ptr<ui::NativePixmapGLBinding> np_gl_binding =
      GetBinding(pixmap, buffer_format, buffer_plane, size,
                 backing->color_space(), gl_texture_service_id, target);
  if (!np_gl_binding) {
    return false;
  }

  textures_.push_back(base::MakeRefCounted<gpu::gles2::TexturePassthrough>(
      gl_texture_service_id, target));

  bindings_.push_back(std::move(np_gl_binding));
  return true;
}

}  // namespace gpu
