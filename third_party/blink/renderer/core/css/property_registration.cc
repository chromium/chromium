// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/property_registration.h"

#include "third_party/blink/renderer/core/animation/css_interpolation_types_map.h"
#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_string_value.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/css_syntax_definition.h"
#include "third_party/blink/renderer/core/css/css_syntax_string_parser.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/css_variable_reference_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css/property_definition.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"

namespace blink {

const PropertyRegistration* PropertyRegistration::From(
    const ExecutionContext* execution_context,
    const AtomicString& property_name) {
  const auto* document = DynamicTo<Document>(execution_context);
  if (!document)
    return nullptr;
  const PropertyRegistry* registry = document->GetPropertyRegistry();
  return registry ? registry->Registration(property_name) : nullptr;
}

PropertyRegistration::PropertyRegistration(
    const AtomicString& name,
    const CSSSyntaxDefinition& syntax,
    bool inherits,
    const CSSValue* initial,
    scoped_refptr<CSSVariableData> initial_variable_data)
    : syntax_(syntax),
      inherits_(inherits),
      initial_(initial),
      initial_variable_data_(std::move(initial_variable_data)),
      interpolation_types_(
          CSSInterpolationTypesMap::CreateInterpolationTypesForCSSSyntax(
              name,
              syntax,
              *this)),
      referenced_(false) {
  DCHECK(RuntimeEnabledFeatures::CSSVariables2Enabled());
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
      if (!ComputationallyIndependent(*inner_value))
        return false;
    }
    return true;
  }

  if (const auto* primitive_value = DynamicTo<CSSPrimitiveValue>(value))
    return primitive_value->IsComputationallyIndependent();

  // TODO(timloh): Images values can also contain lengths.

  return true;
}

static base::Optional<CSSSyntaxDefinition> ConvertSyntax(
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
  if (!value)
    return nullptr;
  return To<CSSCustomPropertyDeclaration>(*value).Value();
}

PropertyRegistration* PropertyRegistration::MaybeCreate(
    Document& document,
    const AtomicString& name,
    StyleRuleProperty& rule) {
  const auto& properties = rule.Properties();

  // syntax
  const CSSValue* syntax_value =
      properties.GetPropertyCSSValue(CSSPropertyID::kSyntax);
  if (!syntax_value)
    return nullptr;
  base::Optional<CSSSyntaxDefinition> syntax = ConvertSyntax(*syntax_value);
  if (!syntax)
    return nullptr;

  // inherits
  const CSSValue* inherits_value =
      properties.GetPropertyCSSValue(CSSPropertyID::kInherits);
  if (!inherits_value)
    return nullptr;
  bool inherits = ConvertInherts(*inherits_value);

  // initial-value (optional)
  const CSSValue* initial_value =
      properties.GetPropertyCSSValue(CSSPropertyID::kInitialValue);
  scoped_refptr<CSSVariableData> initial_variable_data =
      ConvertInitialVariableData(initial_value);

  // Parse initial value, if we have it.
  const CSSValue* initial = nullptr;
  if (initial_variable_data) {
    const CSSParserContext* parser_context =
        document.ElementSheet().Contents()->ParserContext();
    const bool is_animation_tainted = false;
    initial = syntax->Parse(initial_variable_data->TokenRange(), parser_context,
                            is_animation_tainted);
    if (!initial)
      return nullptr;
    if (!ComputationallyIndependent(*initial))
      return nullptr;
    initial = &StyleBuilderConverter::ConvertRegisteredPropertyInitialValue(
        document, *initial);
    initial_variable_data =
        StyleBuilderConverter::ConvertRegisteredPropertyVariableData(
            *initial, is_animation_tainted);
  }

  return MakeGarbageCollected<PropertyRegistration>(
      name, *syntax, inherits, initial, initial_variable_data);
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
  Document* document = To<Document>(execution_context);
  PropertyRegistry& registry = *document->GetPropertyRegistry();
  if (registry.Registration(atomic_name)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidModificationError,
        "The name provided has already been registered.");
    return;
  }

  base::Optional<CSSSyntaxDefinition> syntax_definition =
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
  scoped_refptr<CSSVariableData> initial_variable_data;
  if (property_definition->hasInitialValue()) {
    CSSTokenizer tokenizer(property_definition->initialValue());
    const auto tokens = tokenizer.TokenizeToEOF();
    bool is_animation_tainted = false;
    initial = syntax_definition->Parse(CSSParserTokenRange(tokens),
                                       parser_context, is_animation_tainted);
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
    initial = &StyleBuilderConverter::ConvertRegisteredPropertyInitialValue(
        *document, *initial);
    initial_variable_data =
        StyleBuilderConverter::ConvertRegisteredPropertyVariableData(
            *initial, is_animation_tainted);
  } else {
    if (!syntax_definition->IsTokenStream()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kSyntaxError,
          "An initial value must be provided if the syntax is not '*'");
      return;
    }
  }
  registry.RegisterProperty(
      atomic_name,
      *MakeGarbageCollected<PropertyRegistration>(
          atomic_name, *syntax_definition, property_definition->inherits(),
          initial, std::move(initial_variable_data)));

  document->GetStyleEngine().CustomPropertyRegistered();
}

}  // namespace blink
