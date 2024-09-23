// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_interpolation_type.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/core/animation/css_interpolation_environment.h"
#include "third_party/blink/renderer/core/animation/string_keyframe.h"
#include "third_party/blink/renderer/core/css/anchor_evaluator.h"
#include "third_party/blink/renderer/core/css/computed_style_css_value_mapping.h"
#include "third_party/blink/renderer/core/css/css_inherited_value.h"
#include "third_party/blink/renderer/core/css/css_initial_value.h"
#include "third_party/blink/renderer/core/css/css_revert_layer_value.h"
#include "third_party/blink/renderer/core/css/css_revert_value.h"
#include "third_party/blink/renderer/core/css/css_unparsed_declaration_value.h"
#include "third_party/blink/renderer/core/css/css_unset_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/property_registration.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder.h"
#include "third_party/blink/renderer/core/css/resolver/style_cascade.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"

namespace blink {

// Generic checker for any value that needs resolution through
// CSSInterpolationEnvironment::Resolve (StyleCascade::Resolve).
//
// More specialized checkers (e.g. RevertChecker) may exist even though
// they could also be handled by this class (perhaps less efficiently).
//
// TODO(andruud): Unify this with some other checkers.
class ResolvedValueChecker : public CSSInterpolationType::ConversionChecker {
 public:
  ResolvedValueChecker(const PropertyHandle& property,
                       const CSSValue* unresolved_value,
                       const CSSValue* resolved_value)
      : property_(property),
        unresolved_value_(unresolved_value),
        resolved_value_(resolved_value) {}

  void Trace(Visitor* visitor) const final {
    CSSInterpolationType::ConversionChecker::Trace(visitor);
    visitor->Trace(unresolved_value_);
    visitor->Trace(resolved_value_);
  }

 private:
  bool IsValid(const InterpolationEnvironment& environment,
               const InterpolationValue&) const final {
    const auto& css_environment = To<CSSInterpolationEnvironment>(environment);
    const CSSValue* resolved_value =
        css_environment.Resolve(property_, unresolved_value_);
    return base::ValuesEquivalent(resolved_value_.Get(), resolved_value);
  }

  PropertyHandle property_;
  Member<const CSSValue> unresolved_value_;
  Member<const CSSValue> resolved_value_;
};

class ResolvedVariableChecker : public CSSInterpolationType::ConversionChecker {
 public:
  ResolvedVariableChecker(CSSPropertyID property,
                          const CSSValue* variable_reference,
                          const CSSValue* resolved_value)
      : property_(property),
        variable_reference_(variable_reference),
        resolved_value_(resolved_value) {}

  void Trace(Visitor* visitor) const final {
    CSSInterpolationType::ConversionChecker::Trace(visitor);
    visitor->Trace(variable_reference_);
    visitor->Trace(resolved_value_);
  }

 private:
  bool IsValid(const InterpolationEnvironment& environment,
               const InterpolationValue&) const final {
    const auto& css_environment = To<CSSInterpolationEnvironment>(environment);
    // TODO(alancutter): Just check the variables referenced instead of doing a
    // full CSSValue resolve.
    const CSSValue* resolved_value = css_environment.Resolve(
        PropertyHandle(CSSProperty::Get(property_)), variable_reference_);
    return base::ValuesEquivalent(resolved_value_.Get(), resolved_value);
  }

  CSSPropertyID property_;
  Member<const CSSValue> variable_reference_;
  Member<const CSSValue> resolved_value_;
};

class InheritedCustomPropertyChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  InheritedCustomPropertyChecker(const AtomicString& name,
                                 bool is_inherited_property,
                                 const CSSValue* inherited_value,
                                 const CSSValue* initial_value)
      : name_(name),
        is_inherited_property_(is_inherited_property),
        inherited_value_(inherited_value),
        initial_value_(initial_value) {}

  void Trace(Visitor* visitor) const final {
    CSSInterpolationType::ConversionChecker::Trace(visitor);
    visitor->Trace(inherited_value_);
    visitor->Trace(initial_value_);
  }

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue&) const final {
    const CSSValue* inherited_value =
        state.ParentStyle()->GetVariableValue(name_, is_inherited_property_);
    if (!inherited_value) {
      inherited_value = initial_value_.Get();
    }
    return base::ValuesEquivalent(inherited_value_.Get(), inherited_value);
  }

  AtomicString name_;
  const bool is_inherited_property_;
  Member<const CSSValue> inherited_value_;
  Member<const CSSValue> initial_value_;
};

class ResolvedRegisteredCustomPropertyChecker
    : public InterpolationType::ConversionChecker {
 public:
  ResolvedRegisteredCustomPropertyChecker(const PropertyHandle& property,
                                          const CSSValue& value,
                                          CSSVariableData* resolved_tokens)
      : property_(property), value_(value), resolved_tokens_(resolved_tokens) {}

  void Trace(Visitor* visitor) const final {
    CSSInterpolationType::ConversionChecker::Trace(visitor);
    visitor->Trace(value_);
    visitor->Trace(resolved_tokens_);
  }

 private:
  bool IsValid(const InterpolationEnvironment& environment,
               const InterpolationValue&) const final {
    const auto& css_environment = To<CSSInterpolationEnvironment>(environment);
    const CSSValue* resolved = css_environment.Resolve(property_, value_);
    CSSVariableData* resolved_tokens = nullptr;
    if (const auto* decl = DynamicTo<CSSUnparsedDeclarationValue>(resolved)) {
      resolved_tokens = decl->VariableDataValue();
    }

    return base::ValuesEquivalent(resolved_tokens, resolved_tokens_.Get());
  }

  PropertyHandle property_;
  Member<const CSSValue> value_;
  Member<CSSVariableData> resolved_tokens_;
};

template <typename RevertValueType>
class RevertChecker : public CSSInterpolationType::ConversionChecker {
 public:
  static_assert(
      std::is_same<RevertValueType, cssvalue::CSSRevertValue>::value ||
          std::is_same<RevertValueType, cssvalue::CSSRevertLayerValue>::value,
      "RevertCheck only accepts CSSRevertValue and CSSRevertLayerValue");

  RevertChecker(const PropertyHandle& property_handle,
                const CSSValue* resolved_value)
      : property_handle_(property_handle), resolved_value_(resolved_value) {}

  void Trace(Visitor* visitor) const final {
    CSSInterpolationType::ConversionChecker::Trace(visitor);
    visitor->Trace(resolved_value_);
  }

 private:
  bool IsValid(const InterpolationEnvironment& environment,
               const InterpolationValue&) const final {
    const auto& css_environment = To<CSSInterpolationEnvironment>(environment);
    const CSSValue* current_resolved_value =
        css_environment.Resolve(property_handle_, RevertValueType::Create());
    return base::ValuesEquivalent(resolved_value_.Get(),
                                  current_resolved_value);
  }

  PropertyHandle property_handle_;
  Member<const CSSValue> resolved_value_;
};

CSSInterpolationType::CSSInterpolationType(
    PropertyHandle property,
    const PropertyRegistration* registration)
    : InterpolationType(property), registration_(registration) {
  DCHECK(!GetProperty().IsCSSCustomProperty() || registration);
  DCHECK(!CssProperty().IsShorthand());
}

InterpolationValue CSSInterpolationType::MaybeConvertSingle(
    const PropertySpecificKeyframe& keyframe,
    const InterpolationEnvironment& environment,
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  InterpolationValue result = MaybeConvertSingleInternal(
      keyframe, environment, underlying, conversion_checkers);
  if (result && keyframe.Composite() !=
                    EffectModel::CompositeOperation::kCompositeReplace) {
    return PreInterpolationCompositeIfNeeded(std::move(result), underlying,
                                             keyframe.Composite(),
                                             conversion_checkers);
  }
  return result;
}

InterpolationValue CSSInterpolationType::MaybeConvertSingleInternal(
    const PropertySpecificKeyframe& keyframe,
    const InterpolationEnvironment& environment,
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  const CSSValue* value = To<CSSPropertySpecificKeyframe>(keyframe).Value();
  const auto& css_environment = To<CSSInterpolationEnvironment>(environment);
  const StyleResolverState& state = css_environment.GetState();

  if (!value)
    return MaybeConvertNeutral(underlying, conversion_checkers);

  if (GetProperty().IsCSSCustomProperty()) {
    return MaybeConvertCustomPropertyDeclaration(*value, environment,
                                                 conversion_checkers);
  }

  if (value->IsUnparsedDeclaration() || value->IsPendingSubstitutionValue()) {
    const CSSValue* resolved_value =
        css_environment.Resolve(GetProperty(), value);

    DCHECK(resolved_value);
    conversion_checkers.push_back(MakeGarbageCollected<ResolvedVariableChecker>(
        CssProperty().PropertyID(), value, resolved_value));
    value = resolved_value;
  }
  if (value->IsMathFunctionValue()) {
    // Math functions can contain anchor() and anchor-size() functions,
    // and those functions can make the value invalid at computed-value time
    // if they reference an invalid anchor and also don't have a fallback.
    const CSSValue* resolved_value =
        css_environment.Resolve(GetProperty(), value);
    DCHECK(resolved_value);
    conversion_checkers.push_back(MakeGarbageCollected<ResolvedValueChecker>(
        GetProperty(), /* unresolved_value */ value, resolved_value));
    value = resolved_value;
  }

  if (value->IsRevertValue()) {
    value = css_environment.Resolve(GetProperty(), value);
    DCHECK(value);
    conversion_checkers.push_back(
        MakeGarbageCollected<RevertChecker<cssvalue::CSSRevertValue>>(
            GetProperty(), value));
  }

  if (value->IsRevertLayerValue()) {
    value = css_environment.Resolve(GetProperty(), value);
    DCHECK(value);
    conversion_checkers.push_back(
        MakeGarbageCollected<RevertChecker<cssvalue::CSSRevertLayerValue>>(
            GetProperty(), value));
  }

  bool is_inherited = CssProperty().IsInherited();
  if (value->IsInitialValue() || (value->IsUnsetValue() && !is_inherited)) {
    return MaybeConvertInitial(state, conversion_checkers);
  }

  if (value->IsInheritedValue() || (value->IsUnsetValue() && is_inherited)) {
    return MaybeConvertInherit(state, conversion_checkers);
  }

  return MaybeConvertValue(*value, &state, conversion_checkers);
}

InterpolationValue CSSInterpolationType::MaybeConvertCustomPropertyDeclaration(
    const CSSValue& declaration,
    const InterpolationEnvironment& environment,
    ConversionCheckers& conversion_checkers) const {
  const auto& css_environment = To<CSSInterpolationEnvironment>(environment);
  const StyleResolverState& state = css_environment.GetState();

  AtomicString name = GetProperty().CustomPropertyName();

  const CSSValue* value = &declaration;
  value = css_environment.Resolve(GetProperty(), value);
  DCHECK(value) << "CSSVarCycleInterpolationType should have handled nullptr";

  if (declaration.IsRevertValue()) {
    conversion_checkers.push_back(
        MakeGarbageCollected<RevertChecker<cssvalue::CSSRevertValue>>(
            GetProperty(), value));
  }
  if (declaration.IsRevertLayerValue()) {
    conversion_checkers.push_back(
        MakeGarbageCollected<RevertChecker<cssvalue::CSSRevertLayerValue>>(
            GetProperty(), value));
  }
  if (const auto* resolved_declaration =
          DynamicTo<CSSUnparsedDeclarationValue>(value)) {
    // If Resolve returned a different CSSUnparsedDeclarationValue, var()
    // references were substituted.
    if (resolved_declaration != &declaration) {
      conversion_checkers.push_back(
          MakeGarbageCollected<ResolvedRegisteredCustomPropertyChecker>(
              GetProperty(), declaration,
              resolved_declaration->VariableDataValue()));
    }
  }

  // Unfortunately we transport CSS-wide keywords inside the
  // CSSUnparsedDeclarationValue. Expand those keywords into real CSSValues
  // if present.
  bool is_inherited = Registration().Inherits();
  const StyleInitialData* initial_data = state.StyleBuilder().InitialData();
  DCHECK(initial_data);
  const CSSValue* initial_value = initial_data->GetVariableValue(name);

  // Handle CSS-wide keywords (except 'revert', which should have been
  // handled already).
  DCHECK(!value->IsRevertValue());
  if (value->IsInitialValue() || (value->IsUnsetValue() && !is_inherited)) {
    value = initial_value;
  } else if (value->IsInheritedValue() ||
             (value->IsUnsetValue() && is_inherited)) {
    value = state.ParentStyle()->GetVariableValue(name, is_inherited);
    if (!value) {
      value = initial_value;
    }
    conversion_checkers.push_back(
        MakeGarbageCollected<InheritedCustomPropertyChecker>(
            name, is_inherited, value, initial_value));
  }

  if (const auto* resolved_declaration =
          DynamicTo<CSSUnparsedDeclarationValue>(value)) {
    DCHECK(
        !resolved_declaration->VariableDataValue()->NeedsVariableResolution());
    value = resolved_declaration->VariableDataValue()->ParseForSyntax(
        registration_->Syntax(),
        state.GetDocument().GetExecutionContext()->GetSecureContextMode());
    if (!value)
      return nullptr;
  }

  DCHECK(value);
  return MaybeConvertValue(*value, &state, conversion_checkers);
}

InterpolationValue CSSInterpolationType::MaybeConvertUnderlyingValue(
    const InterpolationEnvironment& environment) const {
  const auto& css_environment = To<CSSInterpolationEnvironment>(environment);
  const ComputedStyle& style = css_environment.BaseStyle();
  if (!GetProperty().IsCSSCustomProperty()) {
    return MaybeConvertStandardPropertyUnderlyingValue(style);
  }

  const PropertyHandle property = GetProperty();
  const AtomicString& name = property.CustomPropertyName();
  const CSSValue* underlying_value =
      style.GetVariableValue(name, Registration().Inherits());
  if (!underlying_value)
    return nullptr;
  // TODO(alancutter): Remove the need for passing in conversion checkers.
  ConversionCheckers dummy_conversion_checkers;
  return MaybeConvertValue(*underlying_value,
                           css_environment.GetOptionalState(),
                           dummy_conversion_checkers);
}

void CSSInterpolationType::Apply(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    InterpolationEnvironment& environment) const {
  StyleResolverState& state =
      To<CSSInterpolationEnvironment>(environment).GetState();

  if (GetProperty().IsCSSCustomProperty()) {
    ApplyCustomPropertyValue(interpolable_value, non_interpolable_value, state);
    return;
  }

  // The anchor() and anchor-size() functions evaluate differently depending
  // on which property they are used in. The regular CSSProperty::ApplyValue
  // code paths take care of this, but we are bypassing those code paths,
  // so we have to do it ourselves.
  AnchorScope anchor_scope(
      CssProperty().PropertyID(),
      state.CssToLengthConversionData().GetAnchorEvaluator());
  ApplyStandardPropertyValue(interpolable_value, non_interpolable_value, state);
}

void CSSInterpolationType::ApplyCustomPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  DCHECK(GetProperty().IsCSSCustomProperty());

  const CSSValue* css_value =
      CreateCSSValue(interpolable_value, non_interpolable_value, state);
  DCHECK(!css_value->IsUnparsedDeclaration());
  StyleBuilder::ApplyProperty(GetProperty().GetCSSPropertyName(), state,
                              *css_value, StyleBuilder::ValueMode::kAnimated);
}

}  // namespace blink
