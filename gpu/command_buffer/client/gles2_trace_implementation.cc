// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/gles2_trace_implementation.h"

#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/client/client_shared_image.h"

namespace gpu {
namespace gles2 {

GLES2TraceImplementation::GLES2TraceImplementation(GLES2Interface* gl)
    : gl_(gl) {
}

GLES2TraceImplementation::~GLES2TraceImplementation() = default;

bool GLES2TraceImplementation::CanCopySharedImageToGLTextureViaTextureCopy(
    ClientSharedImage* shared_image) {
  return gl_->CanCopySharedImageToGLTextureViaTextureCopy(shared_image);
}

gpu::SyncToken
GLES2TraceImplementation::CopySharedImageToGLTextureViaTextureCopy(
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
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
  return gl_->CopySharedImageToGLTextureViaTextureCopy(
      coded_size, visible_rect, source_shared_image, source_sync_token, target,
      texture, internal_format, format, type, level, dst_alpha_type,
      dst_origin);
}

// InterfaceBase implementation.
void GLES2TraceImplementation::GenSyncTokenCHROMIUM(GLbyte* sync_token) {
  gl_->GenSyncTokenCHROMIUM(sync_token);
}
void GLES2TraceImplementation::GenUnverifiedSyncTokenCHROMIUM(
    GLbyte* sync_token) {
  gl_->GenUnverifiedSyncTokenCHROMIUM(sync_token);
}
void GLES2TraceImplementation::VerifySyncTokensCHROMIUM(GLbyte** sync_tokens,
                                                        GLsizei count) {
  gl_->VerifySyncTokensCHROMIUM(sync_tokens, count);
}
void GLES2TraceImplementation::WaitSyncTokenCHROMIUM(const GLbyte* sync_token) {
  gl_->WaitSyncTokenCHROMIUM(sync_token);
}
void GLES2TraceImplementation::ShallowFlushCHROMIUM() {
  gl_->ShallowFlushCHROMIUM();
}

// Include the auto-generated part of this file. We split this because it means
// we can easily edit the non-auto generated parts right here in this file
// instead of having to edit some template or the code generator.
#include "gpu/command_buffer/client/gles2_trace_implementation_impl_autogen.h"

}  // namespace gles2
}  // namespace gpu

