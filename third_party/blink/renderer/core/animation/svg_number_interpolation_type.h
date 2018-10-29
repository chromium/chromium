// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SVG_NUMBER_INTERPOLATION_TYPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SVG_NUMBER_INTERPOLATION_TYPE_H_

#include "third_party/blink/renderer/core/animation/svg_interpolation_type.h"
#include "third_party/blink/renderer/core/svg_names.h"

namespace blink {

class SVGNumberInterpolationType : public SVGInterpolationType {
 public:
  SVGNumberInterpolationType(const QualifiedName& attribute)
      : SVGInterpolationType(attribute),
        is_non_negative_(attribute == svg_names::kPathLengthAttr) {}

 private:
  InterpolationValue MaybeConvertNeutral(const InterpolationValue& underlying,
                                         ConversionCheckers&) const final;
  InterpolationValue MaybeConvertSVGValue(
      const SVGPropertyBase& svg_value) const final;
  SVGPropertyBase* AppliedSVGValue(const InterpolableValue&,
                                   const NonInterpolableValue*) const final;

  bool is_non_negative_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SVG_NUMBER_INTERPOLATION_TYPE_H_
