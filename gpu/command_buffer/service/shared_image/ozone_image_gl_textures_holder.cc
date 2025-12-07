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

// Returns SharedImageFormat for given `format` and `plane_index`.
viz::SharedImageFormat GetFormatForPlane(viz::SharedImageFormat format,
                                         int plane_index) {
  DCHECK(format.IsValidPlaneIndex(plane_index));
  int num_channels = format.NumChannelsInPlane(plane_index);
  DCHECK_LE(num_channels, 2);
  switch (format.channel_format()) {
    case viz::SharedImageFormat::ChannelFormat::k8:
      return num_channels == 2 ? viz::SinglePlaneFormat::kRG_88
                               : viz::SinglePlaneFormat::kR_8;
    case viz::SharedImageFormat::ChannelFormat::k10:
    case viz::SharedImageFormat::ChannelFormat::k16:
    case viz::SharedImageFormat::ChannelFormat::k16F:
      return num_channels == 2 ? viz::SinglePlaneFormat::kRG_1616
                               : viz::SinglePlaneFormat::kR_16;
  }
  NOTREACHED();
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
  NOTREACHED();
}

// Create a NativePixmapGLBinding for the given `pixmap`. On failure, returns
// nullptr.
std::unique_ptr<ui::NativePixmapGLBinding> GetBinding(
    scoped_refptr<gfx::NativePixmap> pixmap,
    viz::SharedImageFormat format,
    int plane_index,
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

  // Get the plane format and plane size using utility methods for multiplanar
  // formats.
  viz::SharedImageFormat plane_format = format;
  gfx::Size plane_size;
  // The plane is DEFAULT for single planar formats and multi planar with
  // external sampler.
  gfx::BufferPlane buffer_plane;
  if (format.is_single_plane() || format.PrefersExternalSampler()) {
    plane_size = size;
    buffer_plane = gfx::BufferPlane::DEFAULT;
  } else {
    plane_format = GetFormatForPlane(format, plane_index);
    plane_size = format.GetPlaneSize(plane_index, size);
    buffer_plane = GetBufferPlane(format, plane_index);
  }

  // The target should be GL_TEXTURE_2D unless external sampling is being
  // used, which in this context is equivalent to the passed-in buffer format
  // being multiplanar (if using per-plane sampling of a multiplanar texture,
  // the buffer format passed in here must be the single-planar format of the
  // plane).
  if (format.PrefersExternalSampler()) {
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
      gl_ozone->ImportNativePixmap(pixmap, plane_format, buffer_plane,
                                   plane_size, color_space, target,
                                   gl_texture_service_id);
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
  if (!holder->CreateAndStoreTexture(backing, std::move(pixmap))) {
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

bool OzoneImageGLTexturesHolder::CreateAndStoreTexture(
    SharedImageBacking* backing,
    scoped_refptr<gfx::NativePixmap> pixmap) {
  DCHECK(backing && pixmap);
  auto format = backing->format();
  auto size = backing->size();
  // Initialize the holder with N textures using format and plane.
  for (int plane_index = 0; plane_index < format.NumberOfPlanes();
       plane_index++) {
    GLenum target;
    GLuint gl_texture_service_id;
    std::unique_ptr<ui::NativePixmapGLBinding> np_gl_binding =
        GetBinding(pixmap, format, plane_index, size, backing->color_space(),
                   gl_texture_service_id, target);
    if (!np_gl_binding) {
      return false;
    }

    textures_.push_back(base::MakeRefCounted<gpu::gles2::TexturePassthrough>(
        gl_texture_service_id, target));

    bindings_.push_back(std::move(np_gl_binding));
  }
  return true;
}

}  // namespace gpu
