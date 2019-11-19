// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_var_cycle_interpolation_type.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/animation/css_interpolation_environment.h"
#include "third_party/blink/renderer/core/animation/string_keyframe.h"
#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/property_registration.h"
#include "third_party/blink/renderer/core/css/resolver/css_variable_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder.h"
#include "third_party/blink/renderer/core/css/resolver/style_cascade.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

class CycleChecker : public InterpolationType::ConversionChecker {
 public:
  CycleChecker(const CSSCustomPropertyDeclaration& declaration,
               bool cycle_detected)
      : declaration_(declaration), cycle_detected_(cycle_detected) {}

 private:
  bool IsValid(const InterpolationEnvironment& environment,
               const InterpolationValue&) const final {
    const auto& css_environment = ToCSSInterpolationEnvironment(environment);
    bool cycle_detected = false;
    if (RuntimeEnabledFeatures::CSSCascadeEnabled()) {
      cycle_detected = !css_environment.Resolve(
          PropertyHandle(declaration_->GetName()), declaration_);
    } else {
      css_environment.VariableResolver().ResolveCustomPropertyAnimationKeyframe(
          *declaration_, cycle_detected);
    }
    return cycle_detected == cycle_detected_;
  }

  Persistent<const CSSCustomPropertyDeclaration> declaration_;
  const bool cycle_detected_;
};

CSSVarCycleInterpolationType::CSSVarCycleInterpolationType(
    const PropertyHandle& property,
    const PropertyRegistration& registration)
    : InterpolationType(property), registration_(registration) {
  DCHECK(property.IsCSSCustomProperty());
}

static InterpolationValue CreateCycleDetectedValue() {
  return InterpolationValue(std::make_unique<InterpolableList>(0));
}

InterpolationValue CSSVarCycleInterpolationType::MaybeConvertSingle(
    const PropertySpecificKeyframe& keyframe,
    const InterpolationEnvironment& environment,
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  const auto& declaration = *To<CSSCustomPropertyDeclaration>(
      ToCSSPropertySpecificKeyframe(keyframe).Value());
  DCHECK_EQ(GetProperty().CustomPropertyName(), declaration.GetName());
  if (!declaration.Value() || !declaration.Value()->NeedsVariableResolution()) {
    return nullptr;
  }

  const auto& css_environment = ToCSSInterpolationEnvironment(environment);

  bool cycle_detected = false;
  if (RuntimeEnabledFeatures::CSSCascadeEnabled()) {
    cycle_detected = !css_environment.Resolve(GetProperty(), &declaration);
  } else {
    css_environment.VariableResolver().ResolveCustomPropertyAnimationKeyframe(
        declaration, cycle_detected);
  }

  conversion_checkers.push_back(
      std::make_unique<CycleChecker>(declaration, cycle_detected));
  return cycle_detected ? CreateCycleDetectedValue() : nullptr;
}

static bool IsCycleDetected(const InterpolationValue& value) {
  return static_cast<bool>(value);
}

PairwiseInterpolationValue CSSVarCycleInterpolationType::MaybeConvertPairwise(
    const PropertySpecificKeyframe& start_keyframe,
    const PropertySpecificKeyframe& end_keyframe,
    const InterpolationEnvironment& environment,
    const InterpolationValue& underlying,
    ConversionCheckers& conversionCheckers) const {
  InterpolationValue start = MaybeConvertSingle(start_keyframe, environment,
                                                underlying, conversionCheckers);
  InterpolationValue end = MaybeConvertSingle(end_keyframe, environment,
                                              underlying, conversionCheckers);
  if (!IsCycleDetected(start) && !IsCycleDetected(end)) {
    return nullptr;
  }
  // If either keyframe has a cyclic dependency then the entire interpolation
  // unsets the custom property.
  if (!start) {
    start = CreateCycleDetectedValue();
  }
  if (!end) {
    end = CreateCycleDetectedValue();
  }
  return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                    std::move(end.interpolable_value));
}

InterpolationValue CSSVarCycleInterpolationType::MaybeConvertUnderlyingValue(
    const InterpolationEnvironment& environment) const {
  const ComputedStyle& style =
      ToCSSInterpolationEnvironment(environment).Style();
  DCHECK(!style.GetVariableData(GetProperty().CustomPropertyName()) ||
         !style.GetVariableData(GetProperty().CustomPropertyName())
              ->NeedsVariableResolution());
  return nullptr;
}

void CSSVarCycleInterpolationType::Apply(
    const InterpolableValue&,
    const NonInterpolableValue*,
    InterpolationEnvironment& environment) const {
  StyleBuilder::ApplyProperty(
      GetProperty().GetCSSPropertyName(),
      ToCSSInterpolationEnvironment(environment).GetState(),
      *MakeGarbageCollected<CSSCustomPropertyDeclaration>(
          GetProperty().CustomPropertyName(), CSSValueID::kUnset));
}

}  // namespace blink
