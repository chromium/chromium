// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_representation_gl_ozone.h"

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gl/buffer_format_utils.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_image_native_pixmap.h"
#include "ui/gl/gl_image_shared_memory.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/trace_util.h"

namespace gpu {

// static
std::unique_ptr<SharedImageRepresentationGLOzone>
SharedImageRepresentationGLOzone::Create(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    scoped_refptr<gfx::NativePixmap> pixmap,
    viz::ResourceFormat format) {
  gl::GLApi* api = gl::g_current_gl_context;
  DCHECK(api);

  GLuint internal_format = viz::TextureStorageFormat(format);

  GLuint gl_texture_service_id;
  api->glGenTexturesFn(1, &gl_texture_service_id);
  gl::ScopedTextureBinder binder(GL_TEXTURE_2D, gl_texture_service_id);

  gfx::BufferFormat buffer_format = viz::BufferFormat(format);
  auto image = base::MakeRefCounted<gl::GLImageNativePixmap>(
      pixmap->GetBufferSize(), buffer_format);
  if (!image->Initialize(pixmap)) {
    DLOG(ERROR) << "Unable to initialize EGL image from pixmap";
    return nullptr;
  }

  api->glTexParameteriFn(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  api->glTexParameteriFn(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  api->glTexParameteriFn(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  api->glTexParameteriFn(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  if (!image->BindTexImage(GL_TEXTURE_2D)) {
    DLOG(ERROR) << "Unable to bind EGL image to GL_TEXTURE_2D";
    return nullptr;
  }

  gles2::Texture* texture = new gles2::Texture(gl_texture_service_id);
  texture->SetLightweightRef();
  texture->SetTarget(GL_TEXTURE_2D, 1 /*max_levels=*/);
  texture->set_min_filter(GL_LINEAR);
  texture->set_mag_filter(GL_LINEAR);
  texture->set_wrap_t(GL_CLAMP_TO_EDGE);
  texture->set_wrap_s(GL_CLAMP_TO_EDGE);

  GLenum gl_format = viz::GLDataFormat(format);
  GLenum gl_type = viz::GLDataType(format);
  texture->SetLevelInfo(GL_TEXTURE_2D, 0, internal_format,
                        pixmap->GetBufferSize().width(),
                        pixmap->GetBufferSize().height(), 1, 0, gl_format,
                        gl_type, backing->ClearedRect());
  texture->SetLevelImage(GL_TEXTURE_2D, 0, image.get(), gles2::Texture::BOUND);
  texture->SetImmutable(true, true);

  return base::WrapUnique<SharedImageRepresentationGLOzone>(
      new SharedImageRepresentationGLOzone(manager, backing, tracker, texture));
}

SharedImageRepresentationGLOzone::SharedImageRepresentationGLOzone(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    gles2::Texture* texture)
    : SharedImageRepresentationGLTexture(manager, backing, tracker),
      texture_(texture) {}

SharedImageRepresentationGLOzone::~SharedImageRepresentationGLOzone() {
  texture_->RemoveLightweightRef(has_context());
}

gles2::Texture* SharedImageRepresentationGLOzone::GetTexture() {
  return texture_;
}

bool SharedImageRepresentationGLOzone::BeginAccess(GLenum mode) {
  DCHECK(!current_access_mode_);
  current_access_mode_ = mode;
  std::vector<gfx::GpuFenceHandle> fences;
  ozone_backing()->BeginAccess(&fences);

  if (ozone_backing()->NeedsSynchronization()) {
    for (auto& fence : fences) {
      gfx::GpuFence gpu_fence = gfx::GpuFence(std::move(fence));
      std::unique_ptr<gl::GLFence> gl_fence =
          gl::GLFence::CreateFromGpuFence(gpu_fence);
      gl_fence->ServerWait();
    }
  }

  // We must call VaapiWrapper::SyncSurface() to ensure all VA-API work is done
  // prior to using the buffer in a graphics API.
  return ozone_backing()->VaSync();
}

void SharedImageRepresentationGLOzone::EndAccess() {
  gfx::GpuFenceHandle fence;
  if (ozone_backing()->NeedsSynchronization()) {
    auto gl_fence = gl::GLFence::CreateForGpuFence();
    DCHECK(gl_fence);
    fence = gl_fence->GetGpuFence()->GetGpuFenceHandle().Clone();
  }
  bool readonly =
      current_access_mode_ != GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM;
  ozone_backing()->EndAccess(readonly, std::move(fence));
  current_access_mode_ = 0;
}

}  // namespace gpu
