// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/ax_virtual_object.h"

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

  for (const auto& child : accessible_node_->GetChildren())
    children_.push_back(AXObjectCache().GetOrCreate(child));
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

  bool is_null = true;
  result = accessible_node_->GetProperty(property, is_null);
  return !is_null;
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

void AXVirtualObject::Trace(blink::Visitor* visitor) {
  visitor->Trace(accessible_node_);
  AXObject::Trace(visitor);
}

}  // namespace blink
