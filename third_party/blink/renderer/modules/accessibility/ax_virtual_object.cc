// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/ax_virtual_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/ax_sparse_attribute_setter.h"

namespace blink {

AXVirtualObject::AXVirtualObject(AXObjectCacheImpl& axObjectCache,
                                 AccessibleNode* accessible_node)
    : AXObject(axObjectCache), accessible_node_(accessible_node) {
}

AXVirtualObject::~AXVirtualObject() = default;

void AXVirtualObject::Detach() {
  AXObject::Detach();

  accessible_node_ = nullptr;
}

bool AXVirtualObject::ComputeAccessibilityIsIgnored(
    IgnoredReasons* ignoredReasons) const {
  return AccessibilityIsIgnoredByDefault(ignoredReasons);
}

void AXVirtualObject::AddChildren() {
  if (!accessible_node_)
    return;

  DCHECK(!have_children_);
  have_children_ = true;

  for (const auto& child : accessible_node_->GetChildren()) {
    AXObject* ax_child = AXObjectCache().GetOrCreate(child);
    if (!ax_child)
      continue;

    children_.push_back(ax_child);
    ax_child->SetParent(this);
  }
}

void AXVirtualObject::ChildrenChanged() {
  ClearChildren();
  AXObjectCache().PostNotification(this, ax::mojom::Event::kChildrenChanged);
}

const AtomicString& AXVirtualObject::GetAOMPropertyOrARIAAttribute(
    AOMStringProperty property) const {
  if (!accessible_node_)
    return g_null_atom;

  return accessible_node_->GetProperty(property);
}

bool AXVirtualObject::HasAOMPropertyOrARIAAttribute(AOMBooleanProperty property,
                                                    bool& result) const {
  if (!accessible_node_)
    return false;

  base::Optional<bool> property_value = accessible_node_->GetProperty(property);
  result = property_value.value_or(false);
  return property_value.has_value();
}

AccessibleNode* AXVirtualObject::GetAccessibleNode() const {
  return accessible_node_;
}

String AXVirtualObject::TextAlternative(bool recursive,
                                        bool in_aria_labelled_by_traversal,
                                        AXObjectSet& visited,
                                        ax::mojom::NameFrom& name_from,
                                        AXRelatedObjectVector* related_objects,
                                        NameSources* name_sources) const {
  if (!accessible_node_)
    return String();

  bool found_text_alternative = false;
  return AriaTextAlternative(recursive, in_aria_labelled_by_traversal, visited,
                             name_from, related_objects, name_sources,
                             &found_text_alternative);
}

void AXVirtualObject::Trace(Visitor* visitor) const {
  visitor->Trace(accessible_node_);
  AXObject::Trace(visitor);
}
}  // namespace blink
