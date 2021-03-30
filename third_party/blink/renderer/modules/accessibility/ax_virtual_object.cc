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

Document* AXVirtualObject::GetDocument() const {
  return GetAccessibleNode() ? GetAccessibleNode()->GetDocument() : nullptr;
}

bool AXVirtualObject::ComputeAccessibilityIsIgnored(
    IgnoredReasons* ignoredReasons) const {
  return AccessibilityIsIgnoredByDefault(ignoredReasons);
}

void AXVirtualObject::AddChildren() {
#if DCHECK_IS_ON()
  DCHECK(!IsDetached());
  DCHECK(!is_adding_children_) << " Reentering method on " << GetNode();
  base::AutoReset<bool> reentrancy_protector(&is_adding_children_, true);
  DCHECK_EQ(children_.size(), 0U)
      << "Parent still has " << children_.size() << " children before adding:"
      << "\nParent is " << ToString(true, true) << "\nFirst child is "
      << children_[0]->ToString(true, true);
#endif
  if (!accessible_node_)
    return;

  DCHECK(children_dirty_);
  children_dirty_ = false;

  for (const auto& child : accessible_node_->GetChildren()) {
    AXObject* ax_child = AXObjectCache().GetOrCreate(child, this);
    if (!ax_child)
      continue;
    DCHECK(!ax_child->IsDetached());
    DCHECK(ax_child->AccessibilityIsIncludedInTree());

    children_.push_back(ax_child);
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

ax::mojom::blink::Role AXVirtualObject::DetermineAccessibilityRole() {
  aria_role_ = DetermineAriaRoleAttribute();

  // If no role was assigned, fall back to role="generic".
  if (aria_role_ == ax::mojom::blink::Role::kUnknown)
    aria_role_ = ax::mojom::blink::Role::kGenericContainer;

  return aria_role_;
}

void AXVirtualObject::Trace(Visitor* visitor) const {
  visitor->Trace(accessible_node_);
  AXObject::Trace(visitor);
}
}  // namespace blink
