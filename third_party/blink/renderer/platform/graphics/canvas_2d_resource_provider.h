// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_2D_RESOURCE_PROVIDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_2D_RESOURCE_PROVIDER_H_

#include <algorithm>
#include <memory>
#include <optional>

#include "base/byte_size.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "cc/paint/paint_op_buffer.h"
#include "cc/raster/playback_image_provider.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/canvas_snapshot_info.h"
#include "third_party/blink/renderer/platform/graphics/flush_for_image_listener.h"
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"

namespace blink {

PLATFORM_EXPORT BASE_DECLARE_FEATURE(kCanvas2DReclaimUnusedResources);
PLATFORM_EXPORT BASE_DECLARE_FEATURE(kAppendCpuUsages);
PLATFORM_EXPORT BASE_DECLARE_FEATURE(kCanvasResourceIsWebGPUCompatible);

class BaseRenderingContext2D;
class CanvasResourceSharedImage;
class Canvas2DResourceProvider;
class CanvasImageProvider;
class WebGraphicsSharedImageInterfaceProvider;

// Specifies whether the provider should rasterize paint commands on the CPU
// or GPU. This is used to support software raster with GPU compositing.
enum class RasterMode {
  kGPU,
  kCPU,
};

// Canvas2DResourceProvider
//==============================================================================
//
// Encapsulates a drawable graphics resource for Canvas2D.
// Canvas2DResourceProvider serves as a provider of drawable surfaces for 2D
// rendering with Skia.
//
// General usage:
//   1) Use the Create() static method to create an instance
//   2) use Canvas() to get a drawing interface
//   3) Call Snapshot() to acquire a bitmap with the rendered image in it.

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
enum class CanvasResourceProviderType {
  kTexture [[deprecated]] = 0,
  kBitmap = 1,
  kSharedBitmap [[deprecated]] = 2,
  kTextureGpuMemoryBuffer [[deprecated]] = 3,
  kBitmapGpuMemoryBuffer [[deprecated]] = 4,
  kSharedImage = 5,
  kDirectGpuMemoryBuffer [[deprecated]] = 6,
  kPassThrough [[deprecated]] = 7,
  kSwapChain [[deprecated]] = 8,
  kSkiaDawnSharedImage [[deprecated]] = 9,
  kExternalBitmap [[deprecated]] = 10,
  kMaxValue = kExternalBitmap,
};
#pragma GCC diagnostic pop

// * Subclass of CanvasResourceProvider that is specialized for usage
// * by Canvas2D.
class PLATFORM_EXPORT Canvas2DResourceProvider
    : public CanvasResourceSharedImage::Client {
 public:
  constexpr static base::TimeDelta kUnusedResourceExpirationTime =
      base::Seconds(5);

  Canvas2DResourceProvider(gfx::Size size,
                           viz::SharedImageFormat format,
                           SkAlphaType alpha_type,
                           const gfx::ColorSpace& color_space,
                           const gfx::HDRMetadata& hdr_metadata,
                           base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
                           bool is_accelerated,
                           gpu::SharedImageUsageSet shared_image_usage_flags);
  Canvas2DResourceProvider(
      gfx::Size size,
      viz::SharedImageFormat format,
      SkAlphaType alpha_type,
      const gfx::ColorSpace& color_space,
      const gfx::HDRMetadata& hdr_metadata,
      WebGraphicsSharedImageInterfaceProvider* shared_image_interface_provider);

  ~Canvas2DResourceProvider() override;

  bool unused_resources_reclaim_timer_is_running_for_testing() const;
  bool HasUnusedResourcesForTesting() const;
  bool IsSingleBuffered() const;

  bool IsAccelerated() const { return is_accelerated_; }
  bool IsSoftware() const { return image_pool_->GetImageInfo().is_software; }

  viz::SharedImageFormat GetSharedImageFormat() const {
    return image_pool_->GetImageInfo().format;
  }

  int NumInflightResourcesForTesting() const { return num_inflight_resources_; }

  void SetResourceRecyclingEnabled(bool value);

  CanvasResourceSharedImage* resource() {
    return static_cast<CanvasResourceSharedImage*>(resource_.get());
  }
  const CanvasResourceSharedImage* resource() const {
    return static_cast<const CanvasResourceSharedImage*>(resource_.get());
  }

 private:
  friend class BaseRenderingContext2D;

  void ClearUnusedResources();

  void OnResourceRefReturned(
      scoped_refptr<CanvasResourceSharedImage>&& resource) override;
  void OnDestroyResource() override { --num_inflight_resources_; }

  scoped_refptr<CanvasResourceSharedImage> NewOrRecycledResource();

  base::WeakPtr<Canvas2DResourceProvider> CreateWeakPtr();

  // The maximum number of in-flight resources waiting to be used for
  // recycling.
  static constexpr int kMaxRecycledCanvasResources = 3;

  // If this instance is single-buffered or |resource_recycling_enabled_| is
  // false, |image_pool_| will not recycle resources.
  std::unique_ptr<gpu::SharedImagePool<CanvasResourceSharedImage>> image_pool_;

  scoped_refptr<CanvasResourceSharedImage> resource_;

  const bool is_accelerated_;

  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper_;
  base::WeakPtr<WebGraphicsSharedImageInterfaceProvider>
      shared_image_interface_provider_;

  bool resource_recycling_enabled_ = true;
  int num_inflight_resources_ = 0;
  int max_inflight_resources_ = 0;

  gfx::HDRMetadata hdr_metadata_;

  base::WeakPtrFactory<Canvas2DResourceProvider> weak_ptr_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_2D_RESOURCE_PROVIDER_H_
