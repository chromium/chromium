// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/wrapped_sk_image_backing_factory.h"

#include "base/memory/scoped_refptr.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/wrapped_graphite_texture_backing.h"
#include "gpu/command_buffer/service/shared_image/wrapped_sk_image_backing.h"
#include "gpu/config/gpu_finch_features.h"
#include "skia/buildflags.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkTextureCompressionType.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "third_party/skia/include/gpu/GrTypes.h"

namespace gpu {
namespace {

constexpr uint32_t kSupportedUsage =
    SHARED_IMAGE_USAGE_DISPLAY_READ | SHARED_IMAGE_USAGE_DISPLAY_WRITE |
    SHARED_IMAGE_USAGE_RASTER | SHARED_IMAGE_USAGE_OOP_RASTERIZATION |
    SHARED_IMAGE_USAGE_CPU_UPLOAD | SHARED_IMAGE_USAGE_MIPMAP;

bool IsUsageSupported(uint32_t usage) {
  // Must have at least one of the supported usage flags.
  return usage & kSupportedUsage;
}

}  // namespace

WrappedSkImageBackingFactory::WrappedSkImageBackingFactory(
    scoped_refptr<SharedContextState> context_state)
    : SharedImageBackingFactory(kSupportedUsage),
      context_state_(std::move(context_state)),
      use_graphite_(context_state_->graphite_context()),
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
    std::string debug_label,
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
  if (use_graphite_) {
    auto backing = std::make_unique<WrappedGraphiteTextureBacking>(
        base::PassKey<WrappedSkImageBackingFactory>(), mailbox, format, size,
        color_space, surface_origin, alpha_type, usage, context_state_,
        /*is_thread_safe=*/false);
    if (!backing->Initialize()) {
      return nullptr;
    }
    return backing;
  }
  CHECK(context_state_->gr_context());
  auto backing = std::make_unique<WrappedSkImageBacking>(
      base::PassKey<WrappedSkImageBackingFactory>(), mailbox, format, size,
      color_space, surface_origin, alpha_type, usage, context_state_,
      /*is_thread_safe=*/is_thread_safe &&
          context_state_->GrContextIsVulkan() && is_drdc_enabled_);
  if (!backing->Initialize(debug_label)) {
    return nullptr;
  }
  return backing;
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
    std::string debug_label,
    base::span<const uint8_t> data) {
  if (use_graphite_) {
    auto backing = std::make_unique<WrappedGraphiteTextureBacking>(
        base::PassKey<WrappedSkImageBackingFactory>(), mailbox, format, size,
        color_space, surface_origin, alpha_type, usage, context_state_,
        /*is_thread_safe=*/false);
    if (!backing->InitializeWithData(data)) {
      return nullptr;
    }
    return backing;
  }
  CHECK(context_state_->gr_context());
  auto backing = std::make_unique<WrappedSkImageBacking>(
      base::PassKey<WrappedSkImageBackingFactory>(), mailbox, format, size,
      color_space, surface_origin, alpha_type, usage, context_state_,
      /*is_thread_safe=*/context_state_->GrContextIsVulkan() &&
          is_drdc_enabled_);
  if (!backing->InitializeWithData(debug_label, data)) {
    return nullptr;
  }
  return backing;
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
    std::string debug_label,
    gfx::GpuMemoryBufferHandle handle) {
  NOTREACHED_NORETURN();
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
    uint32_t usage,
    std::string debug_label) {
  NOTREACHED_NORETURN();
}

bool WrappedSkImageBackingFactory::IsSupported(
    uint32_t usage,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    bool thread_safe,
    gfx::GpuMemoryBufferType gmb_type,
    GrContextType gr_context_type,
    base::span<const uint8_t> pixel_data) {
  if (gmb_type != gfx::EMPTY_BUFFER) {
    return false;
  }

  if (!IsUsageSupported(usage)) {
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

  if (format == viz::SinglePlaneFormat::kLUMINANCE_8) {
    // WrappedSkImage does not support LUMINANCE_8. See
    // https://crbug.com/1252502 for details.
    return false;
  } else if (format == viz::SinglePlaneFormat::kALPHA_8) {
    // For ALPHA8 skia will pick format depending on context version and
    // extensions available and we'll have to match that format when we record
    // DDLs. To avoid matching logic here, fallback to other backings (e.g
    // GLTextureImageBacking) where we control what format was used.
    if (gr_context_type == GrContextType::kGL) {
      return false;
    }
  } else if (format == viz::SinglePlaneFormat::kBGRX_8888 ||
             format == viz::SinglePlaneFormat::kBGR_565) {
    // For BGRX_8888/BGR_565 there is no equivalent SkColorType. Skia will use
    // the RGBX_8888/RGB_565 color type on upload so R/B channels are reversed.
    if (usage & SHARED_IMAGE_USAGE_CPU_UPLOAD || !pixel_data.empty()) {
      return false;
    }
  }

  if (context_state_->context_lost()) {
    return false;
  }

  if (format.IsCompressed()) {
    if (pixel_data.empty()) {
      // ETC1 is only supported with initial pixel upload.
      return false;
    }
    // TODO(crbug.com/1430206): Enable once compressed formats are supported.
    if (use_graphite_) {
      return false;
    }
    auto backend_format = context_state_->gr_context()->compressedBackendFormat(
        SkTextureCompressionType::kETC1_RGB8);
    if (!backend_format.isValid()) {
      return false;
    }
    return true;
  }

  // TODO(b/281151641): Check for formats are supported with graphite.
  if (context_state_->gr_context()) {
    // Check that skia-ganesh can create the required backend textures.
    for (int plane = 0; plane < format.NumberOfPlanes(); ++plane) {
      SkColorType color_type =
          viz::ToClosestSkColorType(/*gpu_compositing=*/true, format, plane);
      auto backend_format = context_state_->gr_context()->defaultBackendFormat(
          color_type, GrRenderable::kYes);
      if (!backend_format.isValid()) {
        return false;
      }
    }
  }

  return true;
}

}  // namespace gpu
