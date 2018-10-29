// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/property_registration.h"

#include "third_party/blink/renderer/core/animation/css_interpolation_types_map.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/css_syntax_descriptor.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/css_variable_reference_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css/property_descriptor.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
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
    const CSSSyntaxDescriptor& syntax,
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

  if (value.IsVariableReferenceValue())
    return !ToCSSVariableReferenceValue(value)
                .VariableDataValue()
                ->NeedsVariableResolution();

  if (value.IsValueList()) {
    for (const CSSValue* inner_value : ToCSSValueList(value)) {
      if (!ComputationallyIndependent(*inner_value))
        return false;
    }
    return true;
  }

  if (value.IsPrimitiveValue()) {
    const CSSPrimitiveValue& primitive_value = ToCSSPrimitiveValue(value);
    if (!primitive_value.IsLength() &&
        !primitive_value.IsCalculatedPercentageWithLength())
      return true;

    CSSPrimitiveValue::CSSLengthArray length_array;
    primitive_value.AccumulateLengthArray(length_array);
    for (size_t i = 0; i < length_array.values.size(); i++) {
      if (length_array.type_flags.Get(i) &&
          i != CSSPrimitiveValue::kUnitTypePixels &&
          i != CSSPrimitiveValue::kUnitTypePercentage)
        return false;
    }
    return true;
  }

  // TODO(timloh): Images values can also contain lengths.

  return true;
}

void PropertyRegistration::registerProperty(
    ExecutionContext* execution_context,
    const PropertyDescriptor& descriptor,
    ExceptionState& exception_state) {
  // Bindings code ensures these are set.
  DCHECK(descriptor.hasName());
  DCHECK(descriptor.hasInherits());
  DCHECK(descriptor.hasSyntax());

  String name = descriptor.name();
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

  CSSSyntaxDescriptor syntax_descriptor(descriptor.syntax());
  if (!syntax_descriptor.IsValid()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The syntax provided is not a valid custom property syntax.");
    return;
  }

  const CSSParserContext* parser_context =
      document->ElementSheet().Contents()->ParserContext();

  const CSSValue* initial = nullptr;
  scoped_refptr<CSSVariableData> initial_variable_data;
  if (descriptor.hasInitialValue()) {
    CSSTokenizer tokenizer(descriptor.initialValue());
    const auto tokens = tokenizer.TokenizeToEOF();
    bool is_animation_tainted = false;
    initial = syntax_descriptor.Parse(CSSParserTokenRange(tokens),
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
    initial =
        &StyleBuilderConverter::ConvertRegisteredPropertyInitialValue(*initial);
    initial_variable_data = CSSVariableData::Create(
        CSSParserTokenRange(tokens), is_animation_tainted, false,
        parser_context->BaseURL(), parser_context->Charset());
  } else {
    if (!syntax_descriptor.IsTokenStream()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kSyntaxError,
          "An initial value must be provided if the syntax is not '*'");
      return;
    }
  }
  registry.RegisterProperty(
      atomic_name, *new PropertyRegistration(atomic_name, syntax_descriptor,
                                             descriptor.inherits(), initial,
                                             std::move(initial_variable_data)));

  document->GetStyleEngine().CustomPropertyRegistered();
}

}  // namespace blink
