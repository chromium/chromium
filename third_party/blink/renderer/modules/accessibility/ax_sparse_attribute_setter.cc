// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/ax_sparse_attribute_setter.h"

#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

void SetIntAttribute(ax::mojom::blink::IntAttribute attribute,
                     AXObject* object,
                     ui::AXNodeData* node_data,
                     const AtomicString& value) {
  node_data->AddIntAttribute(attribute, value.ToInt());
}

void SetBoolAttribute(ax::mojom::blink::BoolAttribute attribute,
                      AXObject* object,
                      ui::AXNodeData* node_data,
                      const AtomicString& value) {
  // Don't set kTouchPassthrough unless the feature is enabled in this
  // context.
  if (attribute == ax::mojom::blink::BoolAttribute::kTouchPassthrough) {
    auto* context = object->AXObjectCache().GetDocument().GetExecutionContext();
    if (RuntimeEnabledFeatures::AccessibilityAriaTouchPassthroughEnabled(
            context)) {
      UseCounter::Count(context, WebFeature::kAccessibilityTouchPassthroughSet);
    } else {
      return;
    }
  }

  // ARIA booleans are true if not "false" and not specifically undefined.
  bool is_true = !AccessibleNode::IsUndefinedAttrValue(value) &&
                 !EqualIgnoringASCIICase(value, "false");
  if (is_true)  // Not necessary to add if false
    node_data->AddBoolAttribute(attribute, true);
}

void SetStringAttribute(ax::mojom::blink::StringAttribute attribute,
                        AXObject* object,
                        ui::AXNodeData* node_data,
                        const AtomicString& value) {
  if (object->IsProhibited(attribute))
    return;
  node_data->AddStringAttribute(attribute, value.Utf8());
}

void SetNotEmptyStringAttribute(ax::mojom::blink::StringAttribute attribute,
                                AXObject* object,
                                ui::AXNodeData* node_data,
                                const AtomicString& value) {
  if (value.length() != 0)
    SetStringAttribute(attribute, object, node_data, value);
}

void SetObjectAttribute(ax::mojom::blink::IntAttribute attribute,
                        QualifiedName qualified_name,
                        AXObject* object,
                        ui::AXNodeData* node_data,
                        const AtomicString& value) {
  if (object->IsProhibited(attribute))
    return;

  Element* element = object->GetElement();
  if (!element)
    return;

  Element* target = element->GetElementAttribute(qualified_name);

  if (!target)
    return;

  AXObject* ax_target = object->AXObjectCache().GetOrCreate(target);
  if (!ax_target)
    return;
  if (attribute == ax::mojom::blink::IntAttribute::kActivedescendantId &&
      !ax_target->IsVisible()) {
    return;
  }

  node_data->AddIntAttribute(attribute, ax_target->AXObjectID());
}

void SetIntListAttribute(ax::mojom::blink::IntListAttribute attribute,
                         QualifiedName qualified_name,
                         AXObject* object,
                         ui::AXNodeData* node_data,
                         const AtomicString& value) {
  Element* element = object->GetElement();
  if (!element)
    return;
  HeapVector<Member<Element>>* attr_associated_elements =
      element->GetElementArrayAttribute(qualified_name);
  if (!attr_associated_elements || attr_associated_elements->empty())
    return;
  std::vector<int32_t> ax_ids;

  for (const auto& associated_element : *attr_associated_elements) {
    AXObject* ax_element =
        object->AXObjectCache().GetOrCreate(associated_element);
    if (!ax_element)
      continue;
    if (!ax_element->AccessibilityIsIgnored())
      ax_ids.push_back(ax_element->AXObjectID());
  }
  node_data->AddIntListAttribute(attribute, ax_ids);
}

AXSparseAttributeSetterMap& GetAXSparseAttributeSetterMap() {
  DEFINE_STATIC_LOCAL(AXSparseAttributeSetterMap, ax_sparse_setter_map, ());
  if (ax_sparse_setter_map.empty()) {
    ax_sparse_setter_map.Set(
        html_names::kAriaActivedescendantAttr,
        WTF::BindRepeating(&SetObjectAttribute,
                           ax::mojom::blink::IntAttribute::kActivedescendantId,
                           html_names::kAriaActivedescendantAttr));
    ax_sparse_setter_map.Set(
        html_names::kAriaBraillelabelAttr,
        WTF::BindRepeating(
            &SetStringAttribute,
            ax::mojom::blink::StringAttribute::kAriaBrailleLabel));
    ax_sparse_setter_map.Set(
        html_names::kAriaBrailleroledescriptionAttr,
        WTF::BindRepeating(
            &SetNotEmptyStringAttribute,
            ax::mojom::blink::StringAttribute::kAriaBrailleRoleDescription));
    ax_sparse_setter_map.Set(
        html_names::kAriaBusyAttr,
        WTF::BindRepeating(&SetBoolAttribute,
                           ax::mojom::blink::BoolAttribute::kBusy));
    ax_sparse_setter_map.Set(
        html_names::kAriaColcountAttr,
        WTF::BindRepeating(&SetIntAttribute,
                           ax::mojom::blink::IntAttribute::kAriaColumnCount));
    ax_sparse_setter_map.Set(
        html_names::kAriaColindexAttr,
        WTF::BindRepeating(
            &SetIntAttribute,
            ax::mojom::blink::IntAttribute::kAriaCellColumnIndex));
    ax_sparse_setter_map.Set(
        html_names::kAriaColspanAttr,
        WTF::BindRepeating(
            &SetIntAttribute,
            ax::mojom::blink::IntAttribute::kAriaCellColumnSpan));
    ax_sparse_setter_map.Set(
        html_names::kAriaControlsAttr,
        WTF::BindRepeating(&SetIntListAttribute,
                           ax::mojom::blink::IntListAttribute::kControlsIds,
                           html_names::kAriaControlsAttr));
    ax_sparse_setter_map.Set(
        html_names::kAriaErrormessageAttr,
        WTF::BindRepeating(&SetObjectAttribute,
                           ax::mojom::blink::IntAttribute::kErrormessageId,
                           html_names::kAriaErrormessageAttr));
    ax_sparse_setter_map.Set(
        html_names::kAriaDetailsAttr,
        WTF::BindRepeating(&SetIntListAttribute,
                           ax::mojom::blink::IntListAttribute::kDetailsIds,
                           html_names::kAriaDetailsAttr));
    ax_sparse_setter_map.Set(
        html_names::kAriaFlowtoAttr,
        WTF::BindRepeating(&SetIntListAttribute,
                           ax::mojom::blink::IntListAttribute::kFlowtoIds,
                           html_names::kAriaFlowtoAttr));
    ax_sparse_setter_map.Set(
        html_names::kAriaRowcountAttr,
        WTF::BindRepeating(&SetIntAttribute,
                           ax::mojom::blink::IntAttribute::kAriaRowCount));
    ax_sparse_setter_map.Set(
        html_names::kAriaRowindexAttr,
        WTF::BindRepeating(&SetIntAttribute,
                           ax::mojom::blink::IntAttribute::kAriaCellRowIndex));
    ax_sparse_setter_map.Set(
        html_names::kAriaRowspanAttr,
        WTF::BindRepeating(&SetIntAttribute,
                           ax::mojom::blink::IntAttribute::kAriaCellRowSpan));
    ax_sparse_setter_map.Set(
        html_names::kAriaRoledescriptionAttr,
        WTF::BindRepeating(
            &SetStringAttribute,
            ax::mojom::blink::StringAttribute::kRoleDescription));
    ax_sparse_setter_map.Set(
        html_names::kAriaTouchpassthroughAttr,
        WTF::BindRepeating(&SetBoolAttribute,
                           ax::mojom::blink::BoolAttribute::kTouchPassthrough));
    if (RuntimeEnabledFeatures::AccessibilityAriaVirtualContentEnabled()) {
      ax_sparse_setter_map.Set(
          html_names::kAriaVirtualcontentAttr,
          WTF::BindRepeating(
              &SetStringAttribute,
              ax::mojom::blink::StringAttribute::kVirtualContent));
    }
    ax_sparse_setter_map.Set(
        html_names::kAriaKeyshortcutsAttr,
        WTF::BindRepeating(&SetStringAttribute,
                           ax::mojom::blink::StringAttribute::kKeyShortcuts));
  }

  return ax_sparse_setter_map;
}

void AXNodeDataAOMPropertyClient::AddStringProperty(AOMStringProperty property,
                                                    const String& value) {
  ax::mojom::blink::StringAttribute attribute;
  switch (property) {
    case AOMStringProperty::kAriaBrailleLabel:
      attribute = ax::mojom::blink::StringAttribute::kAriaBrailleLabel;
      break;
    case AOMStringProperty::kAriaBrailleRoleDescription:
      attribute =
          ax::mojom::blink::StringAttribute::kAriaBrailleRoleDescription;
      break;
    case AOMStringProperty::kKeyShortcuts:
      attribute = ax::mojom::blink::StringAttribute::kKeyShortcuts;
      break;
    case AOMStringProperty::kRoleDescription:
      attribute = ax::mojom::blink::StringAttribute::kRoleDescription;
      break;
    case AOMStringProperty::kVirtualContent:
      if (!RuntimeEnabledFeatures::AccessibilityAriaVirtualContentEnabled())
        return;
      attribute = ax::mojom::blink::StringAttribute::kVirtualContent;
      break;
    default:
      return;
  }
  node_data_.AddStringAttribute(attribute, value.Utf8());
}

void AXNodeDataAOMPropertyClient::AddBooleanProperty(
    AOMBooleanProperty property,
    bool value) {
  ax::mojom::blink::BoolAttribute attribute;
  switch (property) {
    case AOMBooleanProperty::kBusy:
      attribute = ax::mojom::blink::BoolAttribute::kBusy;
      break;
    default:
      return;
  }
  node_data_.AddBoolAttribute(attribute, value);
}

void AXNodeDataAOMPropertyClient::AddFloatProperty(AOMFloatProperty property,
                                                   float value) {}

void AXNodeDataAOMPropertyClient::AddRelationProperty(
    AOMRelationProperty property,
    const AccessibleNode& value) {
  ax::mojom::blink::IntAttribute attribute;
  switch (property) {
    case AOMRelationProperty::kActiveDescendant:
      attribute = ax::mojom::blink::IntAttribute::kActivedescendantId;
      break;
    case AOMRelationProperty::kErrorMessage:
      attribute = ax::mojom::blink::IntAttribute::kErrormessageId;
      break;
    default:
      return;
  }

  Element* target = value.element();
  AXObject* ax_target = ax_object_cache_->GetOrCreate(target);
  if (!ax_target)
    return;

  node_data_.AddIntAttribute(attribute, ax_target->AXObjectID());
}

void AXNodeDataAOMPropertyClient::AddRelationListProperty(
    AOMRelationListProperty property,
    const AccessibleNodeList& relations) {
  ax::mojom::blink::IntListAttribute attribute;
  switch (property) {
    case AOMRelationListProperty::kControls:
      attribute = ax::mojom::blink::IntListAttribute::kControlsIds;
      break;
    case AOMRelationListProperty::kDetails:
      attribute = ax::mojom::blink::IntListAttribute::kDetailsIds;
      break;
    case AOMRelationListProperty::kFlowTo:
      attribute = ax::mojom::blink::IntListAttribute::kFlowtoIds;
      break;
    default:
      return;
  }

  std::vector<int32_t> ax_ids;
  for (unsigned i = 0; i < relations.length(); ++i) {
    AccessibleNode* accessible_node = relations.item(i);
    if (accessible_node) {
      Element* element = accessible_node->element();
      AXObject* ax_element = ax_object_cache_->GetOrCreate(element);
      if (ax_element && !ax_element->AccessibilityIsIgnored())
        ax_ids.push_back(ax_element->AXObjectID());
    }
  }

  node_data_.AddIntListAttribute(attribute, ax_ids);
}

}  // namespace blink
