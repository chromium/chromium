// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/gl_ozone_image_representation.h"
#include <memory>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_image/ozone_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/trace_util.h"
#include "ui/ozone/public/gl_ozone.h"
#include "ui/ozone/public/native_pixmap_gl_binding.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

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

}  // namespace

namespace gpu {

using TextureHolder = GLOzoneImageRepresentationShared::TextureHolder;

TextureHolder::TextureHolder(std::unique_ptr<ui::NativePixmapGLBinding> binding,
                             gles2::Texture* texture)
    : binding_(std::move(binding)), texture_(texture) {}

TextureHolder::TextureHolder(
    std::unique_ptr<ui::NativePixmapGLBinding> binding,
    scoped_refptr<gles2::TexturePassthrough> texture_passthrough)
    : binding_(std::move(binding)),
      texture_passthrough_(std::move(texture_passthrough)) {}

TextureHolder::~TextureHolder() {
  if (texture_) {
    texture_.ExtractAsDangling()->RemoveLightweightRef(!context_lost_);
  }
}

void TextureHolder::MarkContextLost() {
  context_lost_ = true;
  if (texture_passthrough_)
    texture_passthrough_->MarkContextLost();
}

bool TextureHolder::WasContextLost() {
  return context_lost_;
}

// static
bool GLOzoneImageRepresentationShared::BeginAccess(
    GLenum mode,
    OzoneImageBacking* ozone_backing,
    bool& need_end_fence) {
  bool readonly = mode != GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM;
  std::vector<gfx::GpuFenceHandle> fences;
  if (!ozone_backing->BeginAccess(readonly,
                                  OzoneImageBacking::AccessStream::kGL, &fences,
                                  need_end_fence)) {
    return false;
  }

  // ChromeOS VMs don't support gpu fences, so there is no good way to
  // synchronize with GL.
  if (gl::GLFence::IsGpuFenceSupported()) {
    for (auto& fence : fences) {
      gfx::GpuFence gpu_fence = gfx::GpuFence(std::move(fence));
      std::unique_ptr<gl::GLFence> gl_fence =
          gl::GLFence::CreateFromGpuFence(gpu_fence);
      gl_fence->ServerWait();
    }
  }

  // We must call VaapiWrapper::SyncSurface() to ensure all VA-API work is done
  // prior to using the buffer in a graphics API.
  return ozone_backing->VaSync();
}

// static
void GLOzoneImageRepresentationShared::EndAccess(
    bool need_end_fence,
    GLenum mode,
    OzoneImageBacking* ozone_backing) {
  gfx::GpuFenceHandle fence;
  // ChromeOS VMs don't support gpu fences, so there is no good way to
  // synchronize with GL.
  if (gl::GLFence::IsGpuFenceSupported() && need_end_fence) {
    auto gl_fence = gl::GLFence::CreateForGpuFence();
    DCHECK(gl_fence);
    fence = gl_fence->GetGpuFence()->GetGpuFenceHandle().Clone();
  }
  bool readonly = mode != GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM;
  ozone_backing->EndAccess(readonly, OzoneImageBacking::AccessStream::kGL,
                           std::move(fence));
}

// static
std::unique_ptr<ui::NativePixmapGLBinding>
GLOzoneImageRepresentationShared::GetBinding(
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

// static
scoped_refptr<TextureHolder>
GLOzoneImageRepresentationShared::CreateTextureHolder(
    SharedImageBacking* backing,
    scoped_refptr<gfx::NativePixmap> pixmap,
    gfx::BufferFormat buffer_format,
    gfx::BufferPlane buffer_plane,
    const gfx::Size& size) {
  GLenum target;
  GLuint gl_texture_service_id;
  std::unique_ptr<ui::NativePixmapGLBinding> np_gl_binding =
      GLOzoneImageRepresentationShared::GetBinding(
          pixmap, buffer_format, buffer_plane, size, backing->color_space(),
          gl_texture_service_id, target);
  if (!np_gl_binding) {
    return nullptr;
  }

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

  return base::MakeRefCounted<TextureHolder>(std::move(np_gl_binding), texture);
}

// static
scoped_refptr<TextureHolder>
GLOzoneImageRepresentationShared::CreateTextureHolderPassthrough(
    SharedImageBacking* backing,
    scoped_refptr<gfx::NativePixmap> pixmap,
    gfx::BufferFormat buffer_format,
    gfx::BufferPlane buffer_plane,
    const gfx::Size& size) {
  GLenum target;
  GLuint gl_texture_service_id;
  std::unique_ptr<ui::NativePixmapGLBinding> np_gl_binding =
      GLOzoneImageRepresentationShared::GetBinding(
          pixmap, buffer_format, buffer_plane, size, backing->color_space(),
          gl_texture_service_id, target);
  if (!np_gl_binding) {
    return nullptr;
  }

  // TODO(crbug.com/1468989): Make sure these match with corresponding formats
  // from ToGLFormatDesc{ExternalSampler}.
  GLuint internal_format = np_gl_binding->GetInternalFormat();
  GLenum gl_format = GetDataFormatFromInternalFormat(internal_format);
  GLenum gl_type = np_gl_binding->GetDataType();
  scoped_refptr<gles2::TexturePassthrough> texture_passthrough =
      base::MakeRefCounted<gpu::gles2::TexturePassthrough>(
          gl_texture_service_id, target, internal_format,
          backing->size().width(), backing->size().height(),
          /*depth=*/1, /*border=*/0, gl_format, gl_type);

  return base::MakeRefCounted<TextureHolder>(std::move(np_gl_binding),
                                             std::move(texture_passthrough));
}

// static
std::vector<scoped_refptr<TextureHolder>>
GLOzoneImageRepresentationShared::CreateShared(
    SharedImageBacking* backing,
    scoped_refptr<gfx::NativePixmap> pixmap,
    gfx::BufferPlane plane,
    bool is_passthrough,
    std::vector<scoped_refptr<TextureHolder>>* cached_texture_holders) {
  std::vector<scoped_refptr<TextureHolder>> texture_holders;
  viz::SharedImageFormat format = backing->format();
  if (cached_texture_holders && !cached_texture_holders->empty()) {
    if (!format.PrefersExternalSampler()) {
      DCHECK_EQ(static_cast<int>(cached_texture_holders->size()),
                format.NumberOfPlanes());
    }
    texture_holders = *cached_texture_holders;
  }

  if (texture_holders.empty()) {
    if (format.is_single_plane()) {
      // Create a single texture holder with format and size of the backing. For
      // legacy multiplanar formats, the plane must be DEFAULT.
      auto size = backing->size();
      auto buffer_format = ToBufferFormat(format);
      if (format.IsLegacyMultiplanar()) {
        DCHECK_EQ(plane, gfx::BufferPlane::DEFAULT);
      }
      auto buffer_plane = plane;
      scoped_refptr<TextureHolder> holder;
      if (is_passthrough) {
        holder = CreateTextureHolderPassthrough(
            backing, std::move(pixmap), buffer_format, buffer_plane, size);
      } else {
        holder = CreateTextureHolder(backing, std::move(pixmap), buffer_format,
                                     buffer_plane, size);
      }
      if (!holder)
        return {};
      texture_holders.push_back(std::move(holder));
    } else if (format.PrefersExternalSampler()) {
      // Create a single texture holder with format of the NativePixmap, size of
      // the backing and DEFAULT plane
      auto size = backing->size();
      auto buffer_format = pixmap->GetBufferFormat();
      DCHECK_EQ(plane, gfx::BufferPlane::DEFAULT);
      auto buffer_plane = plane;
      scoped_refptr<TextureHolder> holder;
      if (is_passthrough) {
        holder = CreateTextureHolderPassthrough(
            backing, std::move(pixmap), buffer_format, buffer_plane, size);
      } else {
        holder = CreateTextureHolder(backing, std::move(pixmap), buffer_format,
                                     buffer_plane, size);
      }
      if (!holder)
        return {};
      texture_holders.push_back(std::move(holder));
    } else {
      // Create N texture holders with format using GetBufferFormatForPlane(),
      // size using GetPlaneSize() and plane using GetBufferPlane()
      DCHECK_EQ(plane, gfx::BufferPlane::DEFAULT);
      for (int plane_index = 0; plane_index < format.NumberOfPlanes();
           plane_index++) {
        auto size = format.GetPlaneSize(plane_index, backing->size());
        auto buffer_format = GetBufferFormatForPlane(format, plane_index);
        auto buffer_plane = GetBufferPlane(format, plane_index);
        scoped_refptr<TextureHolder> holder;
        if (is_passthrough) {
          holder = CreateTextureHolderPassthrough(
              backing, pixmap, buffer_format, buffer_plane, size);
        } else {
          holder = CreateTextureHolder(backing, pixmap, buffer_format,
                                       buffer_plane, size);
        }
        if (!holder)
          return {};
        texture_holders.push_back(std::move(holder));
      }
    }

    if (cached_texture_holders)
      *cached_texture_holders = texture_holders;
  }
  return texture_holders;
}

// static
std::unique_ptr<GLTextureOzoneImageRepresentation>
GLTextureOzoneImageRepresentation::Create(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    scoped_refptr<gfx::NativePixmap> pixmap,
    gfx::BufferPlane plane,
    std::vector<scoped_refptr<TextureHolder>>* cached_texture_holders) {
  std::vector<scoped_refptr<TextureHolder>> texture_holders =
      GLOzoneImageRepresentationShared::CreateShared(
          backing, std::move(pixmap), plane,
          /*is_passthrough=*/false, cached_texture_holders);
  if (texture_holders.empty()) {
    return nullptr;
  }
  return base::WrapUnique<GLTextureOzoneImageRepresentation>(
      new GLTextureOzoneImageRepresentation(manager, backing, tracker,
                                            std::move(texture_holders)));
}

GLTextureOzoneImageRepresentation::GLTextureOzoneImageRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    std::vector<scoped_refptr<TextureHolder>> texture_holders)
    : GLTextureImageRepresentation(manager, backing, tracker),
      texture_holders_(std::move(texture_holders)) {}

GLTextureOzoneImageRepresentation::~GLTextureOzoneImageRepresentation() {
  if (!has_context()) {
    for (auto& texture_holder : texture_holders_)
      texture_holder->MarkContextLost();
  }
}

gles2::Texture* GLTextureOzoneImageRepresentation::GetTexture(int plane_index) {
  return texture_holders_[plane_index]->texture();
}

bool GLTextureOzoneImageRepresentation::BeginAccess(GLenum mode) {
  DCHECK(!current_access_mode_);
  current_access_mode_ = mode;
  return GLOzoneImageRepresentationShared::BeginAccess(
      current_access_mode_, GetOzoneBacking(), need_end_fence_);
}

void GLTextureOzoneImageRepresentation::EndAccess() {
  GLOzoneImageRepresentationShared::EndAccess(
      need_end_fence_, current_access_mode_, GetOzoneBacking());
  current_access_mode_ = 0;
}

OzoneImageBacking* GLTextureOzoneImageRepresentation::GetOzoneBacking() {
  return static_cast<OzoneImageBacking*>(backing());
}

// static
std::unique_ptr<GLTexturePassthroughOzoneImageRepresentation>
GLTexturePassthroughOzoneImageRepresentation::Create(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    scoped_refptr<gfx::NativePixmap> pixmap,
    gfx::BufferPlane plane,
    std::vector<scoped_refptr<TextureHolder>>* cached_texture_holders) {
  std::vector<scoped_refptr<TextureHolder>> texture_holders =
      GLOzoneImageRepresentationShared::CreateShared(
          backing, std::move(pixmap), plane,
          /*is_passthrough=*/true, cached_texture_holders);
  if (texture_holders.empty()) {
    return nullptr;
  }
  return base::WrapUnique<GLTexturePassthroughOzoneImageRepresentation>(
      new GLTexturePassthroughOzoneImageRepresentation(
          manager, backing, tracker, std::move(texture_holders)));
}

GLTexturePassthroughOzoneImageRepresentation::
    GLTexturePassthroughOzoneImageRepresentation(
        SharedImageManager* manager,
        SharedImageBacking* backing,
        MemoryTypeTracker* tracker,
        std::vector<scoped_refptr<TextureHolder>> texture_holders)
    : GLTexturePassthroughImageRepresentation(manager, backing, tracker),
      texture_holders_(std::move(texture_holders)) {}

GLTexturePassthroughOzoneImageRepresentation::
    ~GLTexturePassthroughOzoneImageRepresentation() {
  if (!has_context()) {
    for (auto& texture_holder : texture_holders_)
      texture_holder->MarkContextLost();
  }
}

const scoped_refptr<gles2::TexturePassthrough>&
GLTexturePassthroughOzoneImageRepresentation::GetTexturePassthrough(
    int plane_index) {
  return texture_holders_[plane_index]->texture_passthrough();
}

bool GLTexturePassthroughOzoneImageRepresentation::BeginAccess(GLenum mode) {
  DCHECK(!current_access_mode_);
  current_access_mode_ = mode;
  return GLOzoneImageRepresentationShared::BeginAccess(
      current_access_mode_, GetOzoneBacking(), need_end_fence_);
}

void GLTexturePassthroughOzoneImageRepresentation::EndAccess() {
  GLOzoneImageRepresentationShared::EndAccess(
      need_end_fence_, current_access_mode_, GetOzoneBacking());
  current_access_mode_ = 0;
}

OzoneImageBacking*
GLTexturePassthroughOzoneImageRepresentation::GetOzoneBacking() {
  return static_cast<OzoneImageBacking*>(backing());
}

}  // namespace gpu
