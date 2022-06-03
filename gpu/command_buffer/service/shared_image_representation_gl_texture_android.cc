// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shared_image_representation_gl_texture_android.h"

#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/ipc/common/android/android_image_reader_utils.h"

namespace gpu {

SharedImageRepresentationGLTextureAndroid::
    SharedImageRepresentationGLTextureAndroid(
        SharedImageManager* manager,
        SharedImageBackingAndroid* backing,
        MemoryTypeTracker* tracker,
        gles2::Texture* texture)
    : SharedImageRepresentationGLTexture(manager, backing, tracker),
      texture_(texture) {}

SharedImageRepresentationGLTextureAndroid::
    ~SharedImageRepresentationGLTextureAndroid() {
  EndAccess();

  if (texture_)
    texture_->RemoveLightweightRef(has_context());
}

gles2::Texture* SharedImageRepresentationGLTextureAndroid::GetTexture() {
  return texture_;
}

bool SharedImageRepresentationGLTextureAndroid::BeginAccess(GLenum mode) {
  bool read_only_mode = (mode == GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM) ||
                        (mode == GL_SHARED_IMAGE_ACCESS_MODE_OVERLAY_CHROMIUM);
  bool read_write_mode =
      (mode == GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
  DCHECK(read_only_mode || read_write_mode);

  if (read_only_mode) {
    base::ScopedFD write_sync_fd;
    if (!android_backing()->BeginRead(this, &write_sync_fd))
      return false;
    if (!InsertEglFenceAndWait(std::move(write_sync_fd)))
      return false;
  } else {
    base::ScopedFD sync_fd;
    if (!android_backing()->BeginWrite(&sync_fd))
      return false;

    if (!InsertEglFenceAndWait(std::move(sync_fd)))
      return false;
  }

  if (read_only_mode)
    mode_ = RepresentationAccessMode::kRead;
  else
    mode_ = RepresentationAccessMode::kWrite;

  return true;
}

void SharedImageRepresentationGLTextureAndroid::EndAccess() {
  if (mode_ == RepresentationAccessMode::kNone)
    return;

  base::ScopedFD sync_fd = CreateEglFenceAndExportFd();

  // Pass this fd to its backing.
  if (mode_ == RepresentationAccessMode::kRead) {
    android_backing()->EndRead(this, std::move(sync_fd));
  } else if (mode_ == RepresentationAccessMode::kWrite) {
    android_backing()->EndWrite(std::move(sync_fd));
  }

  mode_ = RepresentationAccessMode::kNone;
}

}  // namespace gpu
