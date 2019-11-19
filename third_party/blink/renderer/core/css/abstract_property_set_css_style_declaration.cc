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

#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/style_attribute_mutation_scope.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"

namespace blink {

unsigned AbstractPropertySetCSSStyleDeclaration::length() const {
  return PropertySet().PropertyCount();
}

String AbstractPropertySetCSSStyleDeclaration::item(unsigned i) const {
  if (i >= PropertySet().PropertyCount())
    return "";
  CSSPropertyValueSet::PropertyReference property = PropertySet().PropertyAt(i);
  if (property.Id() == CSSPropertyID::kVariable)
    return To<CSSCustomPropertyDeclaration>(property.Value()).GetName();
  return property.Property().GetPropertyName();
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

  // A null execution_context may be passed in by the inspector, this shouldn't
  // occur normally.
  const SecureContextMode mode = execution_context
                                     ? execution_context->GetSecureContextMode()
                                     : SecureContextMode::kInsecureContext;

  PropertySet().ParseDeclarationList(text, mode, ContextStyleSheet());

  DidMutate(kPropertyChanged);

  mutation_scope.EnqueueMutationRecord();
}

String AbstractPropertySetCSSStyleDeclaration::getPropertyValue(
    const String& property_name) {
  CSSPropertyID property_id = cssPropertyID(property_name);
  if (!isValidCSSPropertyID(property_id))
    return String();
  if (property_id == CSSPropertyID::kVariable)
    return PropertySet().GetPropertyValue(AtomicString(property_name));
  return PropertySet().GetPropertyValue(property_id);
}

String AbstractPropertySetCSSStyleDeclaration::getPropertyPriority(
    const String& property_name) {
  CSSPropertyID property_id = cssPropertyID(property_name);
  if (!isValidCSSPropertyID(property_id))
    return String();

  bool important = false;
  if (property_id == CSSPropertyID::kVariable)
    important = PropertySet().PropertyIsImportant(AtomicString(property_name));
  else
    important = PropertySet().PropertyIsImportant(property_id);
  return important ? "important" : "";
}

String AbstractPropertySetCSSStyleDeclaration::GetPropertyShorthand(
    const String& property_name) {
  CSSPropertyID property_id = cssPropertyID(property_name);

  // Custom properties don't have shorthands, so we can ignore them here.
  if (!isValidCSSPropertyID(property_id) ||
      !CSSProperty::Get(property_id).IsLonghand())
    return String();
  CSSPropertyID shorthand_id = PropertySet().GetPropertyShorthand(property_id);
  if (!isValidCSSPropertyID(shorthand_id))
    return String();
  return CSSProperty::Get(shorthand_id).GetPropertyNameString();
}

bool AbstractPropertySetCSSStyleDeclaration::IsPropertyImplicit(
    const String& property_name) {
  CSSPropertyID property_id = cssPropertyID(property_name);

  // Custom properties don't have shorthands, so we can ignore them here.
  if (property_id < firstCSSProperty)
    return false;
  return PropertySet().IsPropertyImplicit(property_id);
}

void AbstractPropertySetCSSStyleDeclaration::setProperty(
    const ExecutionContext* execution_context,
    const String& property_name,
    const String& value,
    const String& priority,
    ExceptionState& exception_state) {
  CSSPropertyID property_id = unresolvedCSSPropertyID(property_name);
  if (!isValidCSSPropertyID(property_id))
    return;

  bool important = EqualIgnoringASCIICase(priority, "important");
  if (!important && !priority.IsEmpty())
    return;

  SetPropertyInternal(property_id, property_name, value, important,
                      execution_context->GetSecureContextMode(),
                      exception_state);
}

String AbstractPropertySetCSSStyleDeclaration::removeProperty(
    const String& property_name,
    ExceptionState& exception_state) {
  CSSPropertyID property_id = cssPropertyID(property_name);
  if (!isValidCSSPropertyID(property_id))
    return String();

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

  if (changed)
    mutation_scope.EnqueueMutationRecord();
  return result;
}

const CSSValue*
AbstractPropertySetCSSStyleDeclaration::GetPropertyCSSValueInternal(
    CSSPropertyID property_id) {
  return PropertySet().GetPropertyCSSValue(property_id);
}

const CSSValue*
AbstractPropertySetCSSStyleDeclaration::GetPropertyCSSValueInternal(
    AtomicString custom_property_name) {
  DCHECK_EQ(CSSPropertyID::kVariable, cssPropertyID(custom_property_name));
  return PropertySet().GetPropertyCSSValue(custom_property_name);
}

String AbstractPropertySetCSSStyleDeclaration::GetPropertyValueInternal(
    CSSPropertyID property_id) {
  return PropertySet().GetPropertyValue(property_id);
}

DISABLE_CFI_PERF
void AbstractPropertySetCSSStyleDeclaration::SetPropertyInternal(
    CSSPropertyID unresolved_property,
    const String& custom_property_name,
    const String& value,
    bool important,
    SecureContextMode secure_context_mode,
    ExceptionState&) {
  StyleAttributeMutationScope mutation_scope(this);
  WillMutate();

  bool did_change = false;
  if (unresolved_property == CSSPropertyID::kVariable) {
    AtomicString atomic_name(custom_property_name);

    bool is_animation_tainted = IsKeyframeStyle();
    did_change =
        PropertySet()
            .SetProperty(atomic_name, value, important, secure_context_mode,
                         ContextStyleSheet(), is_animation_tainted)
            .did_change;
  } else {
    did_change = PropertySet()
                     .SetProperty(unresolved_property, value, important,
                                  secure_context_mode, ContextStyleSheet())
                     .did_change;
  }

  DidMutate(did_change ? kPropertyChanged : kNoChanges);

  if (!did_change)
    return;

  Element* parent = ParentElement();
  if (parent) {
    parent->GetDocument().GetStyleEngine().AttributeChangedForElement(
        html_names::kStyleAttr, *parent);
  }
  mutation_scope.EnqueueMutationRecord();
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

void AbstractPropertySetCSSStyleDeclaration::Trace(blink::Visitor* visitor) {
  CSSStyleDeclaration::Trace(visitor);
}

}  // namespace blink
