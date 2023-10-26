// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/gl_ozone_image_representation.h"
#include <memory>

#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_image/ozone_image_backing.h"
#include "gpu/command_buffer/service/shared_image/ozone_image_gl_textures_holder.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/gl_fence.h"

namespace gpu {

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

GLTextureOzoneImageRepresentation::GLTextureOzoneImageRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    scoped_refptr<OzoneImageGLTexturesHolder> textures_holder,
    bool should_mark_context_lost_textures_holder)
    : GLTextureImageRepresentation(manager, backing, tracker),
      textures_holder_(std::move(textures_holder)),
      should_mark_context_lost_textures_holder_(
          should_mark_context_lost_textures_holder) {}

GLTextureOzoneImageRepresentation::~GLTextureOzoneImageRepresentation() {
  if (!has_context()) {
    if (should_mark_context_lost_textures_holder_) {
      textures_holder_->MarkContextLost();
    } else {
      // The textures must have already been marked as context lost.
      DCHECK(textures_holder_->WasContextLost());
    }
  }
}

gles2::Texture* GLTextureOzoneImageRepresentation::GetTexture(int plane_index) {
  return textures_holder_->texture(plane_index);
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

GLTexturePassthroughOzoneImageRepresentation::
    GLTexturePassthroughOzoneImageRepresentation(
        SharedImageManager* manager,
        SharedImageBacking* backing,
        MemoryTypeTracker* tracker,
        scoped_refptr<OzoneImageGLTexturesHolder> textures_holder,
        bool should_mark_context_lost_textures_holder)
    : GLTexturePassthroughImageRepresentation(manager, backing, tracker),
      textures_holder_(std::move(textures_holder)),
      should_mark_context_lost_textures_holder_(
          should_mark_context_lost_textures_holder) {}

GLTexturePassthroughOzoneImageRepresentation::
    ~GLTexturePassthroughOzoneImageRepresentation() {
  if (!has_context()) {
    if (should_mark_context_lost_textures_holder_) {
      textures_holder_->MarkContextLost();
    } else {
      // The textures must have already been marked as context lost.
      DCHECK(textures_holder_->WasContextLost());
    }
  }
}

const scoped_refptr<gles2::TexturePassthrough>&
GLTexturePassthroughOzoneImageRepresentation::GetTexturePassthrough(
    int plane_index) {
  return textures_holder_->texture_passthrough(plane_index);
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
