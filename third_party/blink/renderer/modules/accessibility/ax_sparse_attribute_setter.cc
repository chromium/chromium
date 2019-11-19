// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/ax_sparse_attribute_setter.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"

namespace blink {

class BoolAttributeSetter : public AXSparseAttributeSetter {
 public:
  BoolAttributeSetter(AXBoolAttribute attribute) : attribute_(attribute) {}

 private:
  AXBoolAttribute attribute_;

  void Run(const AXObject& obj,
           AXSparseAttributeClient& attribute_map,
           const AtomicString& value) override {
    // ARIA booleans are true if not "false" and not specifically undefined.
    bool is_true = !AccessibleNode::IsUndefinedAttrValue(value) &&
                   !EqualIgnoringASCIICase(value, "false");
    if (is_true)  // Not necessary to add if false
      attribute_map.AddBoolAttribute(attribute_, true);
  }
};

class IntAttributeSetter : public AXSparseAttributeSetter {
 public:
  IntAttributeSetter(AXIntAttribute attribute) : attribute_(attribute) {}

 private:
  AXIntAttribute attribute_;

  void Run(const AXObject& obj,
           AXSparseAttributeClient& attribute_map,
           const AtomicString& value) override {
    attribute_map.AddIntAttribute(attribute_, value.ToInt());
  }
};

class UIntAttributeSetter : public AXSparseAttributeSetter {
 public:
  UIntAttributeSetter(AXUIntAttribute attribute) : attribute_(attribute) {}

 private:
  AXUIntAttribute attribute_;

  void Run(const AXObject& obj,
           AXSparseAttributeClient& attribute_map,
           const AtomicString& value) override {
    attribute_map.AddUIntAttribute(attribute_, value.ToInt());
  }
};

class StringAttributeSetter : public AXSparseAttributeSetter {
 public:
  StringAttributeSetter(AXStringAttribute attribute) : attribute_(attribute) {}

 private:
  AXStringAttribute attribute_;

  void Run(const AXObject& obj,
           AXSparseAttributeClient& attribute_map,
           const AtomicString& value) override {
    attribute_map.AddStringAttribute(attribute_, value);
  }
};

class ObjectAttributeSetter : public AXSparseAttributeSetter {
 public:
  ObjectAttributeSetter(AXObjectAttribute attribute) : attribute_(attribute) {}

 private:
  AXObjectAttribute attribute_;

  void Run(const AXObject& obj,
           AXSparseAttributeClient& attribute_map,
           const AtomicString& value) override {
    if (value.IsNull() || value.IsEmpty())
      return;

    auto* element = DynamicTo<Element>(obj.GetNode());
    if (!element)
      return;
    Element* target = element->GetTreeScope().getElementById(value);
    if (!target)
      return;
    AXObject* ax_target = obj.AXObjectCache().GetOrCreate(target);
    if (ax_target)
      attribute_map.AddObjectAttribute(attribute_, *ax_target);
  }
};

class ObjectVectorAttributeSetter : public AXSparseAttributeSetter {
 public:
  ObjectVectorAttributeSetter(AXObjectVectorAttribute attribute)
      : attribute_(attribute) {}

 private:
  AXObjectVectorAttribute attribute_;

  void Run(const AXObject& obj,
           AXSparseAttributeClient& attribute_map,
           const AtomicString& value) override {
    Node* node = obj.GetNode();
    if (!node || !node->IsElementNode())
      return;

    String attribute_value = value.GetString();
    if (attribute_value.IsEmpty())
      return;

    Vector<String> ids;
    attribute_value.Split(' ', ids);
    if (ids.IsEmpty())
      return;

    HeapVector<Member<AXObject>> objects;
    TreeScope& scope = node->GetTreeScope();
    for (const auto& id : ids) {
      if (Element* id_element = scope.getElementById(AtomicString(id))) {
        AXObject* ax_id_element = obj.AXObjectCache().GetOrCreate(id_element);
        if (!ax_id_element)
          continue;
        if (AXObject* parent = ax_id_element->ParentObject())
          parent->UpdateChildrenIfNecessary();
        if (!ax_id_element->AccessibilityIsIgnored())
          objects.push_back(ax_id_element);
      }
    }

    attribute_map.AddObjectVectorAttribute(attribute_, objects);
  }
};

AXSparseAttributeSetterMap& GetSparseAttributeSetterMap() {
  // Use a map from attribute name to properties of that attribute.
  // That way we only need to iterate over the list of attributes once,
  // rather than calling getAttribute() once for each possible obscure
  // accessibility attribute.
  DEFINE_STATIC_LOCAL(AXSparseAttributeSetterMap,
                      ax_sparse_attribute_setter_map, ());
  if (ax_sparse_attribute_setter_map.IsEmpty()) {
    ax_sparse_attribute_setter_map.Set(
        html_names::kAriaActivedescendantAttr,
        new ObjectAttributeSetter(AXObjectAttribute::kAriaActiveDescendant));
    ax_sparse_attribute_setter_map.Set(
        html_names::kAriaControlsAttr,
        new ObjectVectorAttributeSetter(
            AXObjectVectorAttribute::kAriaControls));
    ax_sparse_attribute_setter_map.Set(
        html_names::kAriaFlowtoAttr,
        new ObjectVectorAttributeSetter(AXObjectVectorAttribute::kAriaFlowTo));
    ax_sparse_attribute_setter_map.Set(
        html_names::kAriaDetailsAttr,
        new ObjectAttributeSetter(AXObjectAttribute::kAriaDetails));
    ax_sparse_attribute_setter_map.Set(
        html_names::kAriaErrormessageAttr,
        new ObjectAttributeSetter(AXObjectAttribute::kAriaErrorMessage));
    ax_sparse_attribute_setter_map.Set(
        html_names::kAriaKeyshortcutsAttr,
        new StringAttributeSetter(AXStringAttribute::kAriaKeyShortcuts));
    ax_sparse_attribute_setter_map.Set(
        html_names::kAriaRoledescriptionAttr,
        new StringAttributeSetter(AXStringAttribute::kAriaRoleDescription));
    ax_sparse_attribute_setter_map.Set(
        html_names::kAriaBusyAttr,
        new BoolAttributeSetter(AXBoolAttribute::kAriaBusy));
    ax_sparse_attribute_setter_map.Set(
        html_names::kAriaColcountAttr,
        new IntAttributeSetter(AXIntAttribute::kAriaColumnCount));
    ax_sparse_attribute_setter_map.Set(
        html_names::kAriaColindexAttr,
        new UIntAttributeSetter(AXUIntAttribute::kAriaColumnIndex));
    ax_sparse_attribute_setter_map.Set(
        html_names::kAriaColspanAttr,
        new UIntAttributeSetter(AXUIntAttribute::kAriaColumnSpan));
    ax_sparse_attribute_setter_map.Set(
        html_names::kAriaRowcountAttr,
        new IntAttributeSetter(AXIntAttribute::kAriaRowCount));
    ax_sparse_attribute_setter_map.Set(
        html_names::kAriaRowindexAttr,
        new UIntAttributeSetter(AXUIntAttribute::kAriaRowIndex));
    ax_sparse_attribute_setter_map.Set(
        html_names::kAriaRowspanAttr,
        new UIntAttributeSetter(AXUIntAttribute::kAriaRowSpan));
  }
  return ax_sparse_attribute_setter_map;
}

void AXSparseAttributeAOMPropertyClient::AddStringProperty(
    AOMStringProperty property,
    const String& value) {
  AXStringAttribute attribute;
  switch (property) {
    case AOMStringProperty::kKeyShortcuts:
      attribute = AXStringAttribute::kAriaKeyShortcuts;
      break;
    case AOMStringProperty::kRoleDescription:
      attribute = AXStringAttribute::kAriaRoleDescription;
      break;
    default:
      return;
  }
  sparse_attribute_client_.AddStringAttribute(attribute, value);
}

void AXSparseAttributeAOMPropertyClient::AddBooleanProperty(
    AOMBooleanProperty property,
    bool value) {
  AXBoolAttribute attribute;
  switch (property) {
    case AOMBooleanProperty::kBusy:
      attribute = AXBoolAttribute::kAriaBusy;
      break;
    default:
      return;
  }
  sparse_attribute_client_.AddBoolAttribute(attribute, value);
}

void AXSparseAttributeAOMPropertyClient::AddIntProperty(AOMIntProperty property,
                                                        int32_t value) {}

void AXSparseAttributeAOMPropertyClient::AddUIntProperty(
    AOMUIntProperty property,
    uint32_t value) {}

void AXSparseAttributeAOMPropertyClient::AddFloatProperty(
    AOMFloatProperty property,
    float value) {}

void AXSparseAttributeAOMPropertyClient::AddRelationProperty(
    AOMRelationProperty property,
    const AccessibleNode& value) {
  AXObjectAttribute attribute;
  switch (property) {
    case AOMRelationProperty::kActiveDescendant:
      attribute = AXObjectAttribute::kAriaActiveDescendant;
      break;
    case AOMRelationProperty::kDetails:
      attribute = AXObjectAttribute::kAriaDetails;
      break;
    case AOMRelationProperty::kErrorMessage:
      attribute = AXObjectAttribute::kAriaErrorMessage;
      break;
    default:
      return;
  }

  Element* target_element = value.element();
  AXObject* target_obj = ax_object_cache_->GetOrCreate(target_element);
  if (target_element)
    sparse_attribute_client_.AddObjectAttribute(attribute, *target_obj);
}

void AXSparseAttributeAOMPropertyClient::AddRelationListProperty(
    AOMRelationListProperty property,
    const AccessibleNodeList& relations) {
  AXObjectVectorAttribute attribute;
  switch (property) {
    case AOMRelationListProperty::kControls:
      attribute = AXObjectVectorAttribute::kAriaControls;
      break;
    case AOMRelationListProperty::kFlowTo:
      attribute = AXObjectVectorAttribute::kAriaFlowTo;
      break;
    default:
      return;
  }

  HeapVector<Member<AXObject>> objects;
  for (unsigned i = 0; i < relations.length(); ++i) {
    AccessibleNode* accessible_node = relations.item(i);
    if (accessible_node) {
      Element* element = accessible_node->element();
      AXObject* ax_element = ax_object_cache_->GetOrCreate(element);
      if (ax_element && !ax_element->AccessibilityIsIgnored())
        objects.push_back(ax_element);
    }
  }

  sparse_attribute_client_.AddObjectVectorAttribute(attribute, objects);
}

}  // namespace blink
