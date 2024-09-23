// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/style_property_map.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssstylevalue_string.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "third_party/blink/renderer/core/css/css_scoped_keyword_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/core/css/cssom/css_style_value.h"
#include "third_party/blink/renderer/core/css/cssom/cssom_types.h"
#include "third_party/blink/renderer/core/css/cssom/style_value_factory.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

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
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

const CSSValue* StyleValueToCSSValue(
    const CSSProperty& property,
    const AtomicString& custom_property_name,
    const CSSStyleValue& style_value,
    const ExecutionContext& execution_context) {
  DCHECK_EQ(property.IDEquals(CSSPropertyID::kVariable),
            !custom_property_name.IsNull());

  const CSSPropertyID property_id = property.PropertyID();
  if (!CSSOMTypes::PropertyCanTake(property_id, custom_property_name,
                                   style_value)) {
    return nullptr;
  }

  if (style_value.GetType() == CSSStyleValue::kUnknownType) {
    return CSSParser::ParseSingleValue(
        property.PropertyID(), style_value.toString(),
        MakeGarbageCollected<CSSParserContext>(execution_context));
  }

  // Handle properties that use ad-hoc structures for their CSSValues:
  // TODO(https://crbug.com/545324): Move this into a method on
  // CSSProperty when there are more of these cases.
  switch (property_id) {
    case CSSPropertyID::kAnchorScope: {
      // The 'all' keyword is tree-scoped.
      if (const auto* ident =
              DynamicTo<CSSIdentifierValue>(style_value.ToCSSValue());
          ident && ident->GetValueID() == CSSValueID::kAll) {
        return MakeGarbageCollected<cssvalue::CSSScopedKeywordValue>(
            ident->GetValueID());
      }
      break;
    }
    case CSSPropertyID::kBorderBottomLeftRadius:
    case CSSPropertyID::kBorderBottomRightRadius:
    case CSSPropertyID::kBorderTopLeftRadius:
    case CSSPropertyID::kBorderTopRightRadius:
    case CSSPropertyID::kBorderEndEndRadius:
    case CSSPropertyID::kBorderEndStartRadius:
    case CSSPropertyID::kBorderStartEndRadius:
    case CSSPropertyID::kBorderStartStartRadius: {
      // level 1 only accept single <length-percentages>, but border-radius-*
      // expects pairs.
      const auto* value = style_value.ToCSSValue();
      if (value->IsPrimitiveValue()) {
        return MakeGarbageCollected<CSSValuePair>(
            value, value, CSSValuePair::kDropIdenticalValues);
      }
      break;
    }
    case CSSPropertyID::kClipPath: {
      // level 1 only accepts single keywords
      const auto* value = style_value.ToCSSValue();
      // only 'none' is stored as an identifier, the other keywords are
      // wrapped in a list.
      auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
      if (identifier_value && !value->IsCSSWideKeyword() &&
          identifier_value->GetValueID() != CSSValueID::kNone) {
        CSSValueList* list = CSSValueList::CreateSpaceSeparated();
        list->Append(*style_value.ToCSSValue());
        return list;
      }
      break;
    }
    case CSSPropertyID::kContain:
    case CSSPropertyID::kContainerType: {
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
    case CSSPropertyID::kFontVariantEastAsian:
    case CSSPropertyID::kFontVariantLigatures:
    case CSSPropertyID::kFontVariantNumeric: {
      // level 1 only accept single keywords, but font-variant-* store
      // them as a list
      if (const auto* value =
              DynamicTo<CSSIdentifierValue>(style_value.ToCSSValue())) {
        // 'none' and 'normal' are stored as a single value
        if (value->GetValueID() == CSSValueID::kNone ||
            value->GetValueID() == CSSValueID::kNormal) {
          break;
        }

        CSSValueList* list = CSSValueList::CreateSpaceSeparated();
        list->Append(*style_value.ToCSSValue());
        return list;
      }
      break;
    }
    case CSSPropertyID::kGridAutoFlow: {
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
    case CSSPropertyID::kOffsetRotate: {
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
    case CSSPropertyID::kPaintOrder: {
      // level 1 only accepts single keywords
      const auto* value = style_value.ToCSSValue();
      // only 'normal' is stored as an identifier, the other keywords are
      // wrapped in a list.
      auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
      if (identifier_value && !value->IsCSSWideKeyword() &&
          identifier_value->GetValueID() != CSSValueID::kNormal) {
        CSSValueList* list = CSSValueList::CreateSpaceSeparated();
        list->Append(*style_value.ToCSSValue());
        return list;
      }
      break;
    }
    case CSSPropertyID::kTextDecorationLine: {
      // level 1 only accepts single keywords
      const auto* value = style_value.ToCSSValue();
      // only 'none' is stored as an identifier, the other keywords are
      // wrapped in a list.
      auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
      if (identifier_value && !value->IsCSSWideKeyword() &&
          identifier_value->GetValueID() != CSSValueID::kNone) {
        CSSValueList* list = CSSValueList::CreateSpaceSeparated();
        list->Append(*style_value.ToCSSValue());
        return list;
      }
      break;
    }
    case CSSPropertyID::kTextIndent: {
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
    case CSSPropertyID::kTransitionProperty:
    case CSSPropertyID::kTouchAction: {
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

  return style_value.ToCSSValueWithProperty(property_id);
}

const CSSValue* CoerceStyleValueOrString(
    const CSSProperty& property,
    const AtomicString& custom_property_name,
    const V8UnionCSSStyleValueOrString* value,
    const ExecutionContext& execution_context) {
  DCHECK(!property.IsRepeated());
  DCHECK_EQ(property.IDEquals(CSSPropertyID::kVariable),
            !custom_property_name.IsNull());
  DCHECK(value);

  switch (value->GetContentType()) {
    case V8UnionCSSStyleValueOrString::ContentType::kCSSStyleValue:
      return StyleValueToCSSValue(property, custom_property_name,
                                  *value->GetAsCSSStyleValue(),
                                  execution_context);
    case V8UnionCSSStyleValueOrString::ContentType::kString: {
      const auto& values = StyleValueFactory::FromString(
          property.PropertyID(), custom_property_name, value->GetAsString(),
          MakeGarbageCollected<CSSParserContext>(execution_context));
      if (values.size() != 1U) {
        return nullptr;
      }
      return StyleValueToCSSValue(property, custom_property_name, *values[0],
                                  execution_context);
    }
  }

  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

const CSSValue* CoerceStyleValuesOrStrings(
    const CSSProperty& property,
    const AtomicString& custom_property_name,
    const HeapVector<Member<V8UnionCSSStyleValueOrString>>& values,
    const ExecutionContext& execution_context) {
  DCHECK(property.IsRepeated());
  DCHECK_EQ(property.IDEquals(CSSPropertyID::kVariable),
            !custom_property_name.IsNull());
  if (values.empty()) {
    return nullptr;
  }

  CSSStyleValueVector style_values =
      StyleValueFactory::CoerceStyleValuesOrStrings(
          property, custom_property_name, values, execution_context);

  if (style_values.empty()) {
    return nullptr;
  }

  CSSValueList* result = CssValueListForPropertyID(property.PropertyID());
  for (const auto& style_value : style_values) {
    const CSSValue* css_value = StyleValueToCSSValue(
        property, custom_property_name, *style_value, execution_context);
    if (!css_value) {
      return nullptr;
    }
    if (css_value->IsCSSWideKeyword() || css_value->IsUnparsedDeclaration()) {
      return style_values.size() == 1U ? css_value : nullptr;
    }
    result->Append(*css_value);
  }

  return result;
}

}  // namespace

void StylePropertyMap::set(
    const ExecutionContext* execution_context,
    const String& property_name,
    const HeapVector<Member<V8UnionCSSStyleValueOrString>>& values,
    ExceptionState& exception_state) {
  const CSSPropertyID property_id =
      CssPropertyID(execution_context, property_name);
  if (property_id == CSSPropertyID::kInvalid) {
    exception_state.ThrowTypeError("Invalid propertyName: " + property_name);
    return;
  }

  DCHECK(IsValidCSSPropertyID(property_id));
  const CSSProperty& property = CSSProperty::Get(property_id);

  // Descriptors (like 'src') have CSSProperty instances, but are not
  // valid properties in this context.
  if (!property.IsProperty()) {
    exception_state.ThrowTypeError("Invalid propertyName: " + property_name);
    return;
  }

  if (property.IsShorthand()) {
    if (values.size() != 1) {
      exception_state.ThrowTypeError("Invalid type for property");
      return;
    }

    String css_text;
    switch (values[0]->GetContentType()) {
      case V8UnionCSSStyleValueOrString::ContentType::kCSSStyleValue: {
        CSSStyleValue* style_value = values[0]->GetAsCSSStyleValue();
        if (CSSOMTypes::PropertyCanTake(property_id, g_null_atom,
                                        *style_value)) {
          css_text = style_value->toString();
        }
        break;
      }
      case V8UnionCSSStyleValueOrString::ContentType::kString:
        css_text = values[0]->GetAsString();
        break;
    }

    if (css_text.empty() ||
        !SetShorthandProperty(property.PropertyID(), css_text,
                              execution_context->GetSecureContextMode())) {
      exception_state.ThrowTypeError("Invalid type for property");
    }

    return;
  }

  const AtomicString& custom_property_name =
      (property_id == CSSPropertyID::kVariable) ? AtomicString(property_name)
                                                : g_null_atom;

  const CSSValue* result = nullptr;
  if (property.IsRepeated()) {
    result = CoerceStyleValuesOrStrings(property, custom_property_name, values,
                                        *execution_context);
  } else if (values.size() == 1U) {
    result = CoerceStyleValueOrString(property, custom_property_name, values[0],
                                      *execution_context);
  }

  if (!result) {
    exception_state.ThrowTypeError("Invalid type for property");
    return;
  }

  if (property_id == CSSPropertyID::kVariable) {
    SetCustomProperty(custom_property_name, *result);
  } else {
    SetProperty(property_id, *result);
  }
}

void StylePropertyMap::append(
    const ExecutionContext* execution_context,
    const String& property_name,
    const HeapVector<Member<V8UnionCSSStyleValueOrString>>& values,
    ExceptionState& exception_state) {
  if (values.empty()) {
    return;
  }

  const CSSPropertyID property_id =
      CssPropertyID(execution_context, property_name);

  if (property_id == CSSPropertyID::kInvalid) {
    exception_state.ThrowTypeError("Invalid propertyName: " + property_name);
    return;
  }

  const CSSProperty& property = CSSProperty::Get(property_id);

  if (property_id == CSSPropertyID::kVariable) {
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
    if (css_value->IsUnparsedDeclaration() ||
        css_value->IsPendingSubstitutionValue()) {
      // https://drafts.css-houdini.org/css-typed-om/#dom-stylepropertymap-append
      // 8. If props[property] contains a var() reference, throw a TypeError.
      exception_state.ThrowTypeError(
          "Cannot append to a list containing a variable reference");
      return;
    }
    if (!css_value->IsValueList()) {
      // The standard doesn't seem to cover this explicitly
      // (https://github.com/w3c/css-houdini-drafts/issues/823),
      // but the only really reasonable solution seems to be
      // to throw a TypeError.
      //
      // This covers e.g. system-wide CSS keywords, like inherit.
      exception_state.ThrowTypeError(
          "Cannot append to something that is not a list");
      return;
    }
    current_value = To<CSSValueList>(css_value)->Copy();
  } else {
    current_value = CssValueListForPropertyID(property_id);
  }

  const CSSValue* result = CoerceStyleValuesOrStrings(
      property, g_null_atom, values, *execution_context);
  const auto* result_value_list = DynamicTo<CSSValueList>(result);
  if (!result_value_list) {
    exception_state.ThrowTypeError("Invalid type for property");
    return;
  }

  for (const auto& value : *result_value_list) {
    current_value->Append(*value);
  }

  SetProperty(property_id, *current_value);
}

void StylePropertyMap::remove(const ExecutionContext* execution_context,
                              const String& property_name,
                              ExceptionState& exception_state) {
  CSSPropertyID property_id = CssPropertyID(execution_context, property_name);
  if (property_id == CSSPropertyID::kInvalid) {
    exception_state.ThrowTypeError("Invalid property name: " + property_name);
    return;
  }

  if (property_id == CSSPropertyID::kVariable) {
    RemoveCustomProperty(AtomicString(property_name));
  } else {
    RemoveProperty(property_id);
  }
}

void StylePropertyMap::clear() {
  RemoveAllProperties();
}

}  // namespace blink
