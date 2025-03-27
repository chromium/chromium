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
