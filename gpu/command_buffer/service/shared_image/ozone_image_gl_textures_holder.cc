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
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/scoped_binders.h"
#include "ui/ozone/public/gl_ozone.h"
#include "ui/ozone/public/native_pixmap_gl_binding.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace gpu {

namespace {
// Converts a value that is aligned with glTexImage{2|3}D's |internalformat|
// parameter to the value that is correspondingly aligned with
// glTexImage{2|3}D's |format| parameter. |internalformat| is mostly an unsized
// format that can be used both as internal format and data format. However,
// GL_EXT_texture_norm16 follows ES3 semantics and only exposes a sized
// internalformat.
unsigned GetDataFormatFromInternalFormat(unsigned internalformat) {
  switch (internalformat) {
    case GL_R16_EXT:
      return GL_RED_EXT;
    case GL_RG16_EXT:
      return GL_RG_EXT;
    case GL_RGB10_A2_EXT:
      return GL_RGBA;
    case GL_RGB_YCRCB_420_CHROMIUM:
    case GL_RGB_YCBCR_420V_CHROMIUM:
    case GL_RGB_YCBCR_P010_CHROMIUM:
      return GL_RGB;
    case GL_RGBA16F_EXT:
      return GL_RGBA;
    case GL_RED:
    case GL_RG:
    case GL_RGB:
    case GL_RGBA:
    case GL_BGRA_EXT:
      return internalformat;
    default:
      NOTREACHED();
      return GL_NONE;
  }
}

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
  NOTREACHED();
  return gfx::BufferFormat::RGBA_8888;
}

gfx::BufferPlane GetBufferPlane(viz::SharedImageFormat format,
                                int plane_index) {
  DCHECK(format.IsValidPlaneIndex(plane_index));
  switch (format.plane_config()) {
    case viz::SharedImageFormat::PlaneConfig::kY_U_V:
    case viz::SharedImageFormat::PlaneConfig::kY_V_U:
      switch (plane_index) {
        case 0:
          return gfx::BufferPlane::Y;
        case 1:
          return gfx::BufferPlane::U;
        case 2:
          return gfx::BufferPlane::V;
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
  }
  NOTREACHED();
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
    return nullptr;
  }

  target = !NativeBufferNeedsPlatformSpecificTextureTarget(buffer_format,
                                                           buffer_plane)
               ? GL_TEXTURE_2D
               : gpu::GetPlatformSpecificTextureTarget();

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
    gl::GLContext* current_context,
    SharedImageBacking* backing,
    scoped_refptr<gfx::NativePixmap> pixmap,
    gfx::BufferPlane plane,
    bool is_passthrough) {
  // See more details in the constructor below.
  if (current_context) {
    DCHECK_EQ(current_context, gl::GLContext::GetCurrent());
  } else {
    DCHECK(!current_context);
  }
  scoped_refptr<OzoneImageGLTexturesHolder> holder = base::WrapRefCounted(
      new OzoneImageGLTexturesHolder(is_passthrough, current_context));
  if (!holder->Initialize(backing, std::move(pixmap), plane)) {
    holder.reset();
  }
  return holder;
}

OzoneImageGLTexturesHolder::OzoneImageGLTexturesHolder(
    bool is_passthrough,
    gl::GLContext* current_context)
    : context_(current_context), is_passthrough_(is_passthrough) {
  // Only the context that was initialized with an offscreen surface must be
  // passed here. That is, the texture holder can be per-context cached by the
  // OzoneImageBacking. In that case, the textures are shared between image
  // representations and their destruction can happen on a wrong context. To
  // avoid that, a correct context is passed here and it's made current so that
  // textures are destroyed on the same context they were created.
  DCHECK(!context_ || context_->default_surface());
}

OzoneImageGLTexturesHolder::~OzoneImageGLTexturesHolder() {
  // If the context was lost or the textures have already been destroyed, there
  // is no need to call |MaybeDestroyTexturesOnContext| as it will unnecessary
  // switch contexts.
  if (WasContextLost() ||
      (textures_passthrough_.empty() && textures_.empty())) {
    return;
  }

  MaybeDestroyTexturesOnContext();
}

void OzoneImageGLTexturesHolder::OnContextWillDestroy(gl::GLContext* context) {
  // It's not expected that this is called if the |context_| is null.
  DCHECK(context_);
  DCHECK_EQ(context_, context);
  MaybeDestroyTexturesOnContext();
  context_ = nullptr;
  context_destroyed_ = true;
}

void OzoneImageGLTexturesHolder::MaybeDestroyTexturesOnContext() {
  if (!context_) {
    if (context_destroyed_) {
      DCHECK(textures_.empty() && textures_passthrough_.empty());
    }
    return;
  }

  gl::GLContext* prev_current_context = gl::GLContext::GetCurrent();
  gl::GLSurface* prev_current_surface = gl::GLSurface::GetCurrent();

  context_->MakeCurrentDefault();

  textures_passthrough_.clear();
  textures_.clear();
  bindings_.clear();

  if (prev_current_context) {
    DCHECK(prev_current_surface);
    prev_current_context->MakeCurrent(prev_current_surface);
  } else {
    context_->ReleaseCurrent(nullptr);
  }
}

void OzoneImageGLTexturesHolder::MarkContextLost() {
  if (WasContextLost()) {
    return;
  }

  context_lost_ = true;
  for (auto& texture : textures_passthrough_) {
    texture->MarkContextLost();
  }
}

bool OzoneImageGLTexturesHolder::WasContextLost() {
  return context_lost_;
}

size_t OzoneImageGLTexturesHolder::GetNumberOfTextures() const {
  if (is_passthrough_) {
    DCHECK(textures_.empty());
    return textures_passthrough_.size();
  }
  DCHECK(textures_passthrough_.empty());
  return textures_.size();
}

bool OzoneImageGLTexturesHolder::Initialize(
    SharedImageBacking* backing,
    scoped_refptr<gfx::NativePixmap> pixmap,
    gfx::BufferPlane plane) {
  DCHECK(backing && pixmap);
  const viz::SharedImageFormat format = backing->format();
  if (format.is_single_plane()) {
    // Initialize the holder with a single texture with format and size of the
    // backing. For legacy multiplanar formats, the plane must be DEFAULT.
    auto size = backing->size();
    auto buffer_format = ToBufferFormat(format);
    if (format.IsLegacyMultiplanar()) {
      DCHECK_EQ(plane, gfx::BufferPlane::DEFAULT);
    }
    auto buffer_plane = plane;
    return CreateAndStoreTexture(backing, std::move(pixmap), buffer_format,
                                 buffer_plane, size);
  } else if (format.PrefersExternalSampler()) {
    // Initialize the holder with a single texture with format of the
    // NativePixmap, size of the backing and DEFAULT plane.
    auto size = backing->size();
    auto buffer_format = pixmap->GetBufferFormat();
    DCHECK_EQ(plane, gfx::BufferPlane::DEFAULT);
    auto buffer_plane = plane;
    return CreateAndStoreTexture(backing, std::move(pixmap), buffer_format,
                                 buffer_plane, size);
  } else {
    // Initialize the holder with N textures with format using
    // GetBufferFormatForPlane(), size using GetPlaneSize() and plane using
    // GetBufferPlane()
    DCHECK_EQ(plane, gfx::BufferPlane::DEFAULT);
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

  if (is_passthrough()) {
    textures_passthrough_.emplace_back(
        base::MakeRefCounted<gpu::gles2::TexturePassthrough>(
            gl_texture_service_id, target));
  } else {
    gles2::Texture* texture =
        gles2::CreateGLES2TextureWithLightRef(gl_texture_service_id, target);

    // TODO(crbug.com/1468989): Make sure these match with corresponding formats
    // from ToGLFormatDesc{ExternalSampler}.
    GLuint internal_format = np_gl_binding->GetInternalFormat();
    GLenum gl_format = GetDataFormatFromInternalFormat(internal_format);
    GLenum gl_type = np_gl_binding->GetDataType();
    texture->SetLevelInfo(target, 0, internal_format, backing->size().width(),
                          backing->size().height(), 1, 0, gl_format, gl_type,
                          backing->ClearedRect());
    texture->SetImmutable(true, true);

    textures_.emplace_back(texture);
  }

  bindings_.emplace_back(std::move(np_gl_binding));
  return true;
}

}  // namespace gpu
