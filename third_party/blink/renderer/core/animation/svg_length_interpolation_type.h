// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SVG_LENGTH_INTERPOLATION_TYPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SVG_LENGTH_INTERPOLATION_TYPE_H_

#include <memory>
#include "third_party/blink/renderer/core/animation/svg_interpolation_type.h"
#include "third_party/blink/renderer/core/svg/svg_length.h"

namespace blink {

class SVGLengthContext;
enum class SVGLengthMode;

class SVGLengthInterpolationType : public SVGInterpolationType {
 public:
  SVGLengthInterpolationType(const QualifiedName& attribute)
      : SVGInterpolationType(attribute),
        unit_mode_(SVGLength::LengthModeForAnimatedLengthAttribute(attribute)),
        negative_values_forbidden_(
            SVGLength::NegativeValuesForbiddenForAnimatedLengthAttribute(
                attribute)) {}

  static std::unique_ptr<InterpolableValue> NeutralInterpolableValue();
  static InterpolationValue MaybeConvertSVGLength(const SVGLength&);
  static SVGLength* ResolveInterpolableSVGLength(
      const InterpolableValue&,
      const SVGLengthContext&,
      SVGLengthMode,
      bool negative_values_forbidden);

 private:
  InterpolationValue MaybeConvertNeutral(const InterpolationValue& underlying,
                                         ConversionCheckers&) const final;
  InterpolationValue MaybeConvertSVGValue(
      const SVGPropertyBase& svg_value) const final;
  SVGPropertyBase* AppliedSVGValue(const InterpolableValue&,
                                   const NonInterpolableValue*) const final;
  void Apply(const InterpolableValue&,
             const NonInterpolableValue*,
             InterpolationEnvironment&) const final;

  const SVGLengthMode unit_mode_;
  const bool negative_values_forbidden_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SVG_LENGTH_INTERPOLATION_TYPE_H_
