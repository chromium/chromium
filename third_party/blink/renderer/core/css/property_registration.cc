// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/property_registration.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_property_definition.h"
#include "third_party/blink/renderer/core/animation/css_interpolation_types_map.h"
#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_string_value.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/css_syntax_definition.h"
#include "third_party/blink/renderer/core/css/css_syntax_string_parser.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/core/css/css_variable_reference_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"

namespace blink {

const PropertyRegistration* PropertyRegistration::From(
    const ExecutionContext* execution_context,
    const AtomicString& property_name) {
  const auto* window = DynamicTo<LocalDOMWindow>(execution_context);
  if (!window) {
    return nullptr;
  }
  const PropertyRegistry* registry = window->document()->GetPropertyRegistry();
  return registry ? registry->Registration(property_name) : nullptr;
}

PropertyRegistration::PropertyRegistration(const AtomicString& name,
                                           const CSSSyntaxDefinition& syntax,
                                           bool inherits,
                                           const CSSValue* initial)
    : syntax_(syntax),
      inherits_(inherits),
      initial_(initial),
      interpolation_types_(
          CSSInterpolationTypesMap::CreateInterpolationTypesForCSSSyntax(
              name,
              syntax,
              *this)),
      referenced_(false) {}

PropertyRegistration::~PropertyRegistration() = default;

unsigned PropertyRegistration::GetViewportUnitFlags() const {
  unsigned flags = 0;
  if (const auto* primitive_value =
          DynamicTo<CSSPrimitiveValue>(initial_.Get())) {
    CSSPrimitiveValue::LengthTypeFlags length_type_flags;
    primitive_value->AccumulateLengthUnitTypes(length_type_flags);
    if (CSSPrimitiveValue::HasStaticViewportUnits(length_type_flags)) {
      flags |= static_cast<unsigned>(ViewportUnitFlag::kStatic);
    }
    if (CSSPrimitiveValue::HasDynamicViewportUnits(length_type_flags)) {
      flags |= static_cast<unsigned>(ViewportUnitFlag::kDynamic);
    }
  }
  return flags;
}

static bool ComputationallyIndependent(const CSSValue& value) {
  DCHECK(!value.IsCSSWideKeyword());

  if (auto* variable_reference_value =
          DynamicTo<CSSVariableReferenceValue>(value)) {
    return !variable_reference_value->VariableDataValue()
                ->NeedsVariableResolution();
  }

  if (auto* value_list = DynamicTo<CSSValueList>(value)) {
    for (const CSSValue* inner_value : *value_list) {
      if (!ComputationallyIndependent(*inner_value)) {
        return false;
      }
    }
    return true;
  }

  if (const auto* primitive_value = DynamicTo<CSSPrimitiveValue>(value)) {
    return primitive_value->IsComputationallyIndependent();
  }

  // TODO(timloh): Images values can also contain lengths.

  return true;
}

static absl::optional<CSSSyntaxDefinition> ConvertSyntax(
    const CSSValue& value) {
  return CSSSyntaxStringParser(To<CSSStringValue>(value).Value()).Parse();
}

static bool ConvertInherts(const CSSValue& value) {
  CSSValueID inherits_id = To<CSSIdentifierValue>(value).GetValueID();
  DCHECK(inherits_id == CSSValueID::kTrue || inherits_id == CSSValueID::kFalse);
  return inherits_id == CSSValueID::kTrue;
}

static scoped_refptr<CSSVariableData> ConvertInitialVariableData(
    const CSSValue* value) {
  if (!value) {
    return nullptr;
  }
  return &To<CSSCustomPropertyDeclaration>(*value).Value();
}

PropertyRegistration* PropertyRegistration::MaybeCreateForDeclaredProperty(
    Document& document,
    const AtomicString& name,
    StyleRuleProperty& rule) {
  // https://drafts.css-houdini.org/css-properties-values-api-1/#the-syntax-descriptor
  const CSSValue* syntax_value = rule.GetSyntax();
  if (!syntax_value) {
    return nullptr;
  }
  absl::optional<CSSSyntaxDefinition> syntax = ConvertSyntax(*syntax_value);
  if (!syntax) {
    return nullptr;
  }

  // https://drafts.css-houdini.org/css-properties-values-api-1/#inherits-descriptor
  const CSSValue* inherits_value = rule.Inherits();
  if (!inherits_value) {
    return nullptr;
  }
  bool inherits = ConvertInherts(*inherits_value);

  // https://drafts.css-houdini.org/css-properties-values-api-1/#initial-value-descriptor
  const CSSValue* initial_value = rule.GetInitialValue();
  scoped_refptr<CSSVariableData> initial_variable_data =
      ConvertInitialVariableData(initial_value);

  // Parse initial value, if we have it.
  const CSSValue* initial = nullptr;
  if (initial_variable_data) {
    const CSSParserContext* parser_context =
        document.ElementSheet().Contents()->ParserContext();
    const bool is_animation_tainted = false;
    initial = syntax->Parse(initial_variable_data->TokenRange(),
                            *parser_context, is_animation_tainted);
    if (!initial) {
      return nullptr;
    }
    if (!ComputationallyIndependent(*initial)) {
      return nullptr;
    }
  }

  // For non-universal @property rules, the initial value is required for the
  // the rule to be valid.
  if (!initial && !syntax->IsUniversal()) {
    return nullptr;
  }

  return MakeGarbageCollected<PropertyRegistration>(name, *syntax, inherits,
                                                    initial);
}

void PropertyRegistration::registerProperty(
    ExecutionContext* execution_context,
    const PropertyDefinition* property_definition,
    ExceptionState& exception_state) {
  // Bindings code ensures these are set.
  DCHECK(property_definition->hasName());
  DCHECK(property_definition->hasInherits());
  DCHECK(property_definition->hasSyntax());

  String name = property_definition->name();
  if (!CSSVariableParser::IsValidVariableName(name)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "Custom property names must start with '--'.");
    return;
  }
  AtomicString atomic_name(name);
  Document* document = To<LocalDOMWindow>(execution_context)->document();
  PropertyRegistry& registry = document->EnsurePropertyRegistry();
  if (registry.IsInRegisteredPropertySet(atomic_name)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidModificationError,
        "The name provided has already been registered.");
    return;
  }

  absl::optional<CSSSyntaxDefinition> syntax_definition =
      CSSSyntaxStringParser(property_definition->syntax()).Parse();
  if (!syntax_definition) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The syntax provided is not a valid custom property syntax.");
    return;
  }

  const CSSParserContext* parser_context =
      document->ElementSheet().Contents()->ParserContext();

  const CSSValue* initial = nullptr;
  if (property_definition->hasInitialValue()) {
    CSSTokenizer tokenizer(property_definition->initialValue());
    const auto tokens = tokenizer.TokenizeToEOF();
    bool is_animation_tainted = false;
    initial = syntax_definition->Parse(CSSParserTokenRange(tokens),
                                       *parser_context, is_animation_tainted);
    if (!initial) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kSyntaxError,
          "The initial value provided does not parse for the given syntax.");
      return;
    }
    if (!ComputationallyIndependent(*initial)) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kSyntaxError,
          "The initial value provided is not computationally independent.");
      return;
    }
  } else {
    if (!syntax_definition->IsUniversal()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kSyntaxError,
          "An initial value must be provided if the syntax is not '*'");
      return;
    }
  }
  registry.RegisterProperty(atomic_name,
                            *MakeGarbageCollected<PropertyRegistration>(
                                atomic_name, *syntax_definition,
                                property_definition->inherits(), initial));

  document->GetStyleEngine().PropertyRegistryChanged();
}

void PropertyRegistration::RemoveDeclaredProperties(Document& document) {
  if (!document.GetPropertyRegistry()) {
    return;
  }

  PropertyRegistry& registry = document.EnsurePropertyRegistry();

  size_t version_before = registry.Version();
  registry.RemoveDeclaredProperties();
  size_t version_after = registry.Version();

  if (version_before != version_after) {
    document.GetStyleEngine().PropertyRegistryChanged();
  }
}

}  // namespace blink
