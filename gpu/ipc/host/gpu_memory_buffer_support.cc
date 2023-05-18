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

bool GetImageNeedsPlatformSpecificTextureTarget(gfx::BufferFormat format,
                                                gfx::BufferUsage usage) {
  if (!NativeBufferNeedsPlatformSpecificTextureTarget(format))
    return false;
#if BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID)
  GpuMemoryBufferConfigurationSet native_configurations =
      gpu::GpuMemoryBufferSupport::GetNativeGpuMemoryBufferConfigurations();
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
