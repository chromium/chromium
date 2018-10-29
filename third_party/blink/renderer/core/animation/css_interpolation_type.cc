// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_interpolation_type.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/animation/css_interpolation_environment.h"
#include "third_party/blink/renderer/core/animation/string_keyframe.h"
#include "third_party/blink/renderer/core/css/computed_style_css_value_mapping.h"
#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/css_variable_reference_value.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/property_registration.h"
#include "third_party/blink/renderer/core/css/resolver/css_variable_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/data_equivalency.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"

namespace blink {

class ResolvedVariableChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  static std::unique_ptr<ResolvedVariableChecker> Create(
      CSSPropertyID property,
      const CSSValue* variable_reference,
      const CSSValue* resolved_value) {
    return base::WrapUnique(new ResolvedVariableChecker(
        property, variable_reference, resolved_value));
  }

 private:
  ResolvedVariableChecker(CSSPropertyID property,
                          const CSSValue* variable_reference,
                          const CSSValue* resolved_value)
      : property_(property),
        variable_reference_(variable_reference),
        resolved_value_(resolved_value) {}

  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    // TODO(alancutter): Just check the variables referenced instead of doing a
    // full CSSValue resolve.
    bool omit_animation_tainted = false;
    const CSSValue* resolved_value =
        CSSVariableResolver(state).ResolveVariableReferences(
            property_, *variable_reference_, omit_animation_tainted);
    return DataEquivalent(resolved_value_.Get(), resolved_value);
  }

  CSSPropertyID property_;
  Persistent<const CSSValue> variable_reference_;
  Persistent<const CSSValue> resolved_value_;
};

class InheritedCustomPropertyChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  static std::unique_ptr<InheritedCustomPropertyChecker> Create(
      const AtomicString& property,
      bool is_inherited_property,
      const CSSValue* inherited_value,
      const CSSValue* initial_value) {
    return base::WrapUnique(new InheritedCustomPropertyChecker(
        property, is_inherited_property, inherited_value, initial_value));
  }

 private:
  InheritedCustomPropertyChecker(const AtomicString& name,
                                 bool is_inherited_property,
                                 const CSSValue* inherited_value,
                                 const CSSValue* initial_value)
      : name_(name),
        is_inherited_property_(is_inherited_property),
        inherited_value_(inherited_value),
        initial_value_(initial_value) {}

  bool IsValid(const StyleResolverState& state,
               const InterpolationValue&) const final {
    const CSSValue* inherited_value =
        state.ParentStyle()->GetRegisteredVariable(name_,
                                                   is_inherited_property_);
    if (!inherited_value) {
      inherited_value = initial_value_.Get();
    }
    return DataEquivalent(inherited_value_.Get(), inherited_value);
  }

  const AtomicString& name_;
  const bool is_inherited_property_;
  Persistent<const CSSValue> inherited_value_;
  Persistent<const CSSValue> initial_value_;
};

class ResolvedRegisteredCustomPropertyChecker
    : public InterpolationType::ConversionChecker {
 public:
  static std::unique_ptr<ResolvedRegisteredCustomPropertyChecker> Create(
      const CSSCustomPropertyDeclaration& declaration,
      scoped_refptr<CSSVariableData> resolved_tokens) {
    return base::WrapUnique(new ResolvedRegisteredCustomPropertyChecker(
        declaration, std::move(resolved_tokens)));
  }

 private:
  ResolvedRegisteredCustomPropertyChecker(
      const CSSCustomPropertyDeclaration& declaration,
      scoped_refptr<CSSVariableData> resolved_tokens)
      : declaration_(declaration),
        resolved_tokens_(std::move(resolved_tokens)) {}

  bool IsValid(const InterpolationEnvironment& environment,
               const InterpolationValue&) const final {
    DCHECK(ToCSSInterpolationEnvironment(environment).HasVariableResolver());
    bool cycle_detected;
    scoped_refptr<CSSVariableData> resolved_tokens =
        ToCSSInterpolationEnvironment(environment)
            .VariableResolver()
            .ResolveCustomPropertyAnimationKeyframe(*declaration_,
                                                    cycle_detected);
    DCHECK(!cycle_detected);
    return DataEquivalent(resolved_tokens, resolved_tokens_);
  }

  Persistent<const CSSCustomPropertyDeclaration> declaration_;
  scoped_refptr<CSSVariableData> resolved_tokens_;
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
    AdditiveKeyframeHook(result);
  }
  return result;
}

InterpolationValue CSSInterpolationType::MaybeConvertSingleInternal(
    const PropertySpecificKeyframe& keyframe,
    const InterpolationEnvironment& environment,
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  const CSSValue* value = ToCSSPropertySpecificKeyframe(keyframe).Value();
  const CSSInterpolationEnvironment& css_environment =
      ToCSSInterpolationEnvironment(environment);
  const StyleResolverState& state = css_environment.GetState();

  if (!value)
    return MaybeConvertNeutral(underlying, conversion_checkers);

  if (GetProperty().IsCSSCustomProperty()) {
    DCHECK(css_environment.HasVariableResolver());
    return MaybeConvertCustomPropertyDeclaration(
        ToCSSCustomPropertyDeclaration(*value), state,
        css_environment.VariableResolver(), conversion_checkers);
  }

  if (value->IsVariableReferenceValue() ||
      value->IsPendingSubstitutionValue()) {
    bool omit_animation_tainted = false;
    const CSSValue* resolved_value =
        CSSVariableResolver(state).ResolveVariableReferences(
            CssProperty().PropertyID(), *value, omit_animation_tainted);
    conversion_checkers.push_back(ResolvedVariableChecker::Create(
        CssProperty().PropertyID(), value, resolved_value));
    value = resolved_value;
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
    const CSSCustomPropertyDeclaration& declaration,
    const StyleResolverState& state,
    CSSVariableResolver& variable_resolver,
    ConversionCheckers& conversion_checkers) const {
  const AtomicString& name = declaration.GetName();
  DCHECK_EQ(GetProperty().CustomPropertyName(), name);

  if (!declaration.Value()) {
    bool is_inherited_property = Registration().Inherits();
    DCHECK(declaration.IsInitial(is_inherited_property) ||
           declaration.IsInherit(is_inherited_property));

    const CSSValue* value = nullptr;
    if (declaration.IsInitial(is_inherited_property)) {
      value = Registration().Initial();
    } else {
      value = state.ParentStyle()->GetRegisteredVariable(name,
                                                         is_inherited_property);
      if (!value) {
        value = Registration().Initial();
      }
      conversion_checkers.push_back(InheritedCustomPropertyChecker::Create(
          name, is_inherited_property, value, Registration().Initial()));
    }
    if (!value) {
      return nullptr;
    }

    return MaybeConvertValue(*value, &state, conversion_checkers);
  }

  scoped_refptr<CSSVariableData> resolved_tokens;
  if (declaration.Value()->NeedsVariableResolution()) {
    bool cycle_detected;
    resolved_tokens = variable_resolver.ResolveCustomPropertyAnimationKeyframe(
        declaration, cycle_detected);
    DCHECK(!cycle_detected);
    conversion_checkers.push_back(
        ResolvedRegisteredCustomPropertyChecker::Create(declaration,
                                                        resolved_tokens));
  } else {
    resolved_tokens = declaration.Value();
  }
  const CSSValue* resolved_value =
      resolved_tokens ? resolved_tokens->ParseForSyntax(
                            registration_->Syntax(),
                            state.GetDocument().GetSecureContextMode())
                      : nullptr;
  if (!resolved_value) {
    return nullptr;
  }
  return MaybeConvertValue(*resolved_value, &state, conversion_checkers);
}

InterpolationValue CSSInterpolationType::MaybeConvertUnderlyingValue(
    const InterpolationEnvironment& environment) const {
  const ComputedStyle& style =
      ToCSSInterpolationEnvironment(environment).Style();
  if (!GetProperty().IsCSSCustomProperty()) {
    return MaybeConvertStandardPropertyUnderlyingValue(style);
  }

  const PropertyHandle property = GetProperty();
  const AtomicString& name = property.CustomPropertyName();
  const CSSValue* underlying_value =
      style.GetRegisteredVariable(name, Registration().Inherits());
  if (!underlying_value) {
    underlying_value = Registration().Initial();
  }
  if (!underlying_value) {
    return nullptr;
  }
  // TODO(alancutter): Remove the need for passing in conversion checkers.
  ConversionCheckers dummy_conversion_checkers;
  return MaybeConvertValue(*underlying_value, nullptr,
                           dummy_conversion_checkers);
}

void CSSInterpolationType::Apply(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    InterpolationEnvironment& environment) const {
  StyleResolverState& state =
      ToCSSInterpolationEnvironment(environment).GetState();

  if (GetProperty().IsCSSCustomProperty()) {
    ApplyCustomPropertyValue(interpolable_value, non_interpolable_value, state);
    return;
  }
  ApplyStandardPropertyValue(interpolable_value, non_interpolable_value, state);
}

void CSSInterpolationType::ApplyCustomPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  DCHECK(GetProperty().IsCSSCustomProperty());

  const CSSValue* css_value =
      CreateCSSValue(interpolable_value, non_interpolable_value, state);
  DCHECK(!css_value->IsCustomPropertyDeclaration());

  // TODO(alancutter): Defer tokenization of the CSSValue until it is needed.
  String string_value = css_value->CssText();
  CSSTokenizer tokenizer(string_value);
  const auto tokens = tokenizer.TokenizeToEOF();
  bool is_animation_tainted = true;
  bool needs_variable_resolution = false;
  scoped_refptr<CSSVariableData> variable_data = CSSVariableData::Create(
      CSSParserTokenRange(tokens), is_animation_tainted,
      needs_variable_resolution, KURL(), WTF::TextEncoding());
  ComputedStyle& style = *state.Style();
  const PropertyHandle property = GetProperty();
  const AtomicString& property_name = property.CustomPropertyName();
  bool inherits = Registration().Inherits();
  style.SetVariable(property_name, std::move(variable_data), inherits);
  style.SetRegisteredVariable(property_name, css_value, inherits);
}

}  // namespace blink
