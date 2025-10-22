// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/android/android_hardware_buffer_utils.h"

#include <android/hardware_buffer.h>

#include "base/logging.h"
#include "ui/gfx/android/android_surface_control_compat.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {

namespace {
AHardwareBuffer_Desc GetBufferDescription(const gfx::Size& size,
                                          viz::SharedImageFormat format,
                                          gfx::BufferUsage usage) {
  // On create, all elements must be initialized, including setting the
  // "reserved for future use" (rfu) fields to zero.
  AHardwareBuffer_Desc desc = {};
  desc.width = size.width();
  desc.height = size.height();
  desc.layers = 1;  // number of images

  if (format == viz::SinglePlaneFormat::kRGBA_8888) {
    desc.format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
  } else if (format == viz::SinglePlaneFormat::kRGBX_8888) {
    desc.format = AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM;
  } else if (format == viz::SinglePlaneFormat::kBGR_565) {
    desc.format = AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM;
  } else if (format == viz::SinglePlaneFormat::kRGBA_F16) {
    desc.format = AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT;
  } else if (format == viz::SinglePlaneFormat::kRGBA_1010102) {
    desc.format = AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM;
  } else {
    NOTREACHED();
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
      NOTREACHED();
  }
  return desc;
}
}  // namespace

base::android::ScopedHardwareBufferHandle CreateScopedHardwareBufferHandle(
    const gfx::Size& size,
    viz::SharedImageFormat format,
    gfx::BufferUsage usage) {
  AHardwareBuffer* buffer = nullptr;
  AHardwareBuffer_Desc desc = GetBufferDescription(size, format, usage);
  AHardwareBuffer_allocate(&desc, &buffer);
  if (!buffer) {
    LOG(ERROR) << "Failed to allocate AHardwareBuffer";
    return base::android::ScopedHardwareBufferHandle();
  }
  return base::android::ScopedHardwareBufferHandle::Adopt(buffer);
}

}  // namespace gpu
