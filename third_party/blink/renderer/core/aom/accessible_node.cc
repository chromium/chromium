// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/aom/accessible_node.h"

#include "base/ranges/algorithm.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/aom/accessible_node_list.h"
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
    case AOMStringProperty::kSort:
      return html_names::kAriaSortAttr;
    case AOMStringProperty::kValueText:
      return html_names::kAriaValuetextAttr;
    case AOMStringProperty::kVirtualContent:
      return html_names::kAriaVirtualcontentAttr;
  }

  NOTREACHED();
  return g_null_name;
}

QualifiedName GetCorrespondingARIAAttribute(AOMRelationProperty property) {
  switch (property) {
    case AOMRelationProperty::kActiveDescendant:
      return html_names::kAriaActivedescendantAttr;
  }

  NOTREACHED();
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

  NOTREACHED();
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

  NOTREACHED();
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

  NOTREACHED();
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

  NOTREACHED();
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

  NOTREACHED();
  return g_null_name;
}

}  // namespace

AccessibleNode::AccessibleNode(Element* element)
    : element_(element), document_(nullptr) {
  DCHECK(RuntimeEnabledFeatures::AccessibilityObjectModelEnabled());
}

AccessibleNode::AccessibleNode(Document& document)
    : element_(nullptr), document_(document) {
  DCHECK(RuntimeEnabledFeatures::AccessibilityObjectModelEnabled());
}

AccessibleNode::~AccessibleNode() = default;

// static
AccessibleNode* AccessibleNode::Create(Document& document) {
  return MakeGarbageCollected<AccessibleNode>(document);
}

Document* AccessibleNode::GetDocument() const {
  if (document_) {
    DCHECK(!element_);
    return document_.Get();
  }
  if (element_) {
    DCHECK(!document_);
    return &element_->GetDocument();
  }

  return nullptr;
}

const AtomicString& AccessibleNode::GetProperty(
    AOMStringProperty property) const {
  for (const auto& item : string_properties_) {
    if (item.first == property && !item.second.IsNull())
      return item.second;
  }

  return g_null_atom;
}

// static
AccessibleNode* AccessibleNode::GetProperty(Element* element,
                                            AOMRelationProperty property) {
  if (!element)
    return nullptr;

  if (AccessibleNode* accessible_node = element->ExistingAccessibleNode()) {
    for (const auto& item : accessible_node->relation_properties_) {
      if (item.first == property && item.second)
        return item.second.Get();
    }
  }

  return nullptr;
}

// static
AccessibleNodeList* AccessibleNode::GetProperty(
    Element* element,
    AOMRelationListProperty property) {
  if (!element)
    return nullptr;

  if (AccessibleNode* accessible_node = element->ExistingAccessibleNode()) {
    for (const auto& item : accessible_node->relation_list_properties_) {
      if (item.first == property && item.second)
        return item.second.Get();
    }
  }

  return nullptr;
}

// static
bool AccessibleNode::GetProperty(Element* element,
                                 AOMRelationListProperty property,
                                 HeapVector<Member<Element>>& targets) {
  AccessibleNodeList* node_list = GetProperty(element, property);
  if (!node_list)
    return false;

  for (wtf_size_t i = 0; i < node_list->length(); ++i) {
    AccessibleNode* accessible_node = node_list->item(i);
    if (accessible_node) {
      if (Element* target_element = accessible_node->element())
        targets.push_back(target_element);
    }
  }

  return true;
}

template <typename P, typename T>
static absl::optional<T> FindPropertyValue(
    P property,
    const Vector<std::pair<P, T>>& properties) {
  for (const auto& item : properties) {
    if (item.first == property)
      return item.second;
  }
  return absl::nullopt;
}

absl::optional<bool> AccessibleNode::GetProperty(
    AOMBooleanProperty property) const {
  return FindPropertyValue(property, boolean_properties_);
}

// static
absl::optional<int32_t> AccessibleNode::GetProperty(Element* element,
                                                    AOMIntProperty property) {
  if (!element || !element->ExistingAccessibleNode())
    return absl::nullopt;
  return FindPropertyValue(property,
                           element->ExistingAccessibleNode()->int_properties_);
}

// static
absl::optional<uint32_t> AccessibleNode::GetProperty(Element* element,
                                                     AOMUIntProperty property) {
  if (!element || !element->ExistingAccessibleNode())
    return absl::nullopt;
  return FindPropertyValue(property,
                           element->ExistingAccessibleNode()->uint_properties_);
}

// static
absl::optional<float> AccessibleNode::GetProperty(Element* element,
                                                  AOMFloatProperty property) {
  if (!element || !element->ExistingAccessibleNode())
    return absl::nullopt;
  return FindPropertyValue(
      property, element->ExistingAccessibleNode()->float_properties_);
}

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
  return element->GetTreeScope().getElementById(value);
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
    if (Element* id_element = scope.getElementById(AtomicString(id)))
      targets.push_back(id_element);
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

void AccessibleNode::GetAllAOMProperties(AOMPropertyClient* client) {
  for (auto& item : string_properties_) {
    client->AddStringProperty(item.first, item.second);
  }
  for (auto& item : boolean_properties_) {
    client->AddBooleanProperty(item.first, item.second);
  }
  for (auto& item : float_properties_) {
    client->AddFloatProperty(item.first, item.second);
  }
  for (auto& item : relation_properties_) {
    if (!item.second)
      continue;
    client->AddRelationProperty(item.first, *item.second);
  }
  for (auto& item : relation_list_properties_) {
    if (!item.second)
      continue;
    client->AddRelationListProperty(item.first, *item.second);
  }
}

AccessibleNode* AccessibleNode::activeDescendant() const {
  return GetProperty(element_, AOMRelationProperty::kActiveDescendant);
}

void AccessibleNode::setActiveDescendant(AccessibleNode* active_descendant) {
  SetRelationProperty(AOMRelationProperty::kActiveDescendant,
                      active_descendant);
  NotifyAttributeChanged(html_names::kAriaActivedescendantAttr);
}

absl::optional<bool> AccessibleNode::atomic() const {
  return GetProperty(AOMBooleanProperty::kAtomic);
}

void AccessibleNode::setAtomic(absl::optional<bool> value) {
  SetBooleanProperty(AOMBooleanProperty::kAtomic, value);
  NotifyAttributeChanged(html_names::kAriaAtomicAttr);
}

AtomicString AccessibleNode::autocomplete() const {
  return GetProperty(AOMStringProperty::kAutocomplete);
}

void AccessibleNode::setAutocomplete(const AtomicString& autocomplete) {
  SetStringProperty(AOMStringProperty::kAutocomplete, autocomplete);
  NotifyAttributeChanged(html_names::kAriaAutocompleteAttr);
}

absl::optional<bool> AccessibleNode::busy() const {
  return GetProperty(AOMBooleanProperty::kBusy);
}

void AccessibleNode::setBusy(absl::optional<bool> value) {
  SetBooleanProperty(AOMBooleanProperty::kBusy, value);
  NotifyAttributeChanged(html_names::kAriaBusyAttr);
}

AtomicString AccessibleNode::brailleLabel() const {
  return GetProperty(AOMStringProperty::kAriaBrailleLabel);
}

void AccessibleNode::setBrailleLabel(const AtomicString& braille_label) {
  SetStringProperty(AOMStringProperty::kAriaBrailleLabel, braille_label);
  NotifyAttributeChanged(html_names::kAriaBraillelabelAttr);
}

AtomicString AccessibleNode::brailleRoleDescription() const {
  return GetProperty(AOMStringProperty::kAriaBrailleRoleDescription);
}

void AccessibleNode::setBrailleRoleDescription(
    const AtomicString& braille_role_description) {
  SetStringProperty(AOMStringProperty::kAriaBrailleRoleDescription,
                    braille_role_description);
  NotifyAttributeChanged(html_names::kAriaBrailleroledescriptionAttr);
}

AtomicString AccessibleNode::checked() const {
  return GetProperty(AOMStringProperty::kChecked);
}

void AccessibleNode::setChecked(const AtomicString& checked) {
  SetStringProperty(AOMStringProperty::kChecked, checked);
  NotifyAttributeChanged(html_names::kAriaCheckedAttr);
}

absl::optional<int32_t> AccessibleNode::colCount() const {
  return GetProperty(element_, AOMIntProperty::kColCount);
}

void AccessibleNode::setColCount(absl::optional<int32_t> value) {
  SetIntProperty(AOMIntProperty::kColCount, value);
  NotifyAttributeChanged(html_names::kAriaColcountAttr);
}

absl::optional<uint32_t> AccessibleNode::colIndex() const {
  return GetProperty(element_, AOMUIntProperty::kColIndex);
}

void AccessibleNode::setColIndex(absl::optional<uint32_t> value) {
  SetUIntProperty(AOMUIntProperty::kColIndex, value);
  NotifyAttributeChanged(html_names::kAriaColindexAttr);
}

absl::optional<uint32_t> AccessibleNode::colSpan() const {
  return GetProperty(element_, AOMUIntProperty::kColSpan);
}

void AccessibleNode::setColSpan(absl::optional<uint32_t> value) {
  SetUIntProperty(AOMUIntProperty::kColSpan, value);
  NotifyAttributeChanged(html_names::kAriaColspanAttr);
}

AccessibleNodeList* AccessibleNode::controls() const {
  return GetProperty(element_, AOMRelationListProperty::kControls);
}

void AccessibleNode::setControls(AccessibleNodeList* controls) {
  SetRelationListProperty(AOMRelationListProperty::kControls, controls);
  NotifyAttributeChanged(html_names::kAriaControlsAttr);
}

AtomicString AccessibleNode::current() const {
  return GetProperty(AOMStringProperty::kCurrent);
}

void AccessibleNode::setCurrent(const AtomicString& current) {
  SetStringProperty(AOMStringProperty::kCurrent, current);
  NotifyAttributeChanged(html_names::kAriaCurrentAttr);
}

AccessibleNodeList* AccessibleNode::describedBy() {
  return GetProperty(element_, AOMRelationListProperty::kDescribedBy);
}

void AccessibleNode::setDescribedBy(AccessibleNodeList* described_by) {
  SetRelationListProperty(AOMRelationListProperty::kDescribedBy, described_by);
  NotifyAttributeChanged(html_names::kAriaDescribedbyAttr);
}

AtomicString AccessibleNode::description() const {
  return GetProperty(AOMStringProperty::kDescription);
}

void AccessibleNode::setDescription(const AtomicString& description) {
  SetStringProperty(AOMStringProperty::kDescription, description);
  NotifyAttributeChanged(html_names::kAriaDescriptionAttr);
}

AccessibleNodeList* AccessibleNode::details() const {
  return GetProperty(element_, AOMRelationListProperty::kDetails);
}

void AccessibleNode::setDetails(AccessibleNodeList* details) {
  SetRelationListProperty(AOMRelationListProperty::kDetails, details);
  NotifyAttributeChanged(html_names::kAriaDetailsAttr);
}

absl::optional<bool> AccessibleNode::disabled() const {
  return GetProperty(AOMBooleanProperty::kDisabled);
}

void AccessibleNode::setDisabled(absl::optional<bool> value) {
  SetBooleanProperty(AOMBooleanProperty::kDisabled, value);
  NotifyAttributeChanged(html_names::kAriaDisabledAttr);
}

AccessibleNodeList* AccessibleNode::errorMessage() const {
  return GetProperty(element_, AOMRelationListProperty::kErrorMessage);
}

void AccessibleNode::setErrorMessage(AccessibleNodeList* error_messages) {
  SetRelationListProperty(AOMRelationListProperty::kErrorMessage,
                          error_messages);
  NotifyAttributeChanged(html_names::kAriaErrormessageAttr);
}

absl::optional<bool> AccessibleNode::expanded() const {
  return GetProperty(AOMBooleanProperty::kExpanded);
}

void AccessibleNode::setExpanded(absl::optional<bool> value) {
  SetBooleanProperty(AOMBooleanProperty::kExpanded, value);
  NotifyAttributeChanged(html_names::kAriaExpandedAttr);
}

AccessibleNodeList* AccessibleNode::flowTo() const {
  return GetProperty(element_, AOMRelationListProperty::kFlowTo);
}

void AccessibleNode::setFlowTo(AccessibleNodeList* flow_to) {
  SetRelationListProperty(AOMRelationListProperty::kFlowTo, flow_to);
  NotifyAttributeChanged(html_names::kAriaFlowtoAttr);
}

AtomicString AccessibleNode::hasPopup() const {
  return GetProperty(AOMStringProperty::kHasPopup);
}

void AccessibleNode::setHasPopup(const AtomicString& has_popup) {
  SetStringProperty(AOMStringProperty::kHasPopup, has_popup);
  NotifyAttributeChanged(html_names::kAriaHaspopupAttr);
}

absl::optional<bool> AccessibleNode::hidden() const {
  return GetProperty(AOMBooleanProperty::kHidden);
}

void AccessibleNode::setHidden(absl::optional<bool> value) {
  SetBooleanProperty(AOMBooleanProperty::kHidden, value);
  NotifyAttributeChanged(html_names::kAriaHiddenAttr);
}

AtomicString AccessibleNode::invalid() const {
  return GetProperty(AOMStringProperty::kInvalid);
}

void AccessibleNode::setInvalid(const AtomicString& invalid) {
  SetStringProperty(AOMStringProperty::kInvalid, invalid);
  NotifyAttributeChanged(html_names::kAriaInvalidAttr);
}

AtomicString AccessibleNode::keyShortcuts() const {
  return GetProperty(AOMStringProperty::kKeyShortcuts);
}

void AccessibleNode::setKeyShortcuts(const AtomicString& key_shortcuts) {
  SetStringProperty(AOMStringProperty::kKeyShortcuts, key_shortcuts);
  NotifyAttributeChanged(html_names::kAriaKeyshortcutsAttr);
}

AtomicString AccessibleNode::label() const {
  return GetProperty(AOMStringProperty::kLabel);
}

void AccessibleNode::setLabel(const AtomicString& label) {
  SetStringProperty(AOMStringProperty::kLabel, label);
  NotifyAttributeChanged(html_names::kAriaLabelAttr);
}

AccessibleNodeList* AccessibleNode::labeledBy() {
  return GetProperty(element_, AOMRelationListProperty::kLabeledBy);
}

void AccessibleNode::setLabeledBy(AccessibleNodeList* labeled_by) {
  SetRelationListProperty(AOMRelationListProperty::kLabeledBy, labeled_by);
  NotifyAttributeChanged(html_names::kAriaLabelledbyAttr);
}

absl::optional<uint32_t> AccessibleNode::level() const {
  return GetProperty(element_, AOMUIntProperty::kLevel);
}

void AccessibleNode::setLevel(absl::optional<uint32_t> value) {
  SetUIntProperty(AOMUIntProperty::kLevel, value);
  NotifyAttributeChanged(html_names::kAriaLevelAttr);
}

AtomicString AccessibleNode::live() const {
  return GetProperty(AOMStringProperty::kLive);
}

void AccessibleNode::setLive(const AtomicString& live) {
  SetStringProperty(AOMStringProperty::kLive, live);
  NotifyAttributeChanged(html_names::kAriaLiveAttr);
}

absl::optional<bool> AccessibleNode::modal() const {
  return GetProperty(AOMBooleanProperty::kModal);
}

void AccessibleNode::setModal(absl::optional<bool> value) {
  SetBooleanProperty(AOMBooleanProperty::kModal, value);
  NotifyAttributeChanged(html_names::kAriaModalAttr);
}

absl::optional<bool> AccessibleNode::multiline() const {
  return GetProperty(AOMBooleanProperty::kMultiline);
}

void AccessibleNode::setMultiline(absl::optional<bool> value) {
  SetBooleanProperty(AOMBooleanProperty::kMultiline, value);
  NotifyAttributeChanged(html_names::kAriaMultilineAttr);
}

absl::optional<bool> AccessibleNode::multiselectable() const {
  return GetProperty(AOMBooleanProperty::kMultiselectable);
}

void AccessibleNode::setMultiselectable(absl::optional<bool> value) {
  SetBooleanProperty(AOMBooleanProperty::kMultiselectable, value);
  NotifyAttributeChanged(html_names::kAriaMultiselectableAttr);
}

AtomicString AccessibleNode::orientation() const {
  return GetProperty(AOMStringProperty::kOrientation);
}

void AccessibleNode::setOrientation(const AtomicString& orientation) {
  SetStringProperty(AOMStringProperty::kOrientation, orientation);
  NotifyAttributeChanged(html_names::kAriaOrientationAttr);
}

AccessibleNodeList* AccessibleNode::owns() const {
  return GetProperty(element_, AOMRelationListProperty::kOwns);
}

void AccessibleNode::setOwns(AccessibleNodeList* owns) {
  SetRelationListProperty(AOMRelationListProperty::kOwns, owns);
  NotifyAttributeChanged(html_names::kAriaOwnsAttr);
}

AtomicString AccessibleNode::placeholder() const {
  return GetProperty(AOMStringProperty::kPlaceholder);
}

void AccessibleNode::setPlaceholder(const AtomicString& placeholder) {
  SetStringProperty(AOMStringProperty::kPlaceholder, placeholder);
  NotifyAttributeChanged(html_names::kAriaPlaceholderAttr);
}

absl::optional<uint32_t> AccessibleNode::posInSet() const {
  return GetProperty(element_, AOMUIntProperty::kPosInSet);
}

void AccessibleNode::setPosInSet(absl::optional<uint32_t> value) {
  SetUIntProperty(AOMUIntProperty::kPosInSet, value);
  NotifyAttributeChanged(html_names::kAriaPosinsetAttr);
}

AtomicString AccessibleNode::pressed() const {
  return GetProperty(AOMStringProperty::kPressed);
}

void AccessibleNode::setPressed(const AtomicString& pressed) {
  SetStringProperty(AOMStringProperty::kPressed, pressed);
  NotifyAttributeChanged(html_names::kAriaPressedAttr);
}

absl::optional<bool> AccessibleNode::readOnly() const {
  return GetProperty(AOMBooleanProperty::kReadOnly);
}

void AccessibleNode::setReadOnly(absl::optional<bool> value) {
  SetBooleanProperty(AOMBooleanProperty::kReadOnly, value);
  NotifyAttributeChanged(html_names::kAriaReadonlyAttr);
}

AtomicString AccessibleNode::relevant() const {
  return GetProperty(AOMStringProperty::kRelevant);
}

void AccessibleNode::setRelevant(const AtomicString& relevant) {
  SetStringProperty(AOMStringProperty::kRelevant, relevant);
  NotifyAttributeChanged(html_names::kAriaRelevantAttr);
}

absl::optional<bool> AccessibleNode::required() const {
  return GetProperty(AOMBooleanProperty::kRequired);
}

void AccessibleNode::setRequired(absl::optional<bool> value) {
  SetBooleanProperty(AOMBooleanProperty::kRequired, value);
  NotifyAttributeChanged(html_names::kAriaRequiredAttr);
}

AtomicString AccessibleNode::role() const {
  return GetProperty(AOMStringProperty::kRole);
}

void AccessibleNode::setRole(const AtomicString& role) {
  SetStringProperty(AOMStringProperty::kRole, role);
  NotifyAttributeChanged(html_names::kRoleAttr);
}

AtomicString AccessibleNode::roleDescription() const {
  return GetProperty(AOMStringProperty::kRoleDescription);
}

void AccessibleNode::setRoleDescription(const AtomicString& role_description) {
  SetStringProperty(AOMStringProperty::kRoleDescription, role_description);
  NotifyAttributeChanged(html_names::kAriaRoledescriptionAttr);
}

absl::optional<int32_t> AccessibleNode::rowCount() const {
  return GetProperty(element_, AOMIntProperty::kRowCount);
}

void AccessibleNode::setRowCount(absl::optional<int32_t> value) {
  SetIntProperty(AOMIntProperty::kRowCount, value);
  NotifyAttributeChanged(html_names::kAriaRowcountAttr);
}

absl::optional<uint32_t> AccessibleNode::rowIndex() const {
  return GetProperty(element_, AOMUIntProperty::kRowIndex);
}

void AccessibleNode::setRowIndex(absl::optional<uint32_t> value) {
  SetUIntProperty(AOMUIntProperty::kRowIndex, value);
  NotifyAttributeChanged(html_names::kAriaRowindexAttr);
}

absl::optional<uint32_t> AccessibleNode::rowSpan() const {
  return GetProperty(element_, AOMUIntProperty::kRowSpan);
}

void AccessibleNode::setRowSpan(absl::optional<uint32_t> value) {
  SetUIntProperty(AOMUIntProperty::kRowSpan, value);
  NotifyAttributeChanged(html_names::kAriaRowspanAttr);
}

absl::optional<bool> AccessibleNode::selected() const {
  return GetProperty(AOMBooleanProperty::kSelected);
}

void AccessibleNode::setSelected(absl::optional<bool> value) {
  SetBooleanProperty(AOMBooleanProperty::kSelected, value);
  NotifyAttributeChanged(html_names::kAriaSelectedAttr);
}

absl::optional<int32_t> AccessibleNode::setSize() const {
  return GetProperty(element_, AOMIntProperty::kSetSize);
}

void AccessibleNode::setSetSize(absl::optional<int32_t> value) {
  SetIntProperty(AOMIntProperty::kSetSize, value);
  NotifyAttributeChanged(html_names::kAriaSetsizeAttr);
}

AtomicString AccessibleNode::sort() const {
  return GetProperty(AOMStringProperty::kSort);
}

void AccessibleNode::setSort(const AtomicString& sort) {
  SetStringProperty(AOMStringProperty::kSort, sort);
  NotifyAttributeChanged(html_names::kAriaSortAttr);
}

absl::optional<float> AccessibleNode::valueMax() const {
  return GetProperty(element_, AOMFloatProperty::kValueMax);
}

void AccessibleNode::setValueMax(absl::optional<float> value) {
  SetFloatProperty(AOMFloatProperty::kValueMax, value);
  NotifyAttributeChanged(html_names::kAriaValuemaxAttr);
}

absl::optional<float> AccessibleNode::valueMin() const {
  return GetProperty(element_, AOMFloatProperty::kValueMin);
}

void AccessibleNode::setValueMin(absl::optional<float> value) {
  SetFloatProperty(AOMFloatProperty::kValueMin, value);
  NotifyAttributeChanged(html_names::kAriaValueminAttr);
}

absl::optional<float> AccessibleNode::valueNow() const {
  return GetProperty(element_, AOMFloatProperty::kValueNow);
}

void AccessibleNode::setValueNow(absl::optional<float> value) {
  SetFloatProperty(AOMFloatProperty::kValueNow, value);
  NotifyAttributeChanged(html_names::kAriaValuenowAttr);
}

AtomicString AccessibleNode::valueText() const {
  return GetProperty(AOMStringProperty::kValueText);
}

void AccessibleNode::setValueText(const AtomicString& value_text) {
  SetStringProperty(AOMStringProperty::kValueText, value_text);
  NotifyAttributeChanged(html_names::kAriaValuetextAttr);
}

AtomicString AccessibleNode::virtualContent() const {
  return GetProperty(AOMStringProperty::kVirtualContent);
}

void AccessibleNode::setVirtualContent(const AtomicString& virtual_content) {
  SetStringProperty(AOMStringProperty::kVirtualContent, virtual_content);
  NotifyAttributeChanged(html_names::kAriaVirtualcontentAttr);
}

AccessibleNodeList* AccessibleNode::childNodes() {
  return AccessibleNodeList::Create(children_);
}

void AccessibleNode::appendChild(AccessibleNode* child,
                                 ExceptionState& exception_state) {
  if (child == this) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "An AccessibleNode cannot be a child of itself");
    return;
  }

  if (child->element()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "An AccessibleNode associated with an Element cannot be a child.");
    return;
  }

  if (child->parent_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Reparenting is not supported yet.");
    return;
  }
  child->document_ = GetAncestorDocument();
  if (!child->document_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "AccessibleNode must have an ancestor that is attached to a document.");
    return;
  }
  child->parent_ = this;

  if (!GetExecutionContext()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "Trying to access an AccessibleNode in a detached window.");
    return;
  }

  if (!GetExecutionContext()->GetSecurityOrigin()->CanAccess(
          child->GetExecutionContext()->GetSecurityOrigin())) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "Trying to access an AccessibleNode from a different origin.");
    return;
  }

  children_.push_back(child);
  if (AXObjectCache* cache = GetAXObjectCache())
    cache->ChildrenChanged(this);
}

void AccessibleNode::DetachedFromDocument() {
  // Clear associated AXObject from AXObjectCache since its accessible node is
  // removed from document.
  if (AXObjectCache* cache = GetAXObjectCache())
    cache->Remove(this);

  // Clear reference to its document, since this accessible node is removed from
  // document.
  document_ = nullptr;

  // Remove references for subtree.
  for (auto child : GetChildren()) {
    child->DetachedFromDocument();
  }
}

void AccessibleNode::removeChild(AccessibleNode* old_child,
                                 ExceptionState& exception_state) {
  if (old_child->parent_ != this) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "Node to remove is not a child of this node.");
    return;
  }
  auto* ix =
      base::ranges::find(children_, old_child, &Member<AccessibleNode>::Get);
  if (ix == children_.end()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "Node to remove is not a child of this node.");
    return;
  }
  old_child->parent_ = nullptr;
  old_child->DetachedFromDocument();
  children_.erase(ix);

  if (AXObjectCache* cache = GetAXObjectCache())
    cache->ChildrenChanged(this);
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
    case AOMStringProperty::kDescription:
    case AOMStringProperty::kKeyShortcuts:
    case AOMStringProperty::kLabel:
    case AOMStringProperty::kPlaceholder:
    case AOMStringProperty::kRole:  // Is token, but ""/"undefined" not
                                    // supported.
    case AOMStringProperty::kRoleDescription:
    case AOMStringProperty::kValueText:
    case AOMStringProperty::kVirtualContent:
      break;
  }
  return false;
}

const AtomicString& AccessibleNode::InterfaceName() const {
  return event_target_names::kAccessibleNode;
}

ExecutionContext* AccessibleNode::GetExecutionContext() const {
  if (element_)
    return element_->GetExecutionContext();
  if (document_)
    return document_->GetExecutionContext();

  if (parent_)
    return parent_->GetExecutionContext();

  return nullptr;
}

Document* AccessibleNode::GetAncestorDocument() {
  if (element_)
    return &(element_->GetDocument());

  if (parent_)
    return parent_->GetAncestorDocument();

  return nullptr;
}

void AccessibleNode::SetStringProperty(AOMStringProperty property,
                                       const AtomicString& value) {
  for (auto& item : string_properties_) {
    if (item.first == property) {
      item.second = value;
      return;
    }
  }

  string_properties_.push_back(std::make_pair(property, value));
}

void AccessibleNode::SetRelationProperty(AOMRelationProperty property,
                                         AccessibleNode* value) {
  for (auto& item : relation_properties_) {
    if (item.first == property) {
      item.second = value;
      return;
    }
  }

  relation_properties_.push_back(std::make_pair(property, value));
}

void AccessibleNode::SetRelationListProperty(AOMRelationListProperty property,
                                             AccessibleNodeList* value) {
  for (auto& item : relation_list_properties_) {
    if (item.first == property) {
      if (item.second)
        item.second->RemoveOwner(property, this);
      if (value)
        value->AddOwner(property, this);
      item.second = value;
      return;
    }
  }

  relation_list_properties_.push_back(std::make_pair(property, value));
}

template <typename P, typename T>
static void SetProperty(P property,
                        absl::optional<T> value,
                        Vector<std::pair<P, T>>& properties) {
  for (wtf_size_t i = 0; i < properties.size(); i++) {
    auto& item = properties[i];
    if (item.first == property) {
      if (value.has_value())
        item.second = value.value();
      else
        properties.EraseAt(i);
      return;
    }
  }

  if (value.has_value())
    properties.push_back(std::make_pair(property, value.value()));
}

void AccessibleNode::SetBooleanProperty(AOMBooleanProperty property,
                                        absl::optional<bool> value) {
  SetProperty(property, value, boolean_properties_);
}

void AccessibleNode::SetIntProperty(AOMIntProperty property,
                                    absl::optional<int32_t> value) {
  SetProperty(property, value, int_properties_);
}

void AccessibleNode::SetUIntProperty(AOMUIntProperty property,
                                     absl::optional<uint32_t> value) {
  SetProperty(property, value, uint_properties_);
}

void AccessibleNode::SetFloatProperty(AOMFloatProperty property,
                                      absl::optional<float> value) {
  SetProperty(property, value, float_properties_);
}

void AccessibleNode::OnRelationListChanged(AOMRelationListProperty property) {
  NotifyAttributeChanged(GetCorrespondingARIAAttribute(property));
}

void AccessibleNode::NotifyAttributeChanged(
    const blink::QualifiedName& attribute) {
  // TODO(dmazzoni): Make a cleaner API for this rather than pretending
  // the DOM attribute changed.
  AXObjectCache* cache = GetAXObjectCache();
  if (!cache)
    return;

  if (!element_) {
    cache->HandleAttributeChanged(attribute, this);
    return;
  }

  // By definition, any attribute on an AccessibleNode is interesting to
  // AXObjectCache, so no need to check return value.
  cache->HandleAttributeChanged(attribute, element_);
}

AXObjectCache* AccessibleNode::GetAXObjectCache() {
  if (Document* document = GetDocument())
    return document->ExistingAXObjectCache();

  return nullptr;
}

void AccessibleNode::Trace(Visitor* visitor) const {
  visitor->Trace(element_);
  visitor->Trace(document_);
  visitor->Trace(relation_properties_);
  visitor->Trace(relation_list_properties_);
  visitor->Trace(children_);
  visitor->Trace(parent_);
  EventTarget::Trace(visitor);
  ElementRareDataField::Trace(visitor);
}

}  // namespace blink
