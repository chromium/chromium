// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/testing/internals_accessibility.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_accessibility_properties.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/testing/internals.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "ui/accessibility/ax_enum_util.h"

namespace blink {

unsigned InternalsAccessibility::numberOfLiveAXObjects(Internals&) {
  return AXObject::NumberOfLiveAXObjects();
}

namespace {
AXObject* GetAXObject(const Element* element) {
  Document& document = element->GetDocument();
  auto* ax_object_cache =
      To<AXObjectCacheImpl>(document.ExistingAXObjectCache());
  CHECK(ax_object_cache);
  ax_object_cache->UpdateAXForAllDocuments();
  return ax_object_cache->Get(element);
}

String GetRoleName(AXObject* ax_object) {
  // TODO(crbug.com/486968069): Differentiate between nodes which are ignored
  // but included in the tree and nodes which do not exist.
  if (!ax_object || ax_object->IsIgnored()) {
    return AXObject::AriaRoleName(ax::mojom::Role::kNone);
  }

  ax::mojom::blink::Role role = ax_object->ComputeFinalRoleForSerialization();
  return AXObject::AriaRoleName(role);
}

String GetLabel(AXObject* ax_object) {
  if (!ax_object || ax_object->IsIgnored()) {
    return g_empty_string;
  }

  ax::mojom::NameFrom name_from;
  AXObject::AXObjectVector name_objects;
  return ax_object->GetName(name_from, &name_objects, nullptr);
}

AccessibilityProperties* GetAccessibilityProperties(AXObject* ax_object) {
  auto* props = AccessibilityProperties::Create();

  if (!ax_object) {
    props->setRole(AXObject::AriaRoleName(ax::mojom::Role::kNone));
    return props;
  }

  props->setRole(GetRoleName(ax_object));
  props->setAccessibilityId(ax_object->AXObjectID());

  if (AXObject* parent = ax_object->ParentObjectIncludedInTree()) {
    props->setParent(parent->AXObjectID());
  }

  auto children = ax_object->ChildrenIncludingIgnored();
  // TODO(crbug.com/486968069): For now, WPT tests are written to expect an
  // empty array of children when there are no children. This is not yet
  // formalized and may change.
  Vector<int32_t> children_ids;
  for (const auto& child : children) {
    children_ids.push_back(child->AXObjectID());
  }
  props->setChildren(children_ids);

  // Do not calculate more attributes if ignored.
  if (ax_object->IsIgnored()) {
    return props;
  }

  props->setLabel(GetLabel(ax_object));

  ui::AXNodeData node_data;
  ax_object->Serialize(&node_data, ui::kAXModeInspector);

  const char* checked_prop_val = nullptr;
  switch (node_data.GetCheckedState()) {
    case ax::mojom::blink::CheckedState::kTrue:
      checked_prop_val = "true";
      break;
    case ax::mojom::blink::CheckedState::kMixed:
      checked_prop_val = "mixed";
      break;
    case ax::mojom::blink::CheckedState::kFalse:
      checked_prop_val = "false";
      break;
    case ax::mojom::blink::CheckedState::kNone:
      break;
  }

  // TODO(crbug.com/514379955): Add "pressed" concept to AXNodeData
  // to not have to perform role check here.
  if (checked_prop_val) {
    if (node_data.role == ax::mojom::blink::Role::kToggleButton) {
      props->setPressed(checked_prop_val);
    }
    props->setChecked(checked_prop_val);
  }

  return props;
}

}  // namespace

// static
String InternalsAccessibility::getComputedLabel(Internals&,
                                                const Element* element) {
  AXObject* ax_object = GetAXObject(element);
  return GetLabel(ax_object);
}

// static
String InternalsAccessibility::getComputedRole(Internals&,
                                               const Element* element) {
  AXObject* ax_object = GetAXObject(element);
  return GetRoleName(ax_object);
}

// static
AccessibilityProperties*
InternalsAccessibility::getAccessibilityPropertiesForElement(
    Internals&,
    const Element* element) {
  Document& document = element->GetDocument();
  auto* ax_object_cache =
      To<AXObjectCacheImpl>(document.ExistingAXObjectCache());
  CHECK(ax_object_cache);
  ax_object_cache->UpdateAXForAllDocuments();

  // Freeze is necessary to serialize the ax_object.
  ScopedFreezeAXCache freeze(*ax_object_cache);

  AXObject* ax_object = ax_object_cache->Get(element);

  return GetAccessibilityProperties(ax_object);
}

// static
AccessibilityProperties*
InternalsAccessibility::getAccessibilityPropertiesForAccessibilityNode(
    Internals&,
    const Element* element,
    int32_t accessibility_id) {
  Document& document = element->GetDocument();
  auto* ax_object_cache =
      To<AXObjectCacheImpl>(document.ExistingAXObjectCache());
  CHECK(ax_object_cache);
  ax_object_cache->UpdateAXForAllDocuments();

  // Freeze is necessary to serialize the ax_object.
  ScopedFreezeAXCache freeze(*ax_object_cache);

  AXObject* ax_object = ax_object_cache->ObjectFromAXID(accessibility_id);

  return GetAccessibilityProperties(ax_object);
}

}  // namespace blink
