// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/ax_virtual_object.h"

#include "base/auto_reset.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/ax_sparse_attribute_setter.h"

namespace blink {

AXVirtualObject::AXVirtualObject(AXObjectCacheImpl& axObjectCache,
                                 AccessibleNode* accessible_node)
    : AXObject(axObjectCache),
      accessible_node_(accessible_node),
      aria_role_(ax::mojom::blink::Role::kUnknown) {
  DCHECK(accessible_node_);
  DCHECK(!accessible_node_->element())
      << "The accessible node directly attached to an element should not "
         "have its own AXObject, since the AXObject will be keyed off of "
         "the element instead: "
      << accessible_node_->element();
}

AXVirtualObject::~AXVirtualObject() = default;

void AXVirtualObject::Detach() {
  AXObject::Detach();

  accessible_node_ = nullptr;
}

Document* AXVirtualObject::GetDocument() const {
  return GetAccessibleNode() ? GetAccessibleNode()->GetDocument() : nullptr;
}

void AXVirtualObject::AddChildren() {
#if defined(AX_FAIL_FAST_BUILD)
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

  absl::optional<bool> property_value = accessible_node_->GetProperty(property);
  result = property_value.value_or(false);
  return property_value.has_value();
}

AccessibleNode* AXVirtualObject::GetAccessibleNode() const {
  return accessible_node_.Get();
}

String AXVirtualObject::TextAlternative(
    bool recursive,
    const AXObject* aria_label_or_description_root,
    AXObjectSet& visited,
    ax::mojom::NameFrom& name_from,
    AXRelatedObjectVector* related_objects,
    NameSources* name_sources) const {
  if (!accessible_node_)
    return String();

  bool found_text_alternative = false;
  return AriaTextAlternative(recursive, aria_label_or_description_root, visited,
                             name_from, related_objects, name_sources,
                             &found_text_alternative);
}

ax::mojom::blink::Role AXVirtualObject::DetermineAccessibilityRole() {
  aria_role_ = DetermineAriaRoleAttribute();

  if (aria_role_ != ax::mojom::blink::Role::kUnknown)
    return aria_role_;

  return NativeRoleIgnoringAria();
}

ax::mojom::blink::Role AXVirtualObject::AriaRoleAttribute() const {
  return aria_role_;
}

ax::mojom::blink::Role AXVirtualObject::NativeRoleIgnoringAria() const {
  // If no role was assigned, will fall back to role="generic".
  return ax::mojom::blink::Role::kGenericContainer;
}

void AXVirtualObject::Trace(Visitor* visitor) const {
  visitor->Trace(accessible_node_);
  AXObject::Trace(visitor);
}
}  // namespace blink
