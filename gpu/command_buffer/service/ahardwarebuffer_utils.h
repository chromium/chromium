// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_AHARDWAREBUFFER_UTILS_H_
#define GPU_COMMAND_BUFFER_SERVICE_AHARDWAREBUFFER_UTILS_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gl/scoped_egl_image.h"

extern "C" typedef struct AHardwareBuffer AHardwareBuffer;

typedef unsigned int GLenum;

namespace base {
namespace android {
class ScopedHardwareBufferHandle;
}  // namespace android
}  // namespace base

namespace gfx {
class Size;
}  // namespace gfx

namespace gpu {
class SharedContextState;
class VulkanImage;

// Create a vulkan image from the AHB handle.
std::unique_ptr<VulkanImage> CreateVkImageFromAhbHandle(
    base::android::ScopedHardwareBufferHandle ahb_handle,
    SharedContextState* context_state,
    const gfx::Size& size,
    const viz::SharedImageFormat& format,
    uint32_t queue_family_index);

// Creates an EGLImage from |buffer|, setting EGL_IMAGE_PRESERVED_KHR to false.
GPU_GLES2_EXPORT gl::ScopedEGLImage CreateEGLImageFromAHardwareBuffer(
    AHardwareBuffer* buffer);

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_AHARDWAREBUFFER_UTILS_H_
