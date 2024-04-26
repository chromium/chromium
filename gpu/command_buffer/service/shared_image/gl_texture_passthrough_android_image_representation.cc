// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/gl_texture_passthrough_android_image_representation.h"

#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/android/egl_fence_utils.h"

namespace gpu {

GLTexturePassthroughAndroidImageRepresentation::
    GLTexturePassthroughAndroidImageRepresentation(
        SharedImageManager* manager,
        AndroidImageBacking* backing,
        MemoryTypeTracker* tracker,
        gl::ScopedEGLImage egl_image,
        scoped_refptr<gles2::TexturePassthrough> texture)
    : GLTexturePassthroughImageRepresentation(manager, backing, tracker),
      egl_image_(std::move(egl_image)),
      texture_(std::move(texture)) {
  // TODO(crbug.com/40166788): Remove this CHECK.
  CHECK(texture_);
}

GLTexturePassthroughAndroidImageRepresentation::
    ~GLTexturePassthroughAndroidImageRepresentation() {
  EndAccess();
  if (!has_context())
    texture_->MarkContextLost();
}

const scoped_refptr<gles2::TexturePassthrough>&
GLTexturePassthroughAndroidImageRepresentation::GetTexturePassthrough(
    int plane_index) {
  DCHECK_EQ(plane_index, 0);
  return texture_;
}

bool GLTexturePassthroughAndroidImageRepresentation::BeginAccess(GLenum mode) {
  bool read_only_mode = (mode == GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
  bool read_write_mode =
      (mode == GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
  DCHECK(read_only_mode || read_write_mode);

  if (read_only_mode) {
    base::ScopedFD write_sync_fd;
    if (!android_backing()->BeginRead(this, &write_sync_fd))
      return false;
    if (!gl::InsertEglFenceAndWait(std::move(write_sync_fd)))
      return false;
  } else {
    base::ScopedFD sync_fd;
    if (!android_backing()->BeginWrite(&sync_fd))
      return false;

    if (!gl::InsertEglFenceAndWait(std::move(sync_fd)))
      return false;
  }

  if (read_only_mode)
    mode_ = RepresentationAccessMode::kRead;
  else
    mode_ = RepresentationAccessMode::kWrite;

  return true;
}

void GLTexturePassthroughAndroidImageRepresentation::EndAccess() {
  if (mode_ == RepresentationAccessMode::kNone)
    return;

  base::ScopedFD sync_fd = gl::CreateEglFenceAndExportFd();

  // Pass this fd to its backing.
  if (mode_ == RepresentationAccessMode::kRead) {
    android_backing()->EndRead(this, std::move(sync_fd));
  } else if (mode_ == RepresentationAccessMode::kWrite) {
    android_backing()->EndWrite(std::move(sync_fd));
  }

  mode_ = RepresentationAccessMode::kNone;
}

}  // namespace gpu
