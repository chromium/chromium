// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/aom/accessible_node.h"

#include "base/ranges/algorithm.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/custom/element_internals.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

namespace {

QualifiedName GetCorrespondingARIAAttribute(AOMStringProperty property) {
  switch (property) {
    case AOMStringProperty::kAutocomplete:
      return html_names::kAriaAutocompleteAttr;
    case AOMStringProperty::kAriaBrailleLabel:
      return html_names::kAriaBraillelabelAttr;
    case AOMStringProperty::kAriaBrailleRoleDescription:
      return html_names::kAriaBrailleroledescriptionAttr;
    case AOMStringProperty::kChecked:
      return html_names::kAriaCheckedAttr;
    case AOMStringProperty::kColIndexText:
      return html_names::kAriaColindextextAttr;
    case AOMStringProperty::kCurrent:
      return html_names::kAriaCurrentAttr;
    case AOMStringProperty::kDescription:
      return html_names::kAriaDescriptionAttr;
    case AOMStringProperty::kHasPopup:
      return html_names::kAriaHaspopupAttr;
    case AOMStringProperty::kInvalid:
      return html_names::kAriaInvalidAttr;
    case AOMStringProperty::kKeyShortcuts:
      return html_names::kAriaKeyshortcutsAttr;
    case AOMStringProperty::kLabel:
      return html_names::kAriaLabelAttr;
    case AOMStringProperty::kLive:
      return html_names::kAriaLiveAttr;
    case AOMStringProperty::kOrientation:
      return html_names::kAriaOrientationAttr;
    case AOMStringProperty::kPlaceholder:
      return html_names::kAriaPlaceholderAttr;
    case AOMStringProperty::kPressed:
      return html_names::kAriaPressedAttr;
    case AOMStringProperty::kRelevant:
      return html_names::kAriaRelevantAttr;
    case AOMStringProperty::kRole:
      return html_names::kRoleAttr;
    case AOMStringProperty::kRoleDescription:
      return html_names::kAriaRoledescriptionAttr;
    case AOMStringProperty::kRowIndexText:
      return html_names::kAriaRowindextextAttr;
    case AOMStringProperty::kSort:
      return html_names::kAriaSortAttr;
    case AOMStringProperty::kValueText:
      return html_names::kAriaValuetextAttr;
    case AOMStringProperty::kVirtualContent:
      return html_names::kAriaVirtualcontentAttr;
  }

  NOTREACHED_IN_MIGRATION();
  return g_null_name;
}

QualifiedName GetCorrespondingARIAAttribute(AOMRelationProperty property) {
  switch (property) {
    case AOMRelationProperty::kActiveDescendant:
      return html_names::kAriaActivedescendantAttr;
  }

  NOTREACHED_IN_MIGRATION();
  return g_null_name;
}

QualifiedName GetCorrespondingARIAAttribute(AOMRelationListProperty property) {
  switch (property) {
    case AOMRelationListProperty::kDescribedBy:
      return html_names::kAriaDescribedbyAttr;
    case AOMRelationListProperty::kDetails:
      return html_names::kAriaDetailsAttr;
    case AOMRelationListProperty::kControls:
      return html_names::kAriaControlsAttr;
    case AOMRelationListProperty::kErrorMessage:
      return html_names::kAriaErrormessageAttr;
    case AOMRelationListProperty::kFlowTo:
      return html_names::kAriaFlowtoAttr;
    case AOMRelationListProperty::kLabeledBy:
      // Note that there are two allowed spellings of this attribute.
      // Callers should check both.
      return html_names::kAriaLabelledbyAttr;
    case AOMRelationListProperty::kOwns:
      return html_names::kAriaOwnsAttr;
  }

  NOTREACHED_IN_MIGRATION();
  return g_null_name;
}

QualifiedName GetCorrespondingARIAAttribute(AOMBooleanProperty property) {
  switch (property) {
    case AOMBooleanProperty::kAtomic:
      return html_names::kAriaAtomicAttr;
    case AOMBooleanProperty::kBusy:
      return html_names::kAriaBusyAttr;
    case AOMBooleanProperty::kDisabled:
      return html_names::kAriaDisabledAttr;
    case AOMBooleanProperty::kExpanded:
      return html_names::kAriaExpandedAttr;
    case AOMBooleanProperty::kHidden:
      return html_names::kAriaHiddenAttr;
    case AOMBooleanProperty::kModal:
      return html_names::kAriaModalAttr;
    case AOMBooleanProperty::kMultiline:
      return html_names::kAriaMultilineAttr;
    case AOMBooleanProperty::kMultiselectable:
      return html_names::kAriaMultiselectableAttr;
    case AOMBooleanProperty::kReadOnly:
      return html_names::kAriaReadonlyAttr;
    case AOMBooleanProperty::kRequired:
      return html_names::kAriaRequiredAttr;
    case AOMBooleanProperty::kSelected:
      return html_names::kAriaSelectedAttr;
  }

  NOTREACHED_IN_MIGRATION();
  return g_null_name;
}

QualifiedName GetCorrespondingARIAAttribute(AOMFloatProperty property) {
  AtomicString attr_value;
  switch (property) {
    case AOMFloatProperty::kValueMax:
      return html_names::kAriaValuemaxAttr;
    case AOMFloatProperty::kValueMin:
      return html_names::kAriaValueminAttr;
    case AOMFloatProperty::kValueNow:
      return html_names::kAriaValuenowAttr;
  }

  NOTREACHED_IN_MIGRATION();
  return g_null_name;
}

QualifiedName GetCorrespondingARIAAttribute(AOMUIntProperty property) {
  switch (property) {
    case AOMUIntProperty::kColIndex:
      return html_names::kAriaColindexAttr;
    case AOMUIntProperty::kColSpan:
      return html_names::kAriaColspanAttr;
    case AOMUIntProperty::kLevel:
      return html_names::kAriaLevelAttr;
    case AOMUIntProperty::kPosInSet:
      return html_names::kAriaPosinsetAttr;
    case AOMUIntProperty::kRowIndex:
      return html_names::kAriaRowindexAttr;
    case AOMUIntProperty::kRowSpan:
      return html_names::kAriaRowspanAttr;
  }

  NOTREACHED_IN_MIGRATION();
  return g_null_name;
}

QualifiedName GetCorrespondingARIAAttribute(AOMIntProperty property) {
  switch (property) {
    case AOMIntProperty::kColCount:
      return html_names::kAriaColcountAttr;
    case AOMIntProperty::kRowCount:
      return html_names::kAriaRowcountAttr;
    case AOMIntProperty::kSetSize:
      return html_names::kAriaSetsizeAttr;
  }

  NOTREACHED_IN_MIGRATION();
  return g_null_name;
}

}  // namespace

bool AccessibleNode::IsUndefinedAttrValue(const AtomicString& value) {
  return value.empty() || EqualIgnoringASCIICase(value, "undefined");
}

// static
const AtomicString& AccessibleNode::GetElementOrInternalsARIAAttribute(
    Element& element,
    const QualifiedName& attribute,
    bool is_token_attr) {
  const AtomicString& attr_value = element.FastGetAttribute(attribute);
  if ((attr_value != g_null_atom) &&
      (!is_token_attr || !IsUndefinedAttrValue(attr_value))) {
    return attr_value;
  }

  if (!element.DidAttachInternals())
    return g_null_atom;

  return element.EnsureElementInternals().FastGetAttribute(attribute);
}

// static
const AtomicString& AccessibleNode::GetPropertyOrARIAAttribute(
    Element* element,
    AOMStringProperty property) {
  if (!element)
    return g_null_atom;

  const bool is_token_attr = IsStringTokenProperty(property);

  // We are currently only checking ARIA attributes, instead of AccessibleNode
  // properties. Further refactoring will be happening as the API is finalised.
  QualifiedName attribute = GetCorrespondingARIAAttribute(property);
  const AtomicString& attr_value =
      GetElementOrInternalsARIAAttribute(*element, attribute, is_token_attr);
  if (is_token_attr && IsUndefinedAttrValue(attr_value))
    return g_null_atom;  // Attribute not set or explicitly undefined.

  return attr_value;
}

// static
const AtomicString& AccessibleNode::GetPropertyOrARIAAttributeValue(
    Element* element,
    AOMRelationProperty property) {
  if (!element)
    return g_null_atom;
  QualifiedName attribute = GetCorrespondingARIAAttribute(property);
  const AtomicString& value =
      GetElementOrInternalsARIAAttribute(*element, attribute);
  if (IsUndefinedAttrValue(value)) {
    return g_null_atom;  // Attribute not set or explicitly undefined.
  }

  return value;
}

Element* AccessibleNode::GetPropertyOrARIAAttribute(
    Element* element,
    AOMRelationProperty property) {
  auto& value = GetPropertyOrARIAAttributeValue(element, property);
  if (value == g_null_atom) {
    return nullptr;
  }
  Element* value_element = element->GetTreeScope().getElementById(value);
  if (!value_element) {
    return nullptr;
  }
  return value_element->GetShadowReferenceTargetOrSelf(
      GetCorrespondingARIAAttribute(property));
}

// static
bool AccessibleNode::GetPropertyOrARIAAttribute(
    Element* element,
    AOMRelationListProperty property,
    HeapVector<Member<Element>>& targets) {
  if (!element)
    return false;
  QualifiedName attribute = GetCorrespondingARIAAttribute(property);
  String value =
      GetElementOrInternalsARIAAttribute(*element, attribute).GetString();
  if (value.empty() && property == AOMRelationListProperty::kLabeledBy) {
    value = GetElementOrInternalsARIAAttribute(*element,
                                               html_names::kAriaLabeledbyAttr)
                .GetString();
  }
  if (value.empty())
    return false;

  Vector<String> ids;
  value.Split(' ', ids);
  if (ids.empty())
    return false;

  TreeScope& scope = element->GetTreeScope();
  for (const auto& id : ids) {
    if (Element* id_element = scope.getElementById(AtomicString(id))) {
      targets.push_back(id_element->GetShadowReferenceTargetOrSelf(attribute));
    }
  }
  return true;
}

// static
bool AccessibleNode::GetPropertyOrARIAAttribute(Element* element,
                                                AOMBooleanProperty property,
                                                bool& is_null) {
  is_null = true;
  if (!element)
    return false;

  // Fall back on the equivalent ARIA attribute.
  QualifiedName attribute = GetCorrespondingARIAAttribute(property);
  AtomicString attr_value =
      GetElementOrInternalsARIAAttribute(*element, attribute);
  is_null = IsUndefinedAttrValue(attr_value);
  return !is_null && !EqualIgnoringASCIICase(attr_value, "false");
}

// static
float AccessibleNode::GetPropertyOrARIAAttribute(Element* element,
                                                 AOMFloatProperty property,
                                                 bool& is_null) {
  is_null = true;
  if (!element)
    return 0.0;

  // Fall back on the equivalent ARIA attribute.
  QualifiedName attribute = GetCorrespondingARIAAttribute(property);
  AtomicString attr_value =
      GetElementOrInternalsARIAAttribute(*element, attribute);
  is_null = attr_value.IsNull();
  return attr_value.ToFloat();
}

// static
uint32_t AccessibleNode::GetPropertyOrARIAAttribute(Element* element,
                                                    AOMUIntProperty property,
                                                    bool& is_null) {
  is_null = true;
  if (!element)
    return 0;

  // Fall back on the equivalent ARIA attribute.
  QualifiedName attribute = GetCorrespondingARIAAttribute(property);
  AtomicString attr_value =
      GetElementOrInternalsARIAAttribute(*element, attribute);
  is_null = attr_value.IsNull();
  return attr_value.GetString().ToUInt();
}

// static
int32_t AccessibleNode::GetPropertyOrARIAAttribute(Element* element,
                                                   AOMIntProperty property,
                                                   bool& is_null) {
  is_null = true;
  if (!element)
    return 0;

  // Fall back on the equivalent ARIA attribute.
  QualifiedName attribute = GetCorrespondingARIAAttribute(property);
  AtomicString attr_value =
      GetElementOrInternalsARIAAttribute(*element, attribute);
  is_null = attr_value.IsNull();
  return attr_value.ToInt();
}

// These properties support a list of tokens, and "undefined"/"" is
// equivalent to not setting the attribute.
bool AccessibleNode::IsStringTokenProperty(AOMStringProperty property) {
  switch (property) {
    case AOMStringProperty::kAutocomplete:
    case AOMStringProperty::kChecked:
    case AOMStringProperty::kCurrent:
    case AOMStringProperty::kHasPopup:
    case AOMStringProperty::kInvalid:
    case AOMStringProperty::kLive:
    case AOMStringProperty::kOrientation:
    case AOMStringProperty::kPressed:
    case AOMStringProperty::kRelevant:
    case AOMStringProperty::kSort:
      return true;
    case AOMStringProperty::kAriaBrailleLabel:
    case AOMStringProperty::kAriaBrailleRoleDescription:
    case AOMStringProperty::kColIndexText:
    case AOMStringProperty::kDescription:
    case AOMStringProperty::kKeyShortcuts:
    case AOMStringProperty::kLabel:
    case AOMStringProperty::kPlaceholder:
    case AOMStringProperty::kRole:  // Is token, but ""/"undefined" not
                                    // supported.
    case AOMStringProperty::kRoleDescription:
    case AOMStringProperty::kRowIndexText:
    case AOMStringProperty::kValueText:
    case AOMStringProperty::kVirtualContent:
      break;
  }
  return false;
}

}  // namespace blink
