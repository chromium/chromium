// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/custom_property.h"

#include "third_party/blink/renderer/core/css/css_unparsed_declaration_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_local_context.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css/property_registration.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

CSSProperty::Flags InheritedFlag(const PropertyRegistration* registration) {
  if (!registration || registration->Inherits()) {
    return CSSProperty::kInherited;
  }
  return 0;
}

}  // namespace

CustomProperty::CustomProperty(AtomicString name, const Document& document)
    : CustomProperty(
          PropertyRegistration::From(document.GetExecutionContext(), name)) {
  // Initializing `name_` on the body prevents `name` to be used after the
  // std::move call.
  name_ = std::move(name);
  DCHECK_EQ(IsShorthand(), CSSProperty::IsShorthand(GetCSSPropertyName()));
  DCHECK_EQ(IsRepeated(), CSSProperty::IsRepeated(GetCSSPropertyName()));
}

CustomProperty::CustomProperty(const AtomicString& name,
                               const PropertyRegistry* registry)
    : CustomProperty(name, registry ? registry->Registration(name) : nullptr) {}

CustomProperty::CustomProperty(const AtomicString& name,
                               const PropertyRegistration* registration)
    : Variable(InheritedFlag(registration)),
      name_(name),
      registration_(registration) {
  DCHECK_EQ(IsShorthand(), CSSProperty::IsShorthand(GetCSSPropertyName()));
  DCHECK_EQ(IsRepeated(), CSSProperty::IsRepeated(GetCSSPropertyName()));
}

CustomProperty::CustomProperty(const PropertyRegistration* registration)
    : Variable(InheritedFlag(registration)), registration_(registration) {}

const AtomicString& CustomProperty::GetPropertyNameAtomicString() const {
  return name_;
}

CSSPropertyName CustomProperty::GetCSSPropertyName() const {
  return CSSPropertyName(name_);
}

bool CustomProperty::HasEqualCSSPropertyName(const CSSProperty& other) const {
  if (PropertyID() != other.PropertyID()) {
    return false;
  }
  return name_ == other.GetPropertyNameAtomicString();
}

void CustomProperty::ApplyInitial(StyleResolverState& state) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  bool is_inherited_property = IsInherited();

  builder.SetHasVariableDeclaration();

  if (!registration_) {
    builder.SetVariableData(name_, nullptr, is_inherited_property);
    return;
  }

  // TODO(crbug.com/831568): The ComputedStyle of elements outside the flat
  // tree is not guaranteed to be up-to-date. This means that the
  // StyleInitialData may also be missing. We just disable initial values in
  // this case, since we shouldn't really be returning a style for those
  // elements anyway.
  if (state.StyleBuilder().IsEnsuredOutsideFlatTree()) {
    return;
  }

  const StyleInitialData* initial_data = state.StyleBuilder().InitialData();
  DCHECK(initial_data);
  CSSVariableData* initial_variable_data = initial_data->GetVariableData(name_);
  const CSSValue* initial_value = initial_data->GetVariableValue(name_);

  builder.SetVariableData(name_, initial_variable_data, is_inherited_property);
  builder.SetVariableValue(name_, initial_value, is_inherited_property);
}

void CustomProperty::ApplyInherit(StyleResolverState& state) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  bool is_inherited_property = IsInherited();

  CSSVariableData* parent_data =
      state.ParentStyle()->GetVariableData(name_, is_inherited_property);

  builder.SetVariableData(name_, parent_data, is_inherited_property);

  if (registration_) {
    const CSSValue* parent_value = state.ParentStyle()->GetVariableValue(name_);
    builder.SetVariableValue(name_, parent_value, is_inherited_property);
  }
}

void CustomProperty::ApplyValue(StyleResolverState& state,
                                const CSSValue& value,
                                ValueMode value_mode) const {
  // Highlight Pseudos do not allow custom property definitions.
  // Properties are copied from the originating element when the
  // style is created.
  if (state.UsesHighlightPseudoInheritance()) {
    return;
  }

  ComputedStyleBuilder& builder = state.StyleBuilder();
  DCHECK(!value.IsCSSWideKeyword());

  builder.SetHasVariableDeclaration();

  if (value.IsInvalidVariableValue()) {
    if (!SupportsGuaranteedInvalid()) {
      ApplyUnset(state);
      return;
    }
    builder.SetVariableData(name_, nullptr, IsInherited());
    if (registration_) {
      builder.SetVariableValue(name_, nullptr, IsInherited());
    }
    return;
  }

  bool is_inherited_property = IsInherited();

  const auto* declaration = DynamicTo<CSSUnparsedDeclarationValue>(value);

  // Unregistered custom properties can only accept
  // CSSUnparsedDeclarationValue objects.
  if (!registration_) {
    // We can reach here without a CSSUnparsedDeclarationValue
    // if we're removing a property registration while animating.
    // TODO(andruud): Cancel animations if the registration changed.
    if (declaration) {
      CSSVariableData& data = *declaration->VariableDataValue();
      DCHECK(!data.NeedsVariableResolution());
      builder.SetVariableData(name_, &data, is_inherited_property);
    }
    return;
  }

  // Registered custom properties can accept either
  // - A CSSUnparsedDeclarationValue, in which case we produce the
  //   `registered_value` value from that, or:
  // - Some other value (typically an interpolated value), which we'll use
  //   as the `registered_value` directly.

  const CSSParserContext* context =
      declaration ? declaration->ParserContext() : nullptr;

  if (!context) {
    // There is no "originating" CSSParserContext associated with the
    // declaration if it represents a "synthetic" token sequence such as those
    // constructed to represent interpolated (registered) custom properties. [1]
    //
    // However, such values should also not contain any relative url()
    // functions, so we don't need any particular parser context in that case.
    //
    // [1]
    // https://drafts.css-houdini.org/css-properties-values-api-1/#equivalent-token-sequence
    context = StrictCSSParserContext(
        state.GetDocument().GetExecutionContext()->GetSecureContextMode());
  }

  const CSSValue* registered_value = declaration ? nullptr : &value;

  if (!registered_value) {
    DCHECK(declaration);
    CSSVariableData& data = *declaration->VariableDataValue();
    registered_value =
        Parse(data.OriginalText(), *context, CSSParserLocalContext());
  }

  if (!registered_value) {
    if (is_inherited_property) {
      ApplyInherit(state);
    } else {
      ApplyInitial(state);
    }
    return;
  }

  bool is_animation_tainted = value_mode == ValueMode::kAnimated;

  // Note that the computed value ("SetVariableValue") is stored separately
  // from the substitution value ("SetVariableData") on ComputedStyle.
  // The substitution value is used for substituting var() references to
  // the custom property, and the computed value is generally used in other
  // cases (e.g. serialization).
  //
  // Note also that `registered_value` may be attr-tainted at this point.
  // This is what we want when producing the substitution value,
  // since any tainting must survive the substitution. However, the computed
  // value should serialize without taint-tokens, hence we store an
  // UntaintedCopy of `registered_value`.
  //
  // See also css_attr_tainting.h.
  registered_value = &StyleBuilderConverter::ConvertRegisteredPropertyValue(
      state, *registered_value, context);
  CSSVariableData* data =
      StyleBuilderConverter::ConvertRegisteredPropertyVariableData(
          *registered_value, is_animation_tainted);
  builder.SetVariableData(name_, data, is_inherited_property);
  builder.SetVariableValue(name_, registered_value->UntaintedCopy(),
                           is_inherited_property);
}

const CSSValue* CustomProperty::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

const CSSValue* CustomProperty::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (registration_) {
    const CSSValue* value = style.GetVariableValue(name_, IsInherited());
    if (value) {
      return value;
    }
    // If we don't have CSSValue for this registered property, it means that
    // that the property was not registered at the time |style| was calculated,
    // hence we proceed with unregistered behavior.
  }

  CSSVariableData* data = style.GetVariableData(name_, IsInherited());

  if (!data) {
    return nullptr;
  }

  return MakeGarbageCollected<CSSUnparsedDeclarationValue>(
      data, /* parser_context */ nullptr);
}

const CSSValue* CustomProperty::ParseUntyped(
    StringView text,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return CSSVariableParser::ParseDeclarationValue(
      text, local_context.IsAnimationTainted(), context);
}

const CSSValue* CustomProperty::Parse(
    StringView text,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  if (!registration_) {
    return ParseUntyped(text, context, local_context);
  }
  return registration_->Syntax().Parse(text, context,
                                       local_context.IsAnimationTainted());
}

bool CustomProperty::HasInitialValue() const {
  if (!registration_) {
    return false;
  }
  return registration_->Initial();
}

bool CustomProperty::SupportsGuaranteedInvalid() const {
  return !registration_ || registration_->Syntax().IsUniversal();
}

bool CustomProperty::HasUniversalSyntax() const {
  return registration_ && registration_->Syntax().IsUniversal();
}

}  // namespace blink
