// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_FILTER_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_FILTER_HELPER_H_

#include "third_party/blink/renderer/platform/graphics/dark_mode_filter.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_image.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/skia/include/core/SkRect.h"

namespace blink {

class GraphicsContext;
class Image;

class PLATFORM_EXPORT DarkModeFilterHelper {
 public:
  // TODO(prashant.n): Move ElementRole to dark_mode_types.h.
  static SkColor ApplyToColorIfNeeded(GraphicsContext* context,
                                      SkColor color,
                                      DarkModeFilter::ElementRole role);
  static void ApplyToImageIfNeeded(GraphicsContext* context,
                                   Image* image,
                                   cc::PaintFlags* flags,
                                   const SkRect& src,
                                   const SkRect& dst);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_FILTER_HELPER_H_
