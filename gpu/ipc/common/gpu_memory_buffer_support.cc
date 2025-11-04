// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/gpu_memory_buffer_support.h"

#include <inttypes.h>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/process_memory_dump.h"
#include "build/build_config.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/buffer_usage_util.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace gpu {

GpuMemoryBufferSupport::GpuMemoryBufferSupport() = default;

GpuMemoryBufferSupport::~GpuMemoryBufferSupport() = default;

// static
bool GpuMemoryBufferSupport::IsNativeGpuMemoryBufferConfigurationSupported(
    viz::SharedImageFormat format,
    gfx::BufferUsage usage) {
#if BUILDFLAG(IS_APPLE)
  switch (usage) {
    case gfx::BufferUsage::GPU_READ:
    case gfx::BufferUsage::SCANOUT:
    case gfx::BufferUsage::SCANOUT_CPU_READ_WRITE:
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
    case gfx::BufferUsage::SCANOUT_FRONT_RENDERING:
    case gfx::BufferUsage::SCANOUT_VEA_CPU_READ:
      return format == viz::SinglePlaneFormat::kBGRA_8888 ||
             format == viz::SinglePlaneFormat::kRGBA_8888 ||
             format == viz::SinglePlaneFormat::kBGRX_8888 ||
             format == viz::SinglePlaneFormat::kRGBX_8888 ||
             format == viz::SinglePlaneFormat::kR_8 ||
             format == viz::SinglePlaneFormat::kRG_88 ||
             format == viz::SinglePlaneFormat::kR_16 ||
             format == viz::SinglePlaneFormat::kRG_1616 ||
             format == viz::SinglePlaneFormat::kRGBA_F16 ||
             format == viz::SinglePlaneFormat::kBGRA_1010102 ||
             format == viz::MultiPlaneFormat::kNV12 ||
             format == viz::MultiPlaneFormat::kNV12A ||
             format == viz::MultiPlaneFormat::kP010;
    case gfx::BufferUsage::SCANOUT_VDA_WRITE:
    case gfx::BufferUsage::PROTECTED_SCANOUT:
    case gfx::BufferUsage::PROTECTED_SCANOUT_VDA_WRITE:
    case gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE:
    case gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE:
    case gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE:
      return false;
  }
  NOTREACHED();
#elif BUILDFLAG(IS_ANDROID)
  switch (usage) {
    case gfx::BufferUsage::GPU_READ:
    case gfx::BufferUsage::SCANOUT:
      return format == viz::SinglePlaneFormat::kRGBA_8888 ||
             format == viz::SinglePlaneFormat::kRGBX_8888 ||
             format == viz::SinglePlaneFormat::kBGR_565;
    case gfx::BufferUsage::SCANOUT_CPU_READ_WRITE:
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
    case gfx::BufferUsage::SCANOUT_VDA_WRITE:
    case gfx::BufferUsage::PROTECTED_SCANOUT:
    case gfx::BufferUsage::PROTECTED_SCANOUT_VDA_WRITE:
    case gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE:
    case gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE:
    case gfx::BufferUsage::SCANOUT_VEA_CPU_READ:
    case gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE:
    case gfx::BufferUsage::SCANOUT_FRONT_RENDERING:
      return false;
  }
  NOTREACHED();
#elif BUILDFLAG(IS_OZONE)
  auto buffer_format = viz::SharedImageFormatToBufferFormat(format);
  return ui::OzonePlatform::GetInstance()->IsNativePixmapConfigSupported(
      buffer_format, usage);
#elif BUILDFLAG(IS_WIN)
  switch (usage) {
    case gfx::BufferUsage::GPU_READ:
    case gfx::BufferUsage::SCANOUT:
      return format == viz::SinglePlaneFormat::kRGBA_8888 ||
             format == viz::SinglePlaneFormat::kRGBX_8888 ||
             format == viz::SinglePlaneFormat::kBGRA_8888 ||
             format == viz::SinglePlaneFormat::kBGRX_8888;
    case gfx::BufferUsage::SCANOUT_CPU_READ_WRITE:
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
    case gfx::BufferUsage::SCANOUT_VDA_WRITE:
    case gfx::BufferUsage::PROTECTED_SCANOUT:
    case gfx::BufferUsage::PROTECTED_SCANOUT_VDA_WRITE:
    case gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE:
    case gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE:
    case gfx::BufferUsage::SCANOUT_VEA_CPU_READ:
    case gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE:
    case gfx::BufferUsage::SCANOUT_FRONT_RENDERING:
      return false;
  }
  NOTREACHED();
#else
  return false;
#endif
}

}  // namespace gpu
