// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/ax_sparse_attribute_setter.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

void SetIntAttribute(ax::mojom::blink::IntAttribute attribute,
                     ui::AXNodeData* node_data,
                     const AtomicString& value) {
  node_data->AddIntAttribute(attribute, value.ToInt());
}

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

  QualifiedName GetAttributeQualifiedName() {
    switch (attribute_) {
      case AXObjectAttribute::kAriaActiveDescendant:
        return html_names::kAriaActivedescendantAttr;
      case AXObjectAttribute::kAriaErrorMessage:
        return html_names::kAriaErrormessageAttr;
      default:
        NOTREACHED();
    }
    return g_null_name;
  }

  void Run(const AXObject& obj,
           AXSparseAttributeClient& attribute_map,
           const AtomicString& value) override {
    if (value.IsNull())
      return;

    Element* element = obj.GetElement();
    if (!element)
      return;
    const QualifiedName& q_name = GetAttributeQualifiedName();
    Element* target = element->GetElementAttribute(q_name);
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

  QualifiedName GetAttributeQualifiedName() {
    switch (attribute_) {
      case AXObjectVectorAttribute::kAriaControls:
        return html_names::kAriaControlsAttr;
      case AXObjectVectorAttribute::kAriaDetails:
        return html_names::kAriaDetailsAttr;
      case AXObjectVectorAttribute::kAriaFlowTo:
        return html_names::kAriaFlowtoAttr;
      default:
        NOTREACHED();
    }
    return g_null_name;
  }

  void Run(const AXObject& obj,
           AXSparseAttributeClient& attribute_map,
           const AtomicString& value) override {
    Element* element = obj.GetElement();
    if (!element)
      return;

    base::Optional<HeapVector<Member<Element>>> attr_associated_elements =
        element->GetElementArrayAttribute(GetAttributeQualifiedName());
    if (!attr_associated_elements)
      return;
    HeapVector<Member<AXObject>>* objects =
        MakeGarbageCollected<HeapVector<Member<AXObject>>>();
    for (const auto& associated_element : attr_associated_elements.value()) {
      AXObject* ax_element =
          obj.AXObjectCache().GetOrCreate(associated_element);
      if (!ax_element)
        continue;
      if (AXObject* parent = ax_element->ParentObject())
        parent->UpdateChildrenIfNecessary();
      if (!ax_element->AccessibilityIsIgnored())
        objects->push_back(ax_element);
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
        new ObjectVectorAttributeSetter(AXObjectVectorAttribute::kAriaDetails));
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
  }
  return ax_sparse_attribute_setter_map;
}

// TODO(meredithl): move the rest of the sparse attributes to this map.
TempSetterMap& GetTempSetterMap(ui::AXNodeData* node_data) {
  DEFINE_STATIC_LOCAL(TempSetterMap, temp_setter_map, ());
  if (temp_setter_map.IsEmpty()) {
    temp_setter_map.Set(
        html_names::kAriaColcountAttr,
        WTF::BindRepeating(&SetIntAttribute,
                           ax::mojom::blink::IntAttribute::kAriaColumnCount));
    temp_setter_map.Set(
        html_names::kAriaColindexAttr,
        WTF::BindRepeating(
            &SetIntAttribute,
            ax::mojom::blink::IntAttribute::kAriaCellColumnIndex));
    temp_setter_map.Set(
        html_names::kAriaColspanAttr,
        WTF::BindRepeating(
            &SetIntAttribute,
            ax::mojom::blink::IntAttribute::kAriaCellColumnSpan));
    temp_setter_map.Set(
        html_names::kAriaRowcountAttr,
        WTF::BindRepeating(&SetIntAttribute,
                           ax::mojom::blink::IntAttribute::kAriaRowCount));
    temp_setter_map.Set(
        html_names::kAriaRowindexAttr,
        WTF::BindRepeating(&SetIntAttribute,
                           ax::mojom::blink::IntAttribute::kAriaCellRowIndex));
    temp_setter_map.Set(
        html_names::kAriaRowspanAttr,
        WTF::BindRepeating(&SetIntAttribute,
                           ax::mojom::blink::IntAttribute::kAriaCellRowSpan));
  }

  return temp_setter_map;
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
    case AOMRelationListProperty::kDetails:
      attribute = AXObjectVectorAttribute::kAriaDetails;
      break;
    case AOMRelationListProperty::kFlowTo:
      attribute = AXObjectVectorAttribute::kAriaFlowTo;
      break;
    default:
      return;
  }

  HeapVector<Member<AXObject>>* objects =
      MakeGarbageCollected<HeapVector<Member<AXObject>>>();
  for (unsigned i = 0; i < relations.length(); ++i) {
    AccessibleNode* accessible_node = relations.item(i);
    if (accessible_node) {
      Element* element = accessible_node->element();
      AXObject* ax_element = ax_object_cache_->GetOrCreate(element);
      if (ax_element && !ax_element->AccessibilityIsIgnored())
        objects->push_back(ax_element);
    }
  }

  sparse_attribute_client_.AddObjectVectorAttribute(attribute, objects);
}

}  // namespace blink
