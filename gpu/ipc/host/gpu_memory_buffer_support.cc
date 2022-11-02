// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/host/gpu_memory_buffer_support.h"

#include "base/containers/contains.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "ui/gl/gl_bindings.h"

namespace gpu {

GpuMemoryBufferConfigurationSet GetNativeGpuMemoryBufferConfigurations(
    GpuMemoryBufferSupport* support) {
  GpuMemoryBufferConfigurationSet configurations;

#if BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID)
  const gfx::BufferFormat kBufferFormats[] = {
      gfx::BufferFormat::R_8,
      gfx::BufferFormat::R_16,
      gfx::BufferFormat::RG_88,
      gfx::BufferFormat::RG_1616,
      gfx::BufferFormat::BGR_565,
      gfx::BufferFormat::RGBA_4444,
      gfx::BufferFormat::RGBX_8888,
      gfx::BufferFormat::RGBA_8888,
      gfx::BufferFormat::BGRX_8888,
      gfx::BufferFormat::BGRA_1010102,
      gfx::BufferFormat::RGBA_1010102,
      gfx::BufferFormat::BGRA_8888,
      gfx::BufferFormat::RGBA_F16,
      gfx::BufferFormat::YVU_420,
      gfx::BufferFormat::YUV_420_BIPLANAR,
      gfx::BufferFormat::YUVA_420_TRIPLANAR,
      gfx::BufferFormat::P010};

  const gfx::BufferUsage kUsages[] = {
      gfx::BufferUsage::GPU_READ,
      gfx::BufferUsage::SCANOUT,
      gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE,
      gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE,
      gfx::BufferUsage::SCANOUT_CPU_READ_WRITE,
      gfx::BufferUsage::SCANOUT_VDA_WRITE,
      gfx::BufferUsage::PROTECTED_SCANOUT_VDA_WRITE,
      gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
      gfx::BufferUsage::SCANOUT_VEA_CPU_READ,
      gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE,
      gfx::BufferUsage::SCANOUT_FRONT_RENDERING,
  };

  for (auto format : kBufferFormats) {
    for (auto usage : kUsages) {
      if (support->IsNativeGpuMemoryBufferConfigurationSupported(format, usage))
        configurations.insert(gfx::BufferUsageAndFormat(usage, format));
    }
  }
#endif  // BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_ANDROID)

  return configurations;
}

bool GetImageNeedsPlatformSpecificTextureTarget(gfx::BufferFormat format,
                                                gfx::BufferUsage usage) {
  if (!NativeBufferNeedsPlatformSpecificTextureTarget(format))
    return false;
#if BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID)
  GpuMemoryBufferSupport support;
  GpuMemoryBufferConfigurationSet native_configurations =
      GetNativeGpuMemoryBufferConfigurations(&support);
  return base::Contains(native_configurations,
                        gfx::BufferUsageAndFormat(usage, format));
#else
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
