// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_factory_raw_draw.h"

#include "base/logging.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image_backing_raw_draw.h"

namespace gpu {

SharedImageBackingFactoryRawDraw::SharedImageBackingFactoryRawDraw() = default;

SharedImageBackingFactoryRawDraw::~SharedImageBackingFactoryRawDraw() = default;

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryRawDraw::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    bool is_thread_safe) {
  DCHECK(is_thread_safe);
  auto texture = std::make_unique<SharedImageBackingRawDraw>(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage);
  return texture;
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryRawDraw::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    base::span<const uint8_t> data) {
  NOTREACHED() << "Not supported";
  return nullptr;
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryRawDraw::CreateSharedImage(
    const Mailbox& mailbox,
    int client_id,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat buffer_format,
    gfx::BufferPlane plane,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage) {
  NOTREACHED() << "Not supported";
  return nullptr;
}

bool SharedImageBackingFactoryRawDraw::CanUseRawDrawBacking(
    uint32_t usage,
    GrContextType gr_context_type) const {
  // Ignore for mipmap usage.
  usage &= ~SHARED_IMAGE_USAGE_MIPMAP;

  auto kRawDrawBackingUsage =
      SHARED_IMAGE_USAGE_DISPLAY | SHARED_IMAGE_USAGE_RASTER |
      SHARED_IMAGE_USAGE_OOP_RASTERIZATION | SHARED_IMAGE_USAGE_RAW_DRAW;
  return usage == kRawDrawBackingUsage;
}

bool SharedImageBackingFactoryRawDraw::IsSupported(
    uint32_t usage,
    viz::ResourceFormat format,
    bool thread_safe,
    gfx::GpuMemoryBufferType gmb_type,
    GrContextType gr_context_type,
    bool* allow_legacy_mailbox,
    bool is_pixel_used) {
  if (!CanUseRawDrawBacking(usage, gr_context_type)) {
    return false;
  }

  if (is_pixel_used) {
    return false;
  }

  if (gmb_type != gfx::EMPTY_BUFFER) {
    return false;
  }

  *allow_legacy_mailbox = false;
  return true;
}

}  // namespace gpu
