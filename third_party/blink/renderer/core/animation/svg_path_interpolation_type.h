// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SVG_PATH_INTERPOLATION_TYPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SVG_PATH_INTERPOLATION_TYPE_H_

#include "third_party/blink/renderer/core/animation/svg_interpolation_type.h"

namespace blink {

class SVGPathInterpolationType : public SVGInterpolationType {
 public:
  SVGPathInterpolationType(const QualifiedName& attribute)
      : SVGInterpolationType(attribute) {}

 private:
  InterpolationValue MaybeConvertSVGValue(
      const SVGPropertyBase& svg_value) const final;
  InterpolationValue MaybeConvertNeutral(const InterpolationValue& underlying,
                                         ConversionCheckers&) const final;
  PairwiseInterpolationValue MaybeMergeSingles(
      InterpolationValue&& start,
      InterpolationValue&& end) const final;
  void Composite(UnderlyingValueOwner&,
                 double underlying_fraction,
                 const InterpolationValue&,
                 double interpolation_fraction) const final;
  SVGPropertyBase* AppliedSVGValue(const InterpolableValue&,
                                   const NonInterpolableValue*) const final;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SVG_PATH_INTERPOLATION_TYPE_H_
