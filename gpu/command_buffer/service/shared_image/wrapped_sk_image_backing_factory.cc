// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/wrapped_sk_image_backing_factory.h"

#include "base/memory/scoped_refptr.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/wrapped_sk_image_backing.h"
#include "gpu/config/gpu_finch_features.h"
#include "skia/buildflags.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrTypes.h"

namespace gpu {

WrappedSkImageBackingFactory::WrappedSkImageBackingFactory(
    scoped_refptr<SharedContextState> context_state)
    : context_state_(std::move(context_state)),
      is_drdc_enabled_(
          features::IsDrDcEnabled() &&
          !context_state_->feature_info()->workarounds().disable_drdc) {}

WrappedSkImageBackingFactory::~WrappedSkImageBackingFactory() = default;

std::unique_ptr<SharedImageBacking>
WrappedSkImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    bool is_thread_safe) {
  // Ensure that the backing is treated as thread safe only when DrDc is enabled
  // for vulkan context.
  // TODO(vikassoni): Wire |is_thread_safe| flag in remaining
  // CreateSharedImage() factory methods also. Without this flag, backing will
  // always be considered as thread safe when DrDc is enabled for vulkan mode
  // even though it might be used on a single thread (RenderPass for example).
  // That should be fine for now since we do not have/use any locks in backing.
  DCHECK(!is_thread_safe ||
         (context_state_->GrContextIsVulkan() && is_drdc_enabled_));
  auto texture = std::make_unique<WrappedSkImageBacking>(
      base::PassKey<WrappedSkImageBackingFactory>(), mailbox, format, size,
      color_space, surface_origin, alpha_type, usage, context_state_,
      /*is_thread_safe=*/is_thread_safe &&
          context_state_->GrContextIsVulkan() && is_drdc_enabled_);
  if (!texture->Initialize()) {
    return nullptr;
  }
  return texture;
}

std::unique_ptr<SharedImageBacking>
WrappedSkImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    base::span<const uint8_t> data) {
  auto texture = std::make_unique<WrappedSkImageBacking>(
      base::PassKey<WrappedSkImageBackingFactory>(), mailbox, format, size,
      color_space, surface_origin, alpha_type, usage, context_state_,
      /*is_thread_safe=*/context_state_->GrContextIsVulkan() &&
          is_drdc_enabled_);
  if (!texture->InitializeWithData(data, /*stride=*/0)) {
    return nullptr;
  }
  return texture;
}

std::unique_ptr<SharedImageBacking>
WrappedSkImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat buffer_format,
    gfx::BufferPlane plane,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage) {
  NOTREACHED();
  return nullptr;
}

bool WrappedSkImageBackingFactory::CanUseWrappedSkImage(
    uint32_t usage,
    GrContextType gr_context_type) const {
  // Ignore for mipmap usage.
  usage &= ~SHARED_IMAGE_USAGE_MIPMAP;
  auto kWrappedSkImageUsage =
      SHARED_IMAGE_USAGE_DISPLAY_READ | SHARED_IMAGE_USAGE_DISPLAY_WRITE |
      SHARED_IMAGE_USAGE_RASTER | SHARED_IMAGE_USAGE_OOP_RASTERIZATION |
      SHARED_IMAGE_USAGE_CPU_UPLOAD;
  return (usage & kWrappedSkImageUsage) && !(usage & ~kWrappedSkImageUsage);
}

bool WrappedSkImageBackingFactory::IsSupported(
    uint32_t usage,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    bool thread_safe,
    gfx::GpuMemoryBufferType gmb_type,
    GrContextType gr_context_type,
    base::span<const uint8_t> pixel_data) {
  if (format.is_multi_plane()) {
    return false;
  }
  // Note that this backing support thread safety only for vulkan mode because
  // the underlying vulkan resources like vulkan images can be shared across
  // multiple vulkan queues. Also note that this backing currently only supports
  // thread safety for DrDc mode where both gpu main and drdc thread uses/shared
  // a single vulkan queue to submit work and hence do not need to synchronize
  // the reads/writes using semaphores. For this backing to support thread
  // safety across multiple queues, we need to synchronize the reads/writes via
  // semaphores.
  if (thread_safe &&
      (!is_drdc_enabled_ || gr_context_type != GrContextType::kVulkan)) {
    return false;
  }

  // Currently, WrappedSkImage does not support LUMINANCE_8 format and this
  // format is used for single channel planes. See https://crbug.com/1252502 for
  // more details.
  if (format == viz::SinglePlaneFormat::kLUMINANCE_8) {
    return false;
  }

  if (!CanUseWrappedSkImage(usage, gr_context_type)) {
    return false;
  }
  if (gmb_type != gfx::EMPTY_BUFFER) {
    return false;
  }

  return true;
}

}  // namespace gpu
