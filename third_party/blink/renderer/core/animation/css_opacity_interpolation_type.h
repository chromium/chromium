// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_OPACITY_INTERPOLATION_TYPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_OPACITY_INTERPOLATION_TYPE_H_

#include "third_party/blink/renderer/core/animation/css_number_interpolation_type.h"
#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

class CORE_EXPORT CSSOpacityInterpolationType
    : public CSSNumberInterpolationType {
 public:
  explicit CSSOpacityInterpolationType(PropertyHandle property)
      : CSSNumberInterpolationType(property) {}

  InterpolationValue MaybeConvertValue(const CSSValue&,
                                       const StyleResolverState&,
                                       ConversionCheckers&) const override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_OPACITY_INTERPOLATION_TYPE_H_
