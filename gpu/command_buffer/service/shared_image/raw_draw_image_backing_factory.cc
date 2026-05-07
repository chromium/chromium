// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/raw_draw_image_backing_factory.h"

#include "base/logging.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image/raw_draw_image_backing.h"

namespace gpu {
// NOTE: These are the *exact* set of usages that the client must list in order
// for the RawDraw backing to be applied. The client must explicitly opt into
// using RawDraw, and that only in the expected context of rasterizing content
// into PaintOps to play back during compositing.
constexpr SharedImageUsageSet kRequiredUsage = SHARED_IMAGE_USAGE_DISPLAY_READ |
                                               SHARED_IMAGE_USAGE_RASTER_WRITE |
                                               SHARED_IMAGE_USAGE_RAW_DRAW;

RawDrawImageBackingFactory::RawDrawImageBackingFactory()
    : SharedImageBackingFactory(kRequiredUsage) {}

RawDrawImageBackingFactory::~RawDrawImageBackingFactory() = default;

std::unique_ptr<SharedImageBacking>
RawDrawImageBackingFactory::CreateSharedImage(const Mailbox& mailbox,
                                              const SharedImageInfo& si_info,
                                              SurfaceHandle surface_handle,
                                              bool is_thread_safe) {
  DCHECK(is_thread_safe);
  return std::make_unique<RawDrawImageBacking>(mailbox, si_info);
}

bool RawDrawImageBackingFactory::CanUseRawDrawImageBacking(
    SharedImageUsageSet usage,
    GrContextType gr_context_type) const {
  return usage == kRequiredUsage;
}

bool RawDrawImageBackingFactory::IsSupported(
    SharedImageUsageSet usage,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    bool thread_safe,
    gfx::GpuMemoryBufferType gmb_type,
    GrContextType gr_context_type,
    base::span<const uint8_t> pixel_data) {
  if (format.is_multi_plane()) {
    return false;
  }
  if (!CanUseRawDrawImageBacking(usage, gr_context_type)) {
    return false;
  }

  if (!pixel_data.empty()) {
    return false;
  }

  if (gmb_type != gfx::EMPTY_BUFFER) {
    return false;
  }

  return true;
}

SharedImageBackingType RawDrawImageBackingFactory::GetBackingType() {
  return SharedImageBackingType::kRawDraw;
}

}  // namespace gpu
