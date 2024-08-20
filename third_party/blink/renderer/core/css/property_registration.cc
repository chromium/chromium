// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/property_registration.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_property_definition.h"
#include "third_party/blink/renderer/core/animation/css_interpolation_types_map.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_string_value.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/css_syntax_definition.h"
#include "third_party/blink/renderer/core/css/css_syntax_string_parser.h"
#include "third_party/blink/renderer/core/css/css_unparsed_declaration_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
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
                                           const CSSValue* initial,
                                           StyleRuleProperty* property_rule)
    : syntax_(syntax),
      inherits_(inherits),
      initial_(initial),
      property_rule_(property_rule),
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

void PropertyRegistration::Trace(Visitor* visitor) const {
  visitor->Trace(initial_);
  visitor->Trace(property_rule_);
}

static bool ComputationallyIndependent(const CSSValue& value) {
  DCHECK(!value.IsCSSWideKeyword());

  if (auto* variable_reference_value =
          DynamicTo<CSSUnparsedDeclarationValue>(value)) {
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

std::optional<CSSSyntaxDefinition> PropertyRegistration::ConvertSyntax(
    const CSSValue* syntax_value) {
  // https://drafts.css-houdini.org/css-properties-values-api-1/#the-syntax-descriptor
  if (!syntax_value) {
    return {};
  }
  return CSSSyntaxStringParser(To<CSSStringValue>(*syntax_value).Value())
      .Parse();
}

std::optional<bool> PropertyRegistration::ConvertInherits(
    const CSSValue* inherits_value) {
  // https://drafts.css-houdini.org/css-properties-values-api-1/#inherits-descriptor
  if (!inherits_value) {
    return {};
  }

  CSSValueID inherits_id = To<CSSIdentifierValue>(*inherits_value).GetValueID();
  DCHECK(inherits_id == CSSValueID::kTrue || inherits_id == CSSValueID::kFalse);
  return inherits_id == CSSValueID::kTrue;
}

std::optional<const CSSValue*> PropertyRegistration::ConvertInitial(
    const CSSValue* initial_value,
    const CSSSyntaxDefinition& syntax,
    const CSSParserContext& parser_context) {
  // https://drafts.css-houdini.org/css-properties-values-api-1/#initial-value-descriptor
  if (!initial_value) {
    return syntax.IsUniversal() ? std::make_optional(nullptr) : std::nullopt;
  }
  CSSVariableData* initial_variable_data =
      To<CSSUnparsedDeclarationValue>(*initial_value).VariableDataValue();

  // Parse initial value, if we have it.
  const CSSValue* initial = nullptr;
  if (initial_variable_data) {
    const bool is_animation_tainted = false;
    initial = syntax.Parse(initial_variable_data->OriginalText(),
                           parser_context, is_animation_tainted);
    if (!initial) {
      return {};
    }
    if (!ComputationallyIndependent(*initial)) {
      return {};
    }
  }
  // For non-universal @property rules, the initial value is required for the
  // the rule to be valid.
  if (!initial && !syntax.IsUniversal()) {
    return {};
  }

  return initial;
}

PropertyRegistration* PropertyRegistration::MaybeCreateForDeclaredProperty(
    Document& document,
    const AtomicString& name,
    StyleRuleProperty& rule) {
  std::optional<CSSSyntaxDefinition> syntax = ConvertSyntax(rule.GetSyntax());
  if (!syntax.has_value()) {
    return nullptr;
  }
  std::optional<bool> inherits = ConvertInherits(rule.Inherits());
  if (!inherits.has_value()) {
    return nullptr;
  }
  const CSSParserContext* parser_context =
      document.ElementSheet().Contents()->ParserContext();

  std::optional<const CSSValue*> initial =
      ConvertInitial(rule.GetInitialValue(), *syntax, *parser_context);
  if (!initial.has_value()) {
    return nullptr;
  }

  return MakeGarbageCollected<PropertyRegistration>(name, *syntax, *inherits,
                                                    *initial, &rule);
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

  std::optional<CSSSyntaxDefinition> syntax_definition =
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
    bool is_animation_tainted = false;
    initial = syntax_definition->Parse(property_definition->initialValue(),
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
