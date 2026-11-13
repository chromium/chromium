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

class BaseRenderingContext2D;
class CanvasResourceSharedImage;

// * Subclass of CanvasResourceProvider that is specialized for usage
// * by Canvas2D.
class PLATFORM_EXPORT Canvas2DResourceProvider {
 public:
  explicit Canvas2DResourceProvider(
      std::unique_ptr<gpu::SharedImagePool<CanvasResourceSharedImage>>
          image_pool);

  ~Canvas2DResourceProvider();

  void SetResourceRecyclingEnabled(bool value);

 private:
  friend class BaseRenderingContext2D;

  void ClearUnusedResources();

  // If this instance is single-buffered or |resource_recycling_enabled_| is
  // false, |image_pool_| will not recycle resources.
  std::unique_ptr<gpu::SharedImagePool<CanvasResourceSharedImage>> image_pool_;

  bool resource_recycling_enabled_ = true;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_2D_RESOURCE_PROVIDER_H_
