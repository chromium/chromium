// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/android/android_hardware_buffer_utils.h"

#include "base/android/android_hardware_buffer_compat.h"
#include "base/logging.h"
#include "ui/gfx/android/android_surface_control_compat.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {

namespace {
AHardwareBuffer_Desc GetBufferDescription(const gfx::Size& size,
                                          gfx::BufferFormat format,
                                          gfx::BufferUsage usage) {
  // On create, all elements must be initialized, including setting the
  // "reserved for future use" (rfu) fields to zero.
  AHardwareBuffer_Desc desc = {};
  desc.width = size.width();
  desc.height = size.height();
  desc.layers = 1;  // number of images

  switch (format) {
    case gfx::BufferFormat::RGBA_8888:
      desc.format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
      break;
    case gfx::BufferFormat::RGBX_8888:
      desc.format = AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM;
      break;
    case gfx::BufferFormat::BGR_565:
      desc.format = AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM;
      break;
    case gfx::BufferFormat::RGBA_F16:
      desc.format = AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT;
      break;
    case gfx::BufferFormat::RGBA_1010102:
      desc.format = AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  switch (usage) {
    case gfx::BufferUsage::GPU_READ:
    case gfx::BufferUsage::SCANOUT:
      desc.usage = AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE |
                   AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT;
      if (usage == gfx::BufferUsage::SCANOUT) {
        desc.usage |= gfx::SurfaceControl::RequiredUsage();
      }
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return desc;
}
}  // namespace

base::android::ScopedHardwareBufferHandle CreateScopedHardwareBufferHandle(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  CHECK(base::AndroidHardwareBufferCompat::IsSupportAvailable());
  AHardwareBuffer* buffer = nullptr;
  AHardwareBuffer_Desc desc = GetBufferDescription(size, format, usage);
  base::AndroidHardwareBufferCompat::GetInstance().Allocate(&desc, &buffer);
  if (!buffer) {
    LOG(ERROR) << "Failed to allocate AHardwareBuffer";
    return base::android::ScopedHardwareBufferHandle();
  }
  return base::android::ScopedHardwareBufferHandle::Adopt(buffer);
}

}  // namespace gpu
