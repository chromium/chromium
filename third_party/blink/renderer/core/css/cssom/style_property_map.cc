// Copyright 2016 the chromium authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/style_property_map.h"

#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/core/css/cssom/css_style_value.h"
#include "third_party/blink/renderer/core/css/cssom/cssom_types.h"
#include "third_party/blink/renderer/core/css/cssom/style_value_factory.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

bool IsListValuedProperty(const CSSProperty& property,
                          const PropertyRegistration* registration) {
  if (property.IsRepeated())
    return true;
  // TODO(andruud): The concept of "list-valued properties" doesn't fully work
  // in all cases. See https://github.com/w3c/css-houdini-drafts/issues/823
  // For now we only consider a custom property list-valued if it has a single
  // syntax component that is repeatable (e.g. <length>+).
  if (property.IDEquals(CSSPropertyVariable) && registration) {
    const auto& components = registration->Syntax().Components();
    return components.size() == 1 && components[0].IsRepeatable();
  }
  return false;
}

CSSValueList* CssValueListForPropertyID(CSSPropertyID property_id) {
  DCHECK(CSSProperty::Get(property_id).IsRepeated());
  char separator = CSSProperty::Get(property_id).RepetitionSeparator();
  switch (separator) {
    case ' ':
      return CSSValueList::CreateSpaceSeparated();
    case ',':
      return CSSValueList::CreateCommaSeparated();
    case '/':
      return CSSValueList::CreateSlashSeparated();
    default:
      NOTREACHED();
      return nullptr;
  }
}

String StyleValueToString(const CSSProperty& property,
                          const CSSStyleValue& style_value,
                          const CSSSyntaxComponent* syntax_component) {
  if (style_value.GetType() == CSSStyleValue::kUnknownType)
    return style_value.toString();
  return style_value
      .ToCSSValueWithProperty(property.PropertyID(), syntax_component)
      ->CssText();
}

const CSSVariableReferenceValue* CreateVariableReferenceValue(
    const String& value,
    const CSSParserContext& context) {
  CSSTokenizer tokenizer(value);
  const auto tokens = tokenizer.TokenizeToEOF();
  CSSParserTokenRange range(tokens);
  scoped_refptr<CSSVariableData> variable_data = CSSVariableData::Create(
      range, false, false, context.BaseURL(), context.Charset());
  return CSSVariableReferenceValue::Create(variable_data, context);
}

const CSSVariableReferenceValue* CreateVariableReferenceValue(
    const CSSProperty& property,
    const AtomicString& custom_property_name,
    const PropertyRegistration& registration,
    const CSSStyleValueVector& values,
    const CSSParserContext& context) {
  DCHECK(IsListValuedProperty(property, &registration));
  DCHECK_EQ(registration.Syntax().Components().size(), 1U);

  char separator = registration.Syntax().Components()[0].Separator();

  StringBuilder builder;

  for (const auto& value : values) {
    const CSSSyntaxComponent* syntax_component = nullptr;

    if (!CSSOMTypes::PropertyCanTake(property.PropertyID(),
                                     custom_property_name, &registration,
                                     *value, syntax_component)) {
      return nullptr;
    }
    if (!builder.IsEmpty())
      builder.Append(separator);
    builder.Append(StyleValueToString(property, *value, syntax_component));
  }

  return CreateVariableReferenceValue(builder.ToString(), context);
}

const CSSValue* StyleValueToCSSValue(
    const CSSProperty& property,
    const AtomicString& custom_property_name,
    const PropertyRegistration* registration,
    const CSSStyleValue& style_value,
    const ExecutionContext& execution_context) {
  DCHECK_EQ(property.IDEquals(CSSPropertyVariable),
            !custom_property_name.IsNull());

  const CSSSyntaxComponent* syntax_component = nullptr;

  const CSSPropertyID property_id = property.PropertyID();
  if (!CSSOMTypes::PropertyCanTake(property_id, custom_property_name,
                                   registration, style_value,
                                   syntax_component)) {
    return nullptr;
  }

  if (style_value.GetType() == CSSStyleValue::kUnknownType &&
      // Registered custom properties must enter the CSSPropertyVariable
      // switch-case below, for proper parsing according to registered syntax.
      !(property_id == CSSPropertyVariable && registration)) {
    return CSSParser::ParseSingleValue(
        property.PropertyID(), style_value.toString(),
        CSSParserContext::Create(execution_context));
  }

  // Handle properties that use ad-hoc structures for their CSSValues:
  // TODO(https://crbug.com/545324): Move this into a method on
  // CSSProperty when there are more of these cases.
  switch (property_id) {
    case CSSPropertyVariable:
      if (registration &&
          style_value.GetType() != CSSStyleValue::kUnparsedType) {
        CSSParserContext* context = CSSParserContext::Create(execution_context);
        String string =
            StyleValueToString(property, style_value, syntax_component);
        return CreateVariableReferenceValue(string, *context);
      }
      break;
    case CSSPropertyBorderBottomLeftRadius:
    case CSSPropertyBorderBottomRightRadius:
    case CSSPropertyBorderTopLeftRadius:
    case CSSPropertyBorderTopRightRadius: {
      // level 1 only accept single <length-percentages>, but border-radius-*
      // expects pairs.
      const auto* value = style_value.ToCSSValue();
      if (value->IsPrimitiveValue()) {
        return CSSValuePair::Create(value, value,
                                    CSSValuePair::kDropIdenticalValues);
      }
      break;
    }
    case CSSPropertyContain: {
      // level 1 only accepts single values, which are stored internally
      // as a single element list.
      const auto* value = style_value.ToCSSValue();
      if ((value->IsIdentifierValue() && !value->IsCSSWideKeyword()) ||
          value->IsPrimitiveValue()) {
        CSSValueList* list = CSSValueList::CreateSpaceSeparated();
        list->Append(*style_value.ToCSSValue());
        return list;
      }
      break;
    }
    case CSSPropertyFontVariantEastAsian:
    case CSSPropertyFontVariantLigatures:
    case CSSPropertyFontVariantNumeric: {
      // level 1 only accept single keywords, but font-variant-* store
      // them as a list
      if (const auto* value =
              ToCSSIdentifierValueOrNull(style_value.ToCSSValue())) {
        // 'none' and 'normal' are stored as a single value
        if (value->GetValueID() == CSSValueNone ||
            value->GetValueID() == CSSValueNormal) {
          break;
        }

        CSSValueList* list = CSSValueList::CreateSpaceSeparated();
        list->Append(*style_value.ToCSSValue());
        return list;
      }
      break;
    }
    case CSSPropertyGridAutoFlow: {
      // level 1 only accepts single keywords
      const auto* value = style_value.ToCSSValue();
      // single keywords are wrapped in a list.
      if (value->IsIdentifierValue() && !value->IsCSSWideKeyword()) {
        CSSValueList* list = CSSValueList::CreateSpaceSeparated();
        list->Append(*style_value.ToCSSValue());
        return list;
      }
      break;
    }
    case CSSPropertyOffsetRotate: {
      // level 1 only accepts single values, which are stored internally
      // as a single element list.
      const auto* value = style_value.ToCSSValue();
      if ((value->IsIdentifierValue() && !value->IsCSSWideKeyword()) ||
          value->IsPrimitiveValue()) {
        CSSValueList* list = CSSValueList::CreateSpaceSeparated();
        list->Append(*style_value.ToCSSValue());
        return list;
      }
      break;
    }
    case CSSPropertyPaintOrder: {
      // level 1 only accepts single keywords
      const auto* value = style_value.ToCSSValue();
      // only 'normal' is stored as an identifier, the other keywords are
      // wrapped in a list.
      if (value->IsIdentifierValue() && !value->IsCSSWideKeyword() &&
          ToCSSIdentifierValue(value)->GetValueID() != CSSValueNormal) {
        CSSValueList* list = CSSValueList::CreateSpaceSeparated();
        list->Append(*style_value.ToCSSValue());
        return list;
      }
      break;
    }
    case CSSPropertyTextDecorationLine: {
      // level 1 only accepts single keywords
      const auto* value = style_value.ToCSSValue();
      // only 'none' is stored as an identifier, the other keywords are
      // wrapped in a list.
      if (value->IsIdentifierValue() && !value->IsCSSWideKeyword() &&
          ToCSSIdentifierValue(value)->GetValueID() != CSSValueNone) {
        CSSValueList* list = CSSValueList::CreateSpaceSeparated();
        list->Append(*style_value.ToCSSValue());
        return list;
      }
      break;
    }
    case CSSPropertyTextIndent: {
      // level 1 only accepts single values, which are stored internally
      // as a single element list.
      const auto* value = style_value.ToCSSValue();
      if (value->IsPrimitiveValue()) {
        CSSValueList* list = CSSValueList::CreateSpaceSeparated();
        list->Append(*value);
        return list;
      }
      break;
    }
    case CSSPropertyTransitionProperty:
    case CSSPropertyTouchAction: {
      // level 1 only accepts single keywords, which are stored internally
      // as a single element list
      const auto* value = style_value.ToCSSValue();
      if (value->IsIdentifierValue() && !value->IsCSSWideKeyword()) {
        CSSValueList* list = CSSValueList::CreateSpaceSeparated();
        list->Append(*style_value.ToCSSValue());
        return list;
      }
      break;
    }
    default:
      break;
  }

  return style_value.ToCSSValueWithProperty(property_id, syntax_component);
}

const CSSValue* CoerceStyleValueOrString(
    const CSSProperty& property,
    const AtomicString& custom_property_name,
    const PropertyRegistration* registration,
    const CSSStyleValueOrString& value,
    const ExecutionContext& execution_context) {
  DCHECK(!IsListValuedProperty(property, registration));
  DCHECK_EQ(property.IDEquals(CSSPropertyVariable),
            !custom_property_name.IsNull());

  if (value.IsCSSStyleValue()) {
    if (!value.GetAsCSSStyleValue())
      return nullptr;

    return StyleValueToCSSValue(property, custom_property_name, registration,
                                *value.GetAsCSSStyleValue(), execution_context);
  } else {
    DCHECK(value.IsString());
    const auto values = StyleValueFactory::FromString(
        property.PropertyID(), custom_property_name, registration,
        value.GetAsString(), CSSParserContext::Create(execution_context));
    if (values.size() != 1U)
      return nullptr;

    return StyleValueToCSSValue(property, custom_property_name, registration,
                                *values[0], execution_context);
  }
}

const CSSValue* CoerceStyleValuesOrStrings(
    const CSSProperty& property,
    const AtomicString& custom_property_name,
    const PropertyRegistration* registration,
    const HeapVector<CSSStyleValueOrString>& values,
    const ExecutionContext& execution_context) {
  DCHECK(IsListValuedProperty(property, registration));
  DCHECK_EQ(property.IDEquals(CSSPropertyVariable),
            !custom_property_name.IsNull());
  if (values.IsEmpty())
    return nullptr;

  CSSStyleValueVector style_values =
      StyleValueFactory::CoerceStyleValuesOrStrings(
          property, custom_property_name, registration, values,
          execution_context);

  if (style_values.IsEmpty())
    return nullptr;

  if (property.IDEquals(CSSPropertyVariable) && registration) {
    CSSParserContext* context = CSSParserContext::Create(execution_context);
    return CreateVariableReferenceValue(property, custom_property_name,
                                        *registration, style_values, *context);
  }

  CSSValueList* result = CssValueListForPropertyID(property.PropertyID());
  for (const auto& style_value : style_values) {
    const CSSValue* css_value =
        StyleValueToCSSValue(property, custom_property_name, nullptr,
                             *style_value, execution_context);
    if (!css_value)
      return nullptr;
    if (css_value->IsCSSWideKeyword() || css_value->IsVariableReferenceValue())
      return style_values.size() == 1U ? css_value : nullptr;
    result->Append(*css_value);
  }

  return result;
}

}  // namespace

void StylePropertyMap::set(const ExecutionContext* execution_context,
                           const String& property_name,
                           const HeapVector<CSSStyleValueOrString>& values,
                           ExceptionState& exception_state) {
  const CSSPropertyID property_id = cssPropertyID(property_name);
  if (property_id == CSSPropertyInvalid) {
    exception_state.ThrowTypeError("Invalid propertyName: " + property_name);
    return;
  }

  DCHECK(isValidCSSPropertyID(property_id));
  const CSSProperty& property = CSSProperty::Get(property_id);
  if (property.IsShorthand()) {
    if (values.size() != 1) {
      exception_state.ThrowTypeError("Invalid type for property");
      return;
    }

    String css_text;
    if (values[0].IsCSSStyleValue()) {
      CSSStyleValue* style_value = values[0].GetAsCSSStyleValue();
      const CSSSyntaxComponent* syntax_component = nullptr;
      if (style_value &&
          CSSOMTypes::PropertyCanTake(property_id, g_null_atom, nullptr,
                                      *style_value, syntax_component)) {
        css_text = style_value->toString();
      }
    } else {
      css_text = values[0].GetAsString();
    }

    if (css_text.IsEmpty() ||
        !SetShorthandProperty(property.PropertyID(), css_text,
                              execution_context->GetSecureContextMode()))
      exception_state.ThrowTypeError("Invalid type for property");

    return;
  }

  AtomicString custom_property_name = (property_id == CSSPropertyVariable)
                                          ? AtomicString(property_name)
                                          : g_null_atom;

  const PropertyRegistration* registration = nullptr;

  if (property_id == CSSPropertyVariable && IsA<Document>(execution_context)) {
    const PropertyRegistry* registry =
        To<Document>(*execution_context).GetPropertyRegistry();
    if (registry) {
      registration = registry->Registration(custom_property_name);
    }
  }

  const CSSValue* result = nullptr;
  if (IsListValuedProperty(property, registration)) {
    result =
        CoerceStyleValuesOrStrings(property, custom_property_name, registration,
                                   values, *execution_context);
  } else if (values.size() == 1U) {
    result =
        CoerceStyleValueOrString(property, custom_property_name, registration,
                                 values[0], *execution_context);
  }

  if (!result) {
    exception_state.ThrowTypeError("Invalid type for property");
    return;
  }

  if (property_id == CSSPropertyVariable)
    SetCustomProperty(custom_property_name, *result);
  else
    SetProperty(property_id, *result);
}

void StylePropertyMap::append(const ExecutionContext* execution_context,
                              const String& property_name,
                              const HeapVector<CSSStyleValueOrString>& values,
                              ExceptionState& exception_state) {
  if (values.IsEmpty())
    return;

  const CSSPropertyID property_id = cssPropertyID(property_name);

  if (property_id == CSSPropertyInvalid) {
    exception_state.ThrowTypeError("Invalid propertyName: " + property_name);
    return;
  }

  const CSSProperty& property = CSSProperty::Get(property_id);

  if (property_id == CSSPropertyVariable) {
    AtomicString custom_property_name(property_name);

    const PropertyRegistration* registration =
        PropertyRegistration::From(execution_context, custom_property_name);

    if (registration && IsListValuedProperty(property, registration)) {
      CSSStyleValueVector style_values;

      // Add existing CSSStyleValues:
      if (const CSSValue* css_value =
              GetCustomProperty(*execution_context, custom_property_name)) {
        DCHECK(css_value->IsValueList());
        style_values = StyleValueFactory::CssValueToStyleValueVector(
            property_id, custom_property_name, *css_value);
      }

      // Append incoming CSSStyleValues:
      CSSStyleValueVector incoming_style_values =
          StyleValueFactory::CoerceStyleValuesOrStrings(
              property, custom_property_name, registration, values,
              *execution_context);

      const CSSValue* result = nullptr;

      if (!incoming_style_values.IsEmpty()) {
        style_values.AppendVector(incoming_style_values);
        CSSParserContext* context =
            CSSParserContext::Create(*execution_context);
        result =
            CreateVariableReferenceValue(property, custom_property_name,
                                         *registration, style_values, *context);
      }

      if (!result) {
        exception_state.ThrowTypeError("Invalid type for property");
        return;
      }

      SetCustomProperty(custom_property_name, *result);
      return;
    }
    exception_state.ThrowTypeError(
        "Appending to custom properties is not supported");
    return;
  }

  if (!property.IsRepeated()) {
    exception_state.ThrowTypeError("Property does not support multiple values");
    return;
  }

  CSSValueList* current_value = nullptr;
  if (const CSSValue* css_value = GetProperty(property_id)) {
    DCHECK(css_value->IsValueList());
    current_value = ToCSSValueList(css_value)->Copy();
  } else {
    current_value = CssValueListForPropertyID(property_id);
  }

  const CSSValue* result = CoerceStyleValuesOrStrings(
      property, g_null_atom, nullptr, values, *execution_context);
  if (!result || !result->IsValueList()) {
    exception_state.ThrowTypeError("Invalid type for property");
    return;
  }

  for (const auto& value : *ToCSSValueList(result)) {
    current_value->Append(*value);
  }

  SetProperty(property_id, *current_value);
}

void StylePropertyMap::remove(const String& property_name,
                              ExceptionState& exception_state) {
  CSSPropertyID property_id = cssPropertyID(property_name);
  if (property_id == CSSPropertyInvalid) {
    exception_state.ThrowTypeError("Invalid property name: " + property_name);
    return;
  }

  if (property_id == CSSPropertyVariable) {
    RemoveCustomProperty(AtomicString(property_name));
  } else {
    RemoveProperty(property_id);
  }
}

void StylePropertyMap::clear() {
  RemoveAllProperties();
}

}  // namespace blink
