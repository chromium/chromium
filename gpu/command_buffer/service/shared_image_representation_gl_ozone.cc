// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_representation_gl_ozone.h"
#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
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

bool SharedImageRepresentationGLOzoneShared::BeginAccess(
    GLenum mode,
    SharedImageBackingOzone* ozone_backing,
    bool& need_end_fence) {
  bool readonly = mode != GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM;
  std::vector<gfx::GpuFenceHandle> fences;
  ozone_backing->BeginAccess(readonly,
                             SharedImageBackingOzone::AccessStream::kGL,
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

void SharedImageRepresentationGLOzoneShared::EndAccess(
    bool need_end_fence,
    GLenum mode,
    SharedImageBackingOzone* ozone_backing) {
  gfx::GpuFenceHandle fence;
  // ChromeOS VMs don't support gpu fences, so there is no good way to
  // synchronize with GL.
  if (gl::GLFence::IsGpuFenceSupported() && need_end_fence) {
    auto gl_fence = gl::GLFence::CreateForGpuFence();
    DCHECK(gl_fence);
    fence = gl_fence->GetGpuFence()->GetGpuFenceHandle().Clone();
  }
  bool readonly = mode != GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM;
  ozone_backing->EndAccess(readonly, SharedImageBackingOzone::AccessStream::kGL,
                           std::move(fence));
}

std::unique_ptr<ui::NativePixmapGLBinding>
SharedImageRepresentationGLOzoneShared::GetBinding(
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
std::unique_ptr<SharedImageRepresentationGLTextureOzone>
SharedImageRepresentationGLTextureOzone::Create(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    scoped_refptr<gfx::NativePixmap> pixmap,
    gfx::BufferPlane plane) {
  GLenum target;
  GLuint gl_texture_service_id;
  std::unique_ptr<ui::NativePixmapGLBinding> np_gl_binding =
      SharedImageRepresentationGLOzoneShared::GetBinding(
          backing, std::move(pixmap), plane, gl_texture_service_id, target);
  if (!np_gl_binding) {
    return nullptr;
  }

  gles2::Texture* texture = new gles2::Texture(gl_texture_service_id);
  texture->SetLightweightRef();
  texture->SetTarget(target, 1 /*max_levels=*/);
  texture->set_min_filter(GL_LINEAR);
  texture->set_mag_filter(GL_LINEAR);
  texture->set_wrap_t(GL_CLAMP_TO_EDGE);
  texture->set_wrap_s(GL_CLAMP_TO_EDGE);

  GLuint internal_format = np_gl_binding->GetInternalFormat();
  GLenum gl_format = np_gl_binding->GetDataFormat();
  GLenum gl_type = np_gl_binding->GetDataType();
  texture->SetLevelInfo(target, 0, internal_format, backing->size().width(),
                        backing->size().height(), 1, 0, gl_format, gl_type,
                        backing->ClearedRect());
  texture->SetImmutable(true, true);

  return base::WrapUnique<SharedImageRepresentationGLTextureOzone>(
      new SharedImageRepresentationGLTextureOzone(manager, backing, tracker,
                                                  texture));
}

SharedImageRepresentationGLTextureOzone::
    SharedImageRepresentationGLTextureOzone(SharedImageManager* manager,
                                            SharedImageBacking* backing,
                                            MemoryTypeTracker* tracker,
                                            gles2::Texture* texture)
    : SharedImageRepresentationGLTexture(manager, backing, tracker),
      texture_(texture) {}

SharedImageRepresentationGLTextureOzone::
    ~SharedImageRepresentationGLTextureOzone() {
  texture_->RemoveLightweightRef(has_context());
}

gles2::Texture* SharedImageRepresentationGLTextureOzone::GetTexture() {
  return texture_;
}

bool SharedImageRepresentationGLTextureOzone::BeginAccess(GLenum mode) {
  DCHECK(!current_access_mode_);
  current_access_mode_ = mode;
  return SharedImageRepresentationGLOzoneShared::BeginAccess(
      current_access_mode_, ozone_backing(), need_end_fence_);
}

void SharedImageRepresentationGLTextureOzone::EndAccess() {
  SharedImageRepresentationGLOzoneShared::EndAccess(
      need_end_fence_, current_access_mode_, ozone_backing());
  current_access_mode_ = 0;
}

// static
std::unique_ptr<SharedImageRepresentationGLTexturePassthroughOzone>
SharedImageRepresentationGLTexturePassthroughOzone::Create(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    scoped_refptr<gfx::NativePixmap> pixmap,
    gfx::BufferPlane plane) {
  GLenum target;
  GLuint gl_texture_service_id;
  std::unique_ptr<ui::NativePixmapGLBinding> np_gl_binding =
      SharedImageRepresentationGLOzoneShared::GetBinding(
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

  return base::WrapUnique<SharedImageRepresentationGLTexturePassthroughOzone>(
      new SharedImageRepresentationGLTexturePassthroughOzone(
          manager, backing, tracker, texture_passthrough));
}

SharedImageRepresentationGLTexturePassthroughOzone::
    SharedImageRepresentationGLTexturePassthroughOzone(
        SharedImageManager* manager,
        SharedImageBacking* backing,
        MemoryTypeTracker* tracker,
        scoped_refptr<gles2::TexturePassthrough> texture_passthrough)
    : SharedImageRepresentationGLTexturePassthrough(manager, backing, tracker),
      texture_passthrough_(texture_passthrough) {}

SharedImageRepresentationGLTexturePassthroughOzone::
    ~SharedImageRepresentationGLTexturePassthroughOzone() = default;

const scoped_refptr<gles2::TexturePassthrough>&
SharedImageRepresentationGLTexturePassthroughOzone::GetTexturePassthrough() {
  return texture_passthrough_;
}

bool SharedImageRepresentationGLTexturePassthroughOzone::BeginAccess(
    GLenum mode) {
  DCHECK(!current_access_mode_);
  current_access_mode_ = mode;
  return SharedImageRepresentationGLOzoneShared::BeginAccess(
      current_access_mode_, ozone_backing(), need_end_fence_);
}

void SharedImageRepresentationGLTexturePassthroughOzone::EndAccess() {
  SharedImageRepresentationGLOzoneShared::EndAccess(
      need_end_fence_, current_access_mode_, ozone_backing());
  current_access_mode_ = 0;
}

}  // namespace gpu
