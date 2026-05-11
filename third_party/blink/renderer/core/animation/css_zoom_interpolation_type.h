// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_ZOOM_INTERPOLATION_TYPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_ZOOM_INTERPOLATION_TYPE_H_

#include "third_party/blink/renderer/core/animation/css_number_interpolation_type.h"

namespace blink {

class CSSZoomInterpolationType : public CSSNumberInterpolationType {
 public:
  explicit CSSZoomInterpolationType(PropertyHandle property)
      : CSSNumberInterpolationType(property) {}

  InterpolationValue MaybeConvertValue(const CSSValue&,
                                       const StyleResolverState&,
                                       ConversionCheckers&) const override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_ZOOM_INTERPOLATION_TYPE_H_
