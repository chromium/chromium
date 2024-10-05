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

}  // namespace

// static
const AtomicString& AccessibleNode::GetElementOrInternalsARIAAttribute(
    Element& element,
    const QualifiedName& attribute) {
  if (const AtomicString& attr_value = element.FastGetAttribute(attribute)) {
    return attr_value;
  }

  if (!element.DidAttachInternals())
    return g_null_atom;

  return element.EnsureElementInternals().FastGetAttribute(attribute);
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
  return value.empty() ? g_null_atom : value;
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

}  // namespace blink
