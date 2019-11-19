// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/host/gpu_memory_buffer_support.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "ui/gl/gl_bindings.h"

namespace gpu {

GpuMemoryBufferConfigurationSet GetNativeGpuMemoryBufferConfigurations(
    GpuMemoryBufferSupport* support) {
  GpuMemoryBufferConfigurationSet configurations;

#if defined(USE_OZONE) || defined(OS_MACOSX) || defined(OS_WIN) || \
    defined(OS_ANDROID)
  const gfx::BufferFormat kBufferFormats[] = {
      gfx::BufferFormat::R_8,          gfx::BufferFormat::R_16,
      gfx::BufferFormat::RG_88,        gfx::BufferFormat::BGR_565,
      gfx::BufferFormat::RGBA_4444,    gfx::BufferFormat::RGBX_8888,
      gfx::BufferFormat::RGBA_8888,    gfx::BufferFormat::BGRX_8888,
      gfx::BufferFormat::BGRX_1010102, gfx::BufferFormat::RGBX_1010102,
      gfx::BufferFormat::BGRA_8888,    gfx::BufferFormat::RGBA_F16,
      gfx::BufferFormat::YVU_420,      gfx::BufferFormat::YUV_420_BIPLANAR,
      gfx::BufferFormat::P010};

  const gfx::BufferUsage kUsages[] = {
      gfx::BufferUsage::GPU_READ,
      gfx::BufferUsage::SCANOUT,
      gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE,
      gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE,
      gfx::BufferUsage::SCANOUT_CPU_READ_WRITE,
      gfx::BufferUsage::SCANOUT_VDA_WRITE,
      gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
      gfx::BufferUsage::SCANOUT_VEA_READ_CAMERA_AND_CPU_READ_WRITE,
  };

  for (auto format : kBufferFormats) {
    for (auto usage : kUsages) {
      if (support->IsNativeGpuMemoryBufferConfigurationSupported(format, usage))
        configurations.insert(std::make_pair(format, usage));
    }
  }
#endif  // defined(USE_OZONE) || defined(OS_MACOSX) || defined(OS_WIN) ||
        // defined(OS_ANDROID)

  return configurations;
}

bool GetImageNeedsPlatformSpecificTextureTarget(gfx::BufferFormat format,
                                                gfx::BufferUsage usage) {
  if (!NativeBufferNeedsPlatformSpecificTextureTarget(format))
    return false;
#if defined(USE_OZONE) || defined(OS_MACOSX) || defined(OS_WIN) || \
    defined(OS_ANDROID)
  GpuMemoryBufferSupport support;
  GpuMemoryBufferConfigurationSet native_configurations =
      GetNativeGpuMemoryBufferConfigurations(&support);
  return native_configurations.find(std::make_pair(format, usage)) !=
         native_configurations.end();
#else  // defined(USE_OZONE) || defined(OS_MACOSX)
  return false;
#endif
}

std::vector<gfx::BufferUsageAndFormat>
CreateBufferUsageAndFormatExceptionList() {
  std::vector<gfx::BufferUsageAndFormat> usage_format_list;
  for (int usage_idx = 0; usage_idx <= static_cast<int>(gfx::BufferUsage::LAST);
       ++usage_idx) {
    gfx::BufferUsage usage = static_cast<gfx::BufferUsage>(usage_idx);
    for (int format_idx = 0;
         format_idx <= static_cast<int>(gfx::BufferFormat::LAST);
         ++format_idx) {
      gfx::BufferFormat format = static_cast<gfx::BufferFormat>(format_idx);
      if (gpu::GetImageNeedsPlatformSpecificTextureTarget(format, usage))
        usage_format_list.push_back(gfx::BufferUsageAndFormat(usage, format));
    }
  }
  return usage_format_list;
}

}  // namespace gpu
