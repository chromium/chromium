// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/android/android_image_reader_utils.h"

#include <android/native_window_jni.h>

#include "ui/gl/gl_fence_android_native_fence_sync.h"
#include "ui/gl/gl_fence_egl.h"
#include "ui/gl/gl_image_ahardwarebuffer.h"

namespace gpu {

base::ScopedFD CreateEglFenceAndExportFd() {
  std::unique_ptr<gl::GLFenceAndroidNativeFenceSync> android_native_fence =
      gl::GLFenceAndroidNativeFenceSync::CreateForGpuFence();
  if (!android_native_fence) {
    LOG(ERROR) << "Failed to create android native fence sync object.";
    return base::ScopedFD();
  }
  std::unique_ptr<gfx::GpuFence> gpu_fence =
      android_native_fence->GetGpuFence();
  if (!gpu_fence) {
    LOG(ERROR) << "Unable to get a gpu fence object.";
    return base::ScopedFD();
  }
  gfx::GpuFenceHandle fence_handle = gpu_fence->GetGpuFenceHandle().Clone();
  if (fence_handle.is_null()) {
    LOG(ERROR) << "Gpu fence handle is null";
    return base::ScopedFD();
  }
  return std::move(fence_handle.owned_fd);
}

bool DeleteAImageAsync(AImage* image,
                       base::android::AndroidImageReader* loader) {
  // If there is no image to delete, there is no need to insert fence.
  if (image == nullptr)
    return true;

  // Create egl fence and export a sync fd from it.
  base::ScopedFD fence_fd = CreateEglFenceAndExportFd();
  if (!fence_fd.is_valid())
    return false;

  // Delete the image synchronously. Release the fence_fd as below api will own
  // it and ensure that the file descriptor is closed properly.
  loader->AImage_deleteAsync(image, fence_fd.release());
  return true;
}

bool InsertEglFenceAndWait(base::ScopedFD acquire_fence_fd) {
  int fence_fd = acquire_fence_fd.release();

  // If fence_fd is -1, we do not need synchronization fence and image is ready
  // to be used immediately. Also we dont need to close any fd. Else we need to
  // create a sync fence which is used to signal when the buffer is ready to be
  // consumed.
  if (fence_fd == -1)
    return true;

  // Create attribute-value list with the fence_fd.
  EGLint attribs[] = {EGL_SYNC_NATIVE_FENCE_FD_ANDROID, fence_fd, EGL_NONE};

  // Create and insert the fence sync gl command using the helper class in
  // gl_fence_egl.h. This method takes the ownership of the file descriptor if
  // it succeeds.
  std::unique_ptr<gl::GLFenceEGL> egl_fence(
      gl::GLFenceEGL::Create(EGL_SYNC_NATIVE_FENCE_ANDROID, attribs));

  // If above method fails to create an egl_fence, we need to close the file
  // descriptor.
  if (egl_fence == nullptr) {
    // Create a scoped FD to close fence_fd.
    base::ScopedFD temp_fd(fence_fd);
    LOG(ERROR) << " Failed to created egl fence object ";
    return false;
  }

  // Make the server wait and not the client.
  egl_fence->ServerWait();
  return true;
}

bool CreateAndBindEglImage(const AImage* image,
                           GLuint texture_id,
                           base::android::AndroidImageReader* loader) {
  // Get the hardware buffer from the image.
  AHardwareBuffer* buffer = nullptr;
  DCHECK(image);
  if (loader->AImage_getHardwareBuffer(image, &buffer) != AMEDIA_OK) {
    LOG(ERROR) << "hardware buffer is null";
    return false;
  }

  // Create a egl image from the hardware buffer. Get the image size to create
  // egl image.
  int32_t image_height = 0, image_width = 0;
  if (loader->AImage_getWidth(image, &image_width) != AMEDIA_OK) {
    LOG(ERROR) << "image width is null OR image has been deleted";
    return false;
  }
  if (loader->AImage_getHeight(image, &image_height) != AMEDIA_OK) {
    LOG(ERROR) << "image height is null OR image has been deleted";
    return false;
  }
  gfx::Size image_size(image_width, image_height);
  auto egl_image = base::MakeRefCounted<gl::GLImageAHardwareBuffer>(image_size);
  if (!egl_image->Initialize(buffer, false)) {
    LOG(ERROR) << "Failed to create EGL image ";
    return false;
  }

  // Now bind this egl image to the texture target GL_TEXTURE_EXTERNAL_OES. Note
  // that once the egl image is bound, it can be destroyed safely without
  // affecting the rendering using this texture image.
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture_id);
  egl_image->BindTexImage(GL_TEXTURE_EXTERNAL_OES);
  return true;
}

}  // namespace gpu
