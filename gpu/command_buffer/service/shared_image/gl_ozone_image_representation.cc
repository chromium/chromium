// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/gl_ozone_image_representation.h"
#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/trace_util.h"
#include "ui/ozone/public/gl_ozone.h"
#include "ui/ozone/public/native_pixmap_gl_binding.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace gpu {

bool GLOzoneImageRepresentationShared::BeginAccess(
    GLenum mode,
    OzoneImageBacking* ozone_backing,
    bool& need_end_fence) {
  bool readonly = mode != GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM;
  std::vector<gfx::GpuFenceHandle> fences;
  ozone_backing->BeginAccess(readonly, OzoneImageBacking::AccessStream::kGL,
                             &fences, need_end_fence);

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

std::unique_ptr<ui::NativePixmapGLBinding>
GLOzoneImageRepresentationShared::GetBinding(
    SharedImageBacking* backing,
    scoped_refptr<gfx::NativePixmap> pixmap,
    gfx::BufferPlane plane,
    GLuint& gl_texture_service_id,
    GLenum& target) {
  ui::GLOzone* gl_ozone = ui::OzonePlatform::GetInstance()
                              ->GetSurfaceFactoryOzone()
                              ->GetCurrentGLOzone();
  if (!gl_ozone) {
    LOG(FATAL) << "Failed to get GLOzone.";
    return nullptr;
  }

  gfx::BufferFormat buffer_format = viz::BufferFormat(backing->format());
  target = !NativeBufferNeedsPlatformSpecificTextureTarget(buffer_format, plane)
               ? GL_TEXTURE_2D
               : gpu::GetPlatformSpecificTextureTarget();

  gl::GLApi* api = gl::g_current_gl_context;
  DCHECK(api);
  api->glGenTexturesFn(1, &gl_texture_service_id);

  std::unique_ptr<ui::NativePixmapGLBinding> np_gl_binding =
      gl_ozone->ImportNativePixmap(std::move(pixmap), buffer_format, plane,
                                   backing->size(), backing->color_space(),
                                   target, gl_texture_service_id);
  if (!np_gl_binding) {
    DLOG(ERROR) << "Failed to create NativePixmapGLBinding.";
    api->glDeleteTexturesFn(1, &gl_texture_service_id);
    return nullptr;
  }

  return np_gl_binding;
}

// static
std::unique_ptr<GLTextureOzoneImageRepresentation>
GLTextureOzoneImageRepresentation::Create(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    scoped_refptr<gfx::NativePixmap> pixmap,
    gfx::BufferPlane plane) {
  GLenum target;
  GLuint gl_texture_service_id;
  std::unique_ptr<ui::NativePixmapGLBinding> np_gl_binding =
      GLOzoneImageRepresentationShared::GetBinding(
          backing, std::move(pixmap), plane, gl_texture_service_id, target);
  if (!np_gl_binding) {
    return nullptr;
  }

  gles2::Texture* texture =
      gles2::CreateGLES2TextureWithLightRef(gl_texture_service_id, target);

  GLuint internal_format = np_gl_binding->GetInternalFormat();
  GLenum gl_format = np_gl_binding->GetDataFormat();
  GLenum gl_type = np_gl_binding->GetDataType();
  texture->SetLevelInfo(target, 0, internal_format, backing->size().width(),
                        backing->size().height(), 1, 0, gl_format, gl_type,
                        backing->ClearedRect());
  texture->SetImmutable(true, true);

  return base::WrapUnique<GLTextureOzoneImageRepresentation>(
      new GLTextureOzoneImageRepresentation(manager, backing, tracker,
                                            texture));
}

GLTextureOzoneImageRepresentation::GLTextureOzoneImageRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    gles2::Texture* texture)
    : GLTextureImageRepresentation(manager, backing, tracker),
      texture_(texture) {}

GLTextureOzoneImageRepresentation::~GLTextureOzoneImageRepresentation() {
  texture_->RemoveLightweightRef(has_context());
}

gles2::Texture* GLTextureOzoneImageRepresentation::GetTexture() {
  return texture_;
}

bool GLTextureOzoneImageRepresentation::BeginAccess(GLenum mode) {
  DCHECK(!current_access_mode_);
  current_access_mode_ = mode;
  return GLOzoneImageRepresentationShared::BeginAccess(
      current_access_mode_, ozone_backing(), need_end_fence_);
}

void GLTextureOzoneImageRepresentation::EndAccess() {
  GLOzoneImageRepresentationShared::EndAccess(
      need_end_fence_, current_access_mode_, ozone_backing());
  current_access_mode_ = 0;
}

// static
std::unique_ptr<GLTexturePassthroughOzoneImageRepresentation>
GLTexturePassthroughOzoneImageRepresentation::Create(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    scoped_refptr<gfx::NativePixmap> pixmap,
    gfx::BufferPlane plane) {
  GLenum target;
  GLuint gl_texture_service_id;
  std::unique_ptr<ui::NativePixmapGLBinding> np_gl_binding =
      GLOzoneImageRepresentationShared::GetBinding(
          backing, std::move(pixmap), plane, gl_texture_service_id, target);
  if (!np_gl_binding) {
    return nullptr;
  }

  GLuint internal_format = np_gl_binding->GetInternalFormat();
  GLenum gl_format = np_gl_binding->GetDataFormat();
  GLenum gl_type = np_gl_binding->GetDataType();

  scoped_refptr<gles2::TexturePassthrough> texture_passthrough =
      base::MakeRefCounted<gpu::gles2::TexturePassthrough>(
          gl_texture_service_id, target, internal_format,
          backing->size().width(), backing->size().height(),
          /*depth=*/1, /*border=*/0, gl_format, gl_type);

  return base::WrapUnique<GLTexturePassthroughOzoneImageRepresentation>(
      new GLTexturePassthroughOzoneImageRepresentation(
          manager, backing, tracker, texture_passthrough));
}

GLTexturePassthroughOzoneImageRepresentation::
    GLTexturePassthroughOzoneImageRepresentation(
        SharedImageManager* manager,
        SharedImageBacking* backing,
        MemoryTypeTracker* tracker,
        scoped_refptr<gles2::TexturePassthrough> texture_passthrough)
    : GLTexturePassthroughImageRepresentation(manager, backing, tracker),
      texture_passthrough_(texture_passthrough) {}

GLTexturePassthroughOzoneImageRepresentation::
    ~GLTexturePassthroughOzoneImageRepresentation() = default;

const scoped_refptr<gles2::TexturePassthrough>&
GLTexturePassthroughOzoneImageRepresentation::GetTexturePassthrough() {
  return texture_passthrough_;
}

bool GLTexturePassthroughOzoneImageRepresentation::BeginAccess(GLenum mode) {
  DCHECK(!current_access_mode_);
  current_access_mode_ = mode;
  return GLOzoneImageRepresentationShared::BeginAccess(
      current_access_mode_, ozone_backing(), need_end_fence_);
}

void GLTexturePassthroughOzoneImageRepresentation::EndAccess() {
  GLOzoneImageRepresentationShared::EndAccess(
      need_end_fence_, current_access_mode_, ozone_backing());
  current_access_mode_ = 0;
}

}  // namespace gpu
