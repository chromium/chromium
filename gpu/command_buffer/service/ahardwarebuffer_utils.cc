// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/ahardwarebuffer_utils.h"

#include <android/hardware_buffer.h>

#include "base/android/scoped_hardware_buffer_handle.h"
#include "base/check.h"
#include "base/notreached.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/vulkan/vulkan_image.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/scoped_binders.h"

namespace gpu {

bool AHardwareBufferSupportedFormat(viz::ResourceFormat format) {
  switch (format) {
    case viz::RGBA_8888:
    case viz::RGB_565:
    case viz::BGR_565:
    case viz::RGBA_F16:
    case viz::RGBX_8888:
    case viz::RGBA_1010102:
      return true;
    default:
      return false;
  }
}

unsigned int AHardwareBufferFormat(viz::ResourceFormat format) {
  DCHECK(AHardwareBufferSupportedFormat(format));
  switch (format) {
    case viz::RGBA_8888:
      return AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
    case viz::RGB_565:
      return AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM;
    case viz::BGR_565:
      return AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM;
    case viz::RGBA_F16:
      return AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT;
    case viz::RGBX_8888:
      return AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM;
    case viz::RGBA_1010102:
      return AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM;
    default:
      NOTREACHED();
      return AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
  }
}

std::unique_ptr<VulkanImage> CreateVkImageFromAhbHandle(
    base::android::ScopedHardwareBufferHandle ahb_handle,
    SharedContextState* context_state,
    const gfx::Size& size,
    const viz::SharedImageFormat& format,
    uint32_t queue_family_index) {
  DCHECK(context_state);
  DCHECK(context_state->GrContextIsVulkan());

  auto* device_queue = context_state->vk_context_provider()->GetDeviceQueue();
  gfx::GpuMemoryBufferHandle gmb_handle(std::move(ahb_handle));
  return VulkanImage::CreateFromGpuMemoryBufferHandle(
      device_queue, std::move(gmb_handle), size, ToVkFormat(format),
      /*usage=*/0, /*flags=*/0, /*image_tiling=*/VK_IMAGE_TILING_OPTIMAL,
      /*queue_family_index=*/queue_family_index);
}

gl::ScopedEGLImage CreateEGLImageFromAHardwareBuffer(AHardwareBuffer* buffer) {
  EGLint egl_image_attribs[] = {EGL_IMAGE_PRESERVED_KHR, EGL_FALSE, EGL_NONE};
  EGLClientBuffer client_buffer = eglGetNativeClientBufferANDROID(buffer);
  return gl::MakeScopedEGLImage(EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID,
                                client_buffer, egl_image_attribs);
}

}  // namespace gpu
