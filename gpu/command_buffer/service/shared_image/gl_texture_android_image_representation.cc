// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/gl_texture_android_image_representation.h"

#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/android/egl_fence_utils.h"

namespace gpu {

GLTextureAndroidImageRepresentation::GLTextureAndroidImageRepresentation(
    SharedImageManager* manager,
    AndroidImageBacking* backing,
    MemoryTypeTracker* tracker,
    gl::ScopedEGLImage egl_image,
    gles2::Texture* texture)
    : GLTextureImageRepresentation(manager, backing, tracker),
      egl_image_(std::move(egl_image)),
      texture_(texture) {}

GLTextureAndroidImageRepresentation::~GLTextureAndroidImageRepresentation() {
  EndAccess();
  if (texture_) {
    texture_.ExtractAsDangling()->RemoveLightweightRef(has_context());
  }
}

gles2::Texture* GLTextureAndroidImageRepresentation::GetTexture(
    int plane_index) {
  DCHECK_EQ(plane_index, 0);
  return texture_;
}

bool GLTextureAndroidImageRepresentation::BeginAccess(GLenum mode) {
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

void GLTextureAndroidImageRepresentation::EndAccess() {
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
