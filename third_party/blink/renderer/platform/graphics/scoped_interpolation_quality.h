// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_SCOPED_INTERPOLATION_QUALITY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_SCOPED_INTERPOLATION_QUALITY_H_

#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// Helper to update the interpolation quality setting of a GraphicsContext
// within the current scope. Be careful when mixing with other GraphicsContext
// mechanisms that save/restore state (like GraphicsContextStateSaver or the
// Save/Restore methods) to ensure the restoration behavior is the expected
// one.
class ScopedInterpolationQuality {
  STACK_ALLOCATED();

 public:
  ScopedInterpolationQuality(GraphicsContext& context,
                             InterpolationQuality interpolation_quality)
      : context_(context),
        previous_interpolation_quality_(context.ImageInterpolationQuality()) {
    if (previous_interpolation_quality_ != interpolation_quality)
      context_.SetImageInterpolationQuality(interpolation_quality);
  }

  ~ScopedInterpolationQuality() {
    if (previous_interpolation_quality_ != context_.ImageInterpolationQuality())
      context_.SetImageInterpolationQuality(previous_interpolation_quality_);
  }

 private:
  GraphicsContext& context_;
  const InterpolationQuality previous_interpolation_quality_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_SCOPED_INTERPOLATION_QUALITY_H_
