// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_var_cycle_interpolation_type.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/animation/css_interpolation_environment.h"
#include "third_party/blink/renderer/core/animation/string_keyframe.h"
#include "third_party/blink/renderer/core/animation/underlying_value_owner.h"
#include "third_party/blink/renderer/core/css/css_unparsed_declaration_value.h"
#include "third_party/blink/renderer/core/css/css_unset_value.h"
#include "third_party/blink/renderer/core/css/property_registration.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder.h"
#include "third_party/blink/renderer/core/css/resolver/style_cascade.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

class CycleChecker : public InterpolationType::ConversionChecker {
 public:
  CycleChecker(const PropertyHandle& property,
               const CSSValue& value,
               const TreeScope* keyframe_tree_scope,
               bool cycle_detected)
      : property_(property),
        value_(value),
        keyframe_tree_scope_(keyframe_tree_scope),
        cycle_detected_(cycle_detected) {}

  void Trace(Visitor* visitor) const final {
    InterpolationType::ConversionChecker::Trace(visitor);
    visitor->Trace(value_);
    visitor->Trace(keyframe_tree_scope_);
  }

 private:
  bool IsValid(const CSSInterpolationEnvironment& environment,
               const InterpolationValue&) const final {
    bool cycle_detected =
        !environment.Resolve(property_, value_, keyframe_tree_scope_);
    return cycle_detected == cycle_detected_;
  }

  PropertyHandle property_;
  Member<const CSSValue> value_;
  Member<const TreeScope> keyframe_tree_scope_;
  const bool cycle_detected_;
};

CSSVarCycleInterpolationType::CSSVarCycleInterpolationType(
    const PropertyHandle& property,
    const PropertyRegistration& registration)
    : InterpolationType(property), registration_(registration) {
  DCHECK(property.IsCSSCustomProperty());
}

static InterpolationValue CreateCycleDetectedValue() {
  return InterpolationValue(MakeGarbageCollected<InterpolableList>(0));
}

InterpolationValue CSSVarCycleInterpolationType::MaybeConvertSingle(
    const PropertySpecificKeyframe& keyframe,
    const CSSInterpolationEnvironment& environment,
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  const auto& property_specific = To<CSSPropertySpecificKeyframe>(keyframe);
  const CSSValue* value = property_specific.Value();
  const TreeScope* keyframe_tree_scope = property_specific.GetTreeScope();

  if (!value) {
    DCHECK(keyframe.IsNeutral());
    return nullptr;
  }

  // It is only possible to form a cycle if the value points to something else.
  // This is only possible with var(), or with revert-[layer] which may revert
  // to a value which contains var().
  if (const auto* declaration = DynamicTo<CSSUnparsedDeclarationValue>(value)) {
    if (!declaration->VariableDataValue()->NeedsVariableResolution()) {
      return nullptr;
    }
  } else if (!value->IsRevertValue() && !value->IsRevertLayerValue()) {
    return nullptr;
  }

  PropertyHandle property = GetProperty();
  bool cycle_detected =
      !environment.Resolve(property, value, keyframe_tree_scope);
  conversion_checkers.push_back(MakeGarbageCollected<CycleChecker>(
      property, *value, keyframe_tree_scope, cycle_detected));
  return cycle_detected ? CreateCycleDetectedValue() : nullptr;
}

static bool IsCycleDetected(const InterpolationValue& value) {
  return static_cast<bool>(value);
}

PairwiseInterpolationValue CSSVarCycleInterpolationType::MaybeConvertPairwise(
    const PropertySpecificKeyframe& start_keyframe,
    const PropertySpecificKeyframe& end_keyframe,
    const CSSInterpolationEnvironment& environment,
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
    const CSSInterpolationEnvironment& environment) const {
  const ComputedStyle& style = environment.BaseStyle();
  DCHECK(!style.GetVariableData(GetProperty().CustomPropertyName()) ||
         !style.GetVariableData(GetProperty().CustomPropertyName())
              ->NeedsVariableResolution());
  return nullptr;
}

void CSSVarCycleInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  underlying_value_owner.Set(this, value);
}

void CSSVarCycleInterpolationType::Apply(
    const InterpolableValue&,
    const NonInterpolableValue*,
    CSSInterpolationEnvironment& environment) const {
  StyleBuilder::ApplyProperty(GetProperty().GetCSSPropertyName(),
                              environment.GetState(),
                              *cssvalue::CSSUnsetValue::Create());
}

}  // namespace blink
