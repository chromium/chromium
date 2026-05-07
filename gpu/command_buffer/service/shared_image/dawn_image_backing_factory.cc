// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/dawn_image_backing_factory.h"

#include "gpu/command_buffer/service/shared_image/dawn_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "third_party/skia/include/core/SkPixmap.h"

namespace gpu {
namespace {

constexpr SharedImageUsageSet kSupportedUsage =
    SHARED_IMAGE_USAGE_WEBGPU_READ | SHARED_IMAGE_USAGE_WEBGPU_WRITE;
}  // namespace

DawnImageBackingFactory::DawnImageBackingFactory()
    : SharedImageBackingFactory(kSupportedUsage) {}

DawnImageBackingFactory::~DawnImageBackingFactory() = default;

std::unique_ptr<SharedImageBacking> DawnImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    const SharedImageInfo& si_info,
    SurfaceHandle surface_handle,
    bool is_thread_safe) {
  DCHECK(!is_thread_safe);
  return std::make_unique<DawnImageBacking>(mailbox, si_info);
}

bool DawnImageBackingFactory::IsSupported(
    SharedImageUsageSet usage,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    bool thread_safe,
    gfx::GpuMemoryBufferType gmb_type,
    GrContextType gr_context_type,
    base::span<const uint8_t> pixel_data) {
  if ((gmb_type != gfx::EMPTY_BUFFER) || !pixel_data.empty() || thread_safe) {
    return false;
  }

  return true;
}

SharedImageBackingType DawnImageBackingFactory::GetBackingType() {
  return SharedImageBackingType::kDawn;
}

bool DawnImageBackingFactory::IsSupportedForAccessStream(
    SharedImageAccessStream stream,
    viz::SharedImageFormat format,
    const AccessParams* params) const {
  AccessParams default_params;
  return DawnImageBacking::CheckSupportForAccessStream(
      stream, params ? *params : default_params, /*backing_device=*/{});
}

}  // namespace gpu
