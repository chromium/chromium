// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/gles2_interface.h"

#include <GLES2/gl2.h>

namespace gpu {
namespace gles2 {

bool GLES2Interface::CanCopySharedImageToGLTextureViaTextureCopy(
    ClientSharedImage* shared_image) {
  return false;
}

bool GLES2Interface::CanCopySharedImageDirectlyToGLTexture(
    bool is_opaque,
    ClientSharedImage* shared_image,
    uint32_t dst_target,
    uint32_t dst_internal_format,
    uint32_t dst_type,
    int32_t dst_level,
    SkAlphaType dst_alpha_type) {
  return false;
}

bool GLES2Interface::CanCopySharedImageToGLTextureViaSkia(
    bool is_opaque,
    uint32_t shared_image_target,
    uint32_t dst_target,
    uint32_t dst_internal_format,
    uint32_t dst_type,
    int32_t dst_level,
    SkAlphaType dst_alpha_type) {
  return false;
}

gpu::SyncToken GLES2Interface::CopySharedImageToGLTextureViaTextureCopy(
    const gfx::Rect& src_rect,
    ClientSharedImage* source_shared_image,
    const gpu::SyncToken& source_sync_token,
    uint32_t target,
    uint32_t texture,
    uint32_t internal_format,
    uint32_t format,
    uint32_t type,
    int32_t level,
    SkAlphaType dst_alpha_type,
    GrSurfaceOrigin dst_origin) {
  return gpu::SyncToken();
}

GLboolean GLES2Interface::DidGpuSwitch(gl::GpuPreference* active_gpu) {
  return GL_FALSE;
}

}  // namespace gles2
}  // namespace gpu
