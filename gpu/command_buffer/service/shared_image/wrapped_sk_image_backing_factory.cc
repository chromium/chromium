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
#include "gpu/command_buffer/service/dawn_context_provider.h"
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
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"

namespace gpu {
namespace {
constexpr SharedImageUsageSet kSupportedUsage =
    SHARED_IMAGE_USAGE_DISPLAY_READ | SHARED_IMAGE_USAGE_DISPLAY_WRITE |
    SHARED_IMAGE_USAGE_RASTER_READ | SHARED_IMAGE_USAGE_RASTER_WRITE |
    SHARED_IMAGE_USAGE_CPU_UPLOAD | SHARED_IMAGE_USAGE_MIPMAP;

SharedImageUsageSet GetSupportedUsage(const SharedContextState* context_state) {
#if BUILDFLAG(SKIA_USE_DAWN) && !BUILDFLAG(IS_ANDROID)
  // We support WebGL and WebGPU fallback when using Graphite Dawn Vulkan or
  // D3D12. Except on Android where AHardwareBufferImageBackingFactory is used
  // for interop with WebGL and WebGPU.
  constexpr SharedImageUsageSet kGraphiteDawnFallbackUsage =
      SHARED_IMAGE_USAGE_GLES2_READ | SHARED_IMAGE_USAGE_GLES2_WRITE |
      SHARED_IMAGE_USAGE_HIGH_PERFORMANCE_GPU |
      SHARED_IMAGE_USAGE_WEBGPU_READ | SHARED_IMAGE_USAGE_WEBGPU_WRITE |
      SHARED_IMAGE_USAGE_WEBGPU_SWAP_CHAIN_TEXTURE;

  if (context_state->IsGraphiteDawn()) {
    switch (context_state->dawn_context_provider()->backend_type()) {
      case wgpu::BackendType::Vulkan:
        return kSupportedUsage | kGraphiteDawnFallbackUsage;
      default:
        break;
    }
  }
#endif
  return kSupportedUsage;
}

bool GraphiteSupportsCompressedTextures(
    const SharedContextState* context_state) {
#if BUILDFLAG(SKIA_USE_DAWN)
  // TODO(b/281151641): Query graphite instead of dawn to see if compressed
  // textures are supported.
  if (context_state->IsGraphiteDawn()) {
    return context_state->dawn_context_provider()->SupportsFeature(
        wgpu::FeatureName::TextureCompressionETC2);
  }
#endif
  return false;
}

}  // namespace

WrappedSkImageBackingFactory::WrappedSkImageBackingFactory(
    scoped_refptr<SharedContextState> context_state)
    : SharedImageBackingFactory(GetSupportedUsage(context_state.get())),
      context_state_(std::move(context_state)),
      use_graphite_(context_state_->graphite_shared_context()),
      is_drdc_enabled_(context_state_->is_drdc_enabled()),
      graphite_supports_compressed_textures_(
          GraphiteSupportsCompressedTextures(context_state_.get())) {}

WrappedSkImageBackingFactory::~WrappedSkImageBackingFactory() = default;

std::unique_ptr<SharedImageBacking>
WrappedSkImageBackingFactory::CreateSharedImage(const Mailbox& mailbox,
                                                const SharedImageInfo& si_info,
                                                SurfaceHandle surface_handle,
                                                bool is_thread_safe) {
  if (use_graphite_) {
    auto backing = std::make_unique<WrappedGraphiteTextureBacking>(
        base::PassKey<WrappedSkImageBackingFactory>(), mailbox, si_info,
        context_state_, is_thread_safe);
    if (!backing->Initialize()) {
      return nullptr;
    }
    return backing;
  }
  CHECK(context_state_->gr_context());
  auto backing = std::make_unique<WrappedSkImageBacking>(
      base::PassKey<WrappedSkImageBackingFactory>(), mailbox, si_info,
      context_state_, is_thread_safe);
  if (!backing->Initialize(si_info.debug_label)) {
    return nullptr;
  }
  return backing;
}

std::unique_ptr<SharedImageBacking>
WrappedSkImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    const SharedImageInfo& si_info,
    bool is_thread_safe,
    base::span<const uint8_t> data) {
  if (use_graphite_) {
    auto backing = std::make_unique<WrappedGraphiteTextureBacking>(
        base::PassKey<WrappedSkImageBackingFactory>(), mailbox, si_info,
        context_state_, is_thread_safe);
    if (!backing->InitializeWithData(data)) {
      return nullptr;
    }
    return backing;
  }
  CHECK(context_state_->gr_context());
  auto backing = std::make_unique<WrappedSkImageBacking>(
      base::PassKey<WrappedSkImageBackingFactory>(), mailbox, si_info,
      context_state_, is_thread_safe);
  if (!backing->InitializeWithData(si_info.debug_label, data)) {
    return nullptr;
  }
  return backing;
}

bool WrappedSkImageBackingFactory::IsSupported(
    SharedImageUsageSet usage,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    bool thread_safe,
    gfx::GpuMemoryBufferType gmb_type,
    GrContextType gr_context_type,
    base::span<const uint8_t> pixel_data) {
  if (gmb_type != gfx::EMPTY_BUFFER) {
    return false;
  }

  if (!GetSupportedUsage(context_state_.get()).HasAll(usage)) {
    return false;
  }

  // Note that this backing support thread safety only for DawnMetal or Vulkan
  // mode because DawnMetal is already thread safe and the underlying vulkan
  // resources like vulkan images can be shared across multiple vulkan queues.
  // Also note that this backing currently only supports thread safety for DrDc
  // mode where both gpu main and drdc thread uses/shared a single vulkan queue
  // to submit work and hence do not need to synchronize the reads/writes using
  // semaphores. For this backing to support thread safety across multiple
  // queues, we need to synchronize the reads/writes via semaphores.
  if (thread_safe) {
    bool is_vulkan = gr_context_type == GrContextType::kVulkan ||
                     context_state_->IsGraphiteDawnVulkan();
    bool is_dawn_metal = context_state_->IsGraphiteDawnMetal();
    if (!is_drdc_enabled_ || (!is_vulkan && !is_dawn_metal)) {
      return false;
    }
  }

  if (format == viz::SinglePlaneFormat::kALPHA_8) {
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
    if (usage.Has(SHARED_IMAGE_USAGE_CPU_UPLOAD) || !pixel_data.empty()) {
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
    if (use_graphite_) {
      return graphite_supports_compressed_textures_;
    }
    auto backend_format = context_state_->gr_context()->compressedBackendFormat(
        SkTextureCompressionType::kETC1_RGB8);
    return backend_format.isValid();
  }

  // TODO(b/281151641): Check for formats are supported with graphite.
  if (context_state_->gr_context()) {
    // Check that skia-ganesh can create the required backend textures.
    for (int plane = 0; plane < format.NumberOfPlanes(); ++plane) {
      SkColorType color_type = viz::ToClosestSkColorType(format, plane);
      // For ALPHA8 skia will pick format depending on context version and
      // extensions available and we'll have to match that format when we record
      // DDLs. To avoid matching logic here, fallback to other backings (e.g
      // GLTextureImageBacking) where we control what format was used.
      if ((color_type == kAlpha_8_SkColorType ||
           color_type == kR8_unorm_SkColorType) &&
          context_state_->feature_info()->workarounds().r8_egl_images_broken) {
        return false;
      }
      auto backend_format = context_state_->gr_context()->defaultBackendFormat(
          color_type, GrRenderable::kYes);
      if (!backend_format.isValid()) {
        return false;
      }
    }
  }

  return true;
}

SharedImageBackingType WrappedSkImageBackingFactory::GetBackingType() {
  if (use_graphite_) {
    return SharedImageBackingType::kWrappedGraphiteTexture;
  } else {
    return SharedImageBackingType::kWrappedSkImage;
  }
}

bool WrappedSkImageBackingFactory::IsSupportedForAccessStream(
    SharedImageAccessStream stream,
    viz::SharedImageFormat format,
    const AccessParams* params) const {
  // `WrappedSkImageBackingFactory` is strictly bound to the
  // `SharedContextState` it was created with (the GPU main thread). If a
  // request is made from a different thread/context, we must return false early
  // to prevent `SharedImageFactory` from calling `IsSupported`, which would
  // unsafely access the thread-bound `context_state_`. Note that this currently
  // restricts this factory to only be selected and used on the GPU main thread.
  // If it's refactored in the future to remove its dependency on
  // `SharedContextState` in `IsSupported`, this restriction can be relaxed.
  if (params && params->context_state &&
      params->context_state != context_state_) {
    return false;
  }

  if (use_graphite_) {
    // We create a temporary backing just to check for support.
    // TODO(crbug.com/394385381): Consider refactoring this to not require a
    // context_state or a backing instance.
    AccessParams access_params = params ? *params : AccessParams();
    bool supported = WrappedGraphiteTextureBacking::CheckSupportForAccessStream(
        stream, format, access_params);
    return supported;
  }
  return true;
}

}  // namespace gpu
