// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_ANIMATED_COLOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_ANIMATED_COLOR_H_

#include "cc/paint/paint_color.h"
#include "third_party/blink/renderer/core/animation/inherited_animations/deferred_property_utils.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/style/animated_value.h"
#include "third_party/blink/renderer/platform/graphics/color.h"

namespace blink {

// A resolved `Color` from style with an optional `AnimatedSource`. Converts to
// `cc::PaintColor` for painting.
class CORE_EXPORT AnimatedColor : public AnimatedValue<Color> {
 public:
  using AnimatedValue<Color>::AnimatedValue;

  // Returns false if it has a deferred key, so transparent colors still paint.
  bool IsFullyTransparent() const {
    return value.IsFullyTransparent() &&
           (!source.IsValid() || !ToDeferredPropertyKey(source, property));
  }

  cc::PaintColor ToPaintColor() const {
    return source.IsValid() ? ResolvePaintColor()
                            : cc::PaintColor{value.toSkColor4f()};
  }

 private:
  cc::PaintColor ResolvePaintColor() const;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_ANIMATED_COLOR_H_
