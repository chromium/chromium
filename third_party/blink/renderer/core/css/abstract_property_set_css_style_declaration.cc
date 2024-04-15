/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/css/abstract_property_set_css_style_declaration.h"

#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"
#include "third_party/blink/renderer/core/css/style_attribute_mutation_scope.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"

namespace blink {

unsigned AbstractPropertySetCSSStyleDeclaration::length() const {
  return PropertySet().PropertyCount();
}

String AbstractPropertySetCSSStyleDeclaration::item(unsigned i) const {
  if (i >= PropertySet().PropertyCount()) {
    return "";
  }
  return PropertySet().PropertyAt(i).Name().ToAtomicString();
}

String AbstractPropertySetCSSStyleDeclaration::cssText() const {
  return PropertySet().AsText();
}

void AbstractPropertySetCSSStyleDeclaration::setCSSText(
    const ExecutionContext* execution_context,
    const String& text,
    ExceptionState&) {
  StyleAttributeMutationScope mutation_scope(this);
  WillMutate();

  const SecureContextMode mode = execution_context
                                     ? execution_context->GetSecureContextMode()
                                     : SecureContextMode::kInsecureContext;
  PropertySet().ParseDeclarationList(text, mode, ContextStyleSheet());

  DidMutate(kPropertyChanged);

  mutation_scope.EnqueueMutationRecord();
}

String AbstractPropertySetCSSStyleDeclaration::getPropertyValue(
    const String& property_name) {
  CSSPropertyID property_id =
      CssPropertyID(GetExecutionContext(), property_name);
  if (!IsValidCSSPropertyID(property_id)) {
    return String();
  }
  if (property_id == CSSPropertyID::kVariable) {
    return PropertySet().GetPropertyValue(AtomicString(property_name));
  }
  return PropertySet().GetPropertyValue(property_id);
}

String AbstractPropertySetCSSStyleDeclaration::getPropertyPriority(
    const String& property_name) {
  CSSPropertyID property_id =
      CssPropertyID(GetExecutionContext(), property_name);
  if (!IsValidCSSPropertyID(property_id)) {
    return String();
  }

  bool important = false;
  if (property_id == CSSPropertyID::kVariable) {
    important = PropertySet().PropertyIsImportant(AtomicString(property_name));
  } else {
    important = PropertySet().PropertyIsImportant(property_id);
  }
  return important ? "important" : "";
}

String AbstractPropertySetCSSStyleDeclaration::GetPropertyShorthand(
    const String& property_name) {
  CSSPropertyID property_id =
      CssPropertyID(GetExecutionContext(), property_name);

  // Custom properties don't have shorthands, so we can ignore them here.
  if (!IsValidCSSPropertyID(property_id) ||
      !CSSProperty::Get(property_id).IsLonghand()) {
    return String();
  }
  CSSPropertyID shorthand_id = PropertySet().GetPropertyShorthand(property_id);
  if (!IsValidCSSPropertyID(shorthand_id)) {
    return String();
  }
  return CSSProperty::Get(shorthand_id).GetPropertyNameString();
}

bool AbstractPropertySetCSSStyleDeclaration::IsPropertyImplicit(
    const String& property_name) {
  CSSPropertyID property_id =
      CssPropertyID(GetExecutionContext(), property_name);

  // Custom properties don't have shorthands, so we can ignore them here.
  if (property_id < kFirstCSSProperty) {
    return false;
  }
  return PropertySet().IsPropertyImplicit(property_id);
}

void AbstractPropertySetCSSStyleDeclaration::setProperty(
    const ExecutionContext* execution_context,
    const String& property_name,
    const String& value,
    const String& priority,
    ExceptionState& exception_state) {
  CSSPropertyID property_id =
      UnresolvedCSSPropertyID(execution_context, property_name);
  if (!IsValidCSSPropertyID(property_id) || !IsPropertyValid(property_id)) {
    return;
  }

  bool important = EqualIgnoringASCIICase(priority, "important");
  if (!important && !priority.empty()) {
    return;
  }

  const SecureContextMode mode = execution_context
                                     ? execution_context->GetSecureContextMode()
                                     : SecureContextMode::kInsecureContext;
  SetPropertyInternal(property_id, property_name, value, important, mode,
                      exception_state);
}

String AbstractPropertySetCSSStyleDeclaration::removeProperty(
    const String& property_name,
    ExceptionState& exception_state) {
  CSSPropertyID property_id =
      CssPropertyID(GetExecutionContext(), property_name);
  if (!IsValidCSSPropertyID(property_id)) {
    return String();
  }

  StyleAttributeMutationScope mutation_scope(this);
  WillMutate();

  String result;
  bool changed = false;
  if (property_id == CSSPropertyID::kVariable) {
    changed =
        PropertySet().RemoveProperty(AtomicString(property_name), &result);
  } else {
    changed = PropertySet().RemoveProperty(property_id, &result);
  }

  DidMutate(changed ? kPropertyChanged : kNoChanges);

  if (changed) {
    mutation_scope.EnqueueMutationRecord();
  }
  return result;
}

const CSSValue*
AbstractPropertySetCSSStyleDeclaration::GetPropertyCSSValueInternal(
    CSSPropertyID property_id) {
  return PropertySet().GetPropertyCSSValue(property_id);
}

const CSSValue*
AbstractPropertySetCSSStyleDeclaration::GetPropertyCSSValueInternal(
    const AtomicString& custom_property_name) {
  DCHECK_EQ(CSSPropertyID::kVariable,
            CssPropertyID(GetExecutionContext(), custom_property_name));
  return PropertySet().GetPropertyCSSValue(custom_property_name);
}

String AbstractPropertySetCSSStyleDeclaration::GetPropertyValueInternal(
    CSSPropertyID property_id) {
  return PropertySet().GetPropertyValue(property_id);
}

String AbstractPropertySetCSSStyleDeclaration::GetPropertyValueWithHint(
    const String& property_name,
    unsigned index) {
  CSSPropertyID property_id =
      CssPropertyID(GetExecutionContext(), property_name);
  if (!IsValidCSSPropertyID(property_id)) {
    return String();
  }
  if (property_id == CSSPropertyID::kVariable) {
    return PropertySet().GetPropertyValueWithHint(AtomicString(property_name),
                                                  index);
  }
  return PropertySet().GetPropertyValue(property_id);
}

String AbstractPropertySetCSSStyleDeclaration::GetPropertyPriorityWithHint(
    const String& property_name,
    unsigned index) {
  CSSPropertyID property_id =
      CssPropertyID(GetExecutionContext(), property_name);
  if (!IsValidCSSPropertyID(property_id)) {
    return String();
  }
  bool important = false;
  if (property_id == CSSPropertyID::kVariable) {
    important = PropertySet().PropertyIsImportantWithHint(
        AtomicString(property_name), index);
  } else {
    important = PropertySet().PropertyIsImportant(property_id);
  }
  return important ? "important" : "";
}

DISABLE_CFI_PERF
void AbstractPropertySetCSSStyleDeclaration::SetPropertyInternal(
    CSSPropertyID unresolved_property,
    const String& custom_property_name,
    StringView value,
    bool important,
    SecureContextMode secure_context_mode,
    ExceptionState&) {
  StyleAttributeMutationScope mutation_scope(this);
  WillMutate();

  MutableCSSPropertyValueSet::SetResult result;
  if (unresolved_property == CSSPropertyID::kVariable) {
    AtomicString atomic_name(custom_property_name);

    bool is_animation_tainted = IsKeyframeStyle();
    result = PropertySet().ParseAndSetCustomProperty(
        atomic_name, value, important, secure_context_mode, ContextStyleSheet(),
        is_animation_tainted);
  } else {
    result = PropertySet().ParseAndSetProperty(unresolved_property, value,
                                               important, secure_context_mode,
                                               ContextStyleSheet());
  }

  if (result == MutableCSSPropertyValueSet::kParseError ||
      result == MutableCSSPropertyValueSet::kUnchanged) {
    DidMutate(kNoChanges);
    return;
  }

  CSSPropertyID property_id = ResolveCSSPropertyID(unresolved_property);

  if (result == MutableCSSPropertyValueSet::kModifiedExisting &&
      CSSProperty::Get(property_id).SupportsIncrementalStyle()) {
    DidMutate(kIndependentPropertyChanged);
  } else {
    DidMutate(kPropertyChanged);
  }

  mutation_scope.EnqueueMutationRecord();
}

bool AbstractPropertySetCSSStyleDeclaration::FastPathSetProperty(
    CSSPropertyID unresolved_property,
    double value) {
  if (unresolved_property == CSSPropertyID::kVariable) {
    // We don't bother with the fast path for custom properties,
    // even though we could.
    return false;
  }
  if (!std::isfinite(value)) {
    // Just to be on the safe side.
    return false;
  }
  CSSPropertyID property_id = ResolveCSSPropertyID(unresolved_property);
  const CSSProperty& property = CSSProperty::Get(property_id);
  if (!property.AcceptsNumericLiteral()) {
    // Not all properties are prepared to accept numeric literals;
    // e.g. widths could accept doubles but want to convert them
    // to lengths, and shorthand properties may want to do their
    // own things. We don't support either yet, only specifically
    // allowlisted properties.
    return false;
  }

  StyleAttributeMutationScope mutation_scope(this);
  WillMutate();

  const CSSValue* css_value = CSSNumericLiteralValue::Create(
      value, CSSPrimitiveValue::UnitType::kNumber);
  MutableCSSPropertyValueSet::SetResult result =
      PropertySet().SetLonghandProperty(
          CSSPropertyValue(CSSPropertyName(property_id), *css_value,
                           /*important=*/false));

  if (result == MutableCSSPropertyValueSet::kParseError ||
      result == MutableCSSPropertyValueSet::kUnchanged) {
    DidMutate(kNoChanges);
    return true;
  }

  if (result == MutableCSSPropertyValueSet::kModifiedExisting &&
      property.SupportsIncrementalStyle()) {
    DidMutate(kIndependentPropertyChanged);
  } else {
    DidMutate(kPropertyChanged);
  }

  mutation_scope.EnqueueMutationRecord();
  return true;
}

DISABLE_CFI_PERF
StyleSheetContents* AbstractPropertySetCSSStyleDeclaration::ContextStyleSheet()
    const {
  CSSStyleSheet* css_style_sheet = ParentStyleSheet();
  return css_style_sheet ? css_style_sheet->Contents() : nullptr;
}

bool AbstractPropertySetCSSStyleDeclaration::CssPropertyMatches(
    CSSPropertyID property_id,
    const CSSValue& property_value) const {
  return PropertySet().PropertyMatches(property_id, property_value);
}

void AbstractPropertySetCSSStyleDeclaration::Trace(Visitor* visitor) const {
  CSSStyleDeclaration::Trace(visitor);
}

}  // namespace blink
