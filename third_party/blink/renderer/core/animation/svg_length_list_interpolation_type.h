// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SVG_LENGTH_LIST_INTERPOLATION_TYPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SVG_LENGTH_LIST_INTERPOLATION_TYPE_H_

#include "third_party/blink/renderer/core/animation/svg_interpolation_type.h"
#include "third_party/blink/renderer/core/svg/svg_length.h"

namespace blink {

enum class SVGLengthMode;

class SVGLengthListInterpolationType : public SVGInterpolationType {
 public:
  SVGLengthListInterpolationType(const QualifiedName& attribute)
      : SVGInterpolationType(attribute),
        unit_mode_(SVGLength::LengthModeForAnimatedLengthAttribute(attribute)),
        negative_values_forbidden_(
            SVGLength::NegativeValuesForbiddenForAnimatedLengthAttribute(
                attribute)) {}

 private:
  InterpolationValue MaybeConvertNeutral(const InterpolationValue& underlying,
                                         ConversionCheckers&) const final;
  InterpolationValue MaybeConvertSVGValue(
      const SVGPropertyBase& svg_value) const final;
  PairwiseInterpolationValue MaybeMergeSingles(
      InterpolationValue&& start,
      InterpolationValue&& end) const final;
  void Composite(UnderlyingValueOwner&,
                 double underlying_fraction,
                 const InterpolationValue&,
                 double interpolation_fraction) const final;
  SVGPropertyBase* AppliedSVGValue(const InterpolableValue&,
                                   const NonInterpolableValue*) const final;

  const SVGLengthMode unit_mode_;
  const bool negative_values_forbidden_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SVG_LENGTH_LIST_INTERPOLATION_TYPE_H_
