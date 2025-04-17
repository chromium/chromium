// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CANVAS_INTERVENTIONS_CANVAS_INTERVENTIONS_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CANVAS_INTERVENTIONS_CANVAS_INTERVENTIONS_HELPER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_host.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"

namespace blink {

enum class CanvasNoiseReason {
  kAllConditionsMet = 0,
  kNoRenderContext = 1,
  kNoTrigger = 2,
  kNo2d = 4,
  kNoGpu = 8,
  kNotEnabledInMode = 16,
  kNoExecutionContext = 32,
  kMaxValue = kNoExecutionContext
};

inline constexpr CanvasNoiseReason operator|(CanvasNoiseReason a,
                                             CanvasNoiseReason b) {
  return static_cast<CanvasNoiseReason>(static_cast<int>(a) |
                                        static_cast<int>(b));
}
inline constexpr CanvasNoiseReason& operator|=(CanvasNoiseReason& a,
                                               CanvasNoiseReason b) {
  a = a | b;
  return a;
}

class CORE_EXPORT CanvasInterventionsHelper {
 public:
  enum class CanvasInterventionType {
    kNone,
    kNoise,
  };

  // If allowed, performs noising on a copy of the snapshot StaticBitmapImage
  // and returns the noised snapshot, otherwise it will return the original
  // inputted snapshot.
  static bool MaybeNoiseSnapshot(CanvasRenderingContext* rendering_context,
                                 ExecutionContext* execution_context,
                                 scoped_refptr<StaticBitmapImage>& snapshot,
                                 RasterMode raster_mode);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CANVAS_INTERVENTIONS_CANVAS_INTERVENTIONS_HELPER_H_
