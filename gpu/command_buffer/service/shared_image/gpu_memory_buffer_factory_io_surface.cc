// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/gpu_memory_buffer_factory_io_surface.h"

#include <vector>

#include "base/apple/mach_logging.h"
#include "base/logging.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/numerics/safe_conversions.h"
#include "base/unguessable_token.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/service/shared_image/iosurface_image_backing_factory.h"
#include "gpu/ipc/common/gpu_client_ids.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/mac/io_surface.h"

namespace gpu {

GpuMemoryBufferFactoryIOSurface::GpuMemoryBufferFactoryIOSurface() = default;
GpuMemoryBufferFactoryIOSurface::~GpuMemoryBufferFactoryIOSurface() = default;

gfx::GpuMemoryBufferHandle
GpuMemoryBufferFactoryIOSurface::CreateNativeGmbHandle(
    const gfx::Size& size,
    viz::SharedImageFormat format,
    gfx::BufferUsage usage) {
  return IOSurfaceImageBackingFactory::CreateGpuMemoryBufferHandle(size,
                                                                   format);
}

}  // namespace gpu
