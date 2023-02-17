// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/shared_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"

namespace gpu {

SharedImageBackingFactory::SharedImageBackingFactory(uint32_t valid_usages)
    : invalid_usages_(~valid_usages) {}

SharedImageBackingFactory::~SharedImageBackingFactory() = default;

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    gfx::GpuMemoryBufferHandle handle) {
  return nullptr;
}

base::WeakPtr<SharedImageBackingFactory>
SharedImageBackingFactory::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool SharedImageBackingFactory::CanCreateSharedImage(
    uint32_t usage,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    bool thread_safe,
    gfx::GpuMemoryBufferType gmb_type,
    GrContextType gr_context_type,
    base::span<const uint8_t> pixel_data) {
  // TODO(kylechar): Once existing usages are worked out just return false if
  // !usage_supported.
  bool usage_supported = (invalid_usages_ & usage) == 0;

  bool is_supported = IsSupported(usage, format, size, thread_safe, gmb_type,
                                  gr_context_type, pixel_data);

  if (!usage_supported) {
    // The factory should never report supported with an unsupported usage.
    CHECK(!is_supported) << " usage=" << CreateLabelForSharedImageUsage(usage)
                         << ", invalid_usages="
                         << CreateLabelForSharedImageUsage(invalid_usages_);
  }

  return is_supported;
}

void SharedImageBackingFactory::InvalidateWeakPtrsForTesting() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

}  // namespace gpu
