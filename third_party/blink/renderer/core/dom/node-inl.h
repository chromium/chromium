// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NODE_INL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NODE_INL_H_

#include <climits>
#include <concepts>

#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/element_rare_data_vector.h"
#include "third_party/blink/renderer/core/dom/node.h"

namespace blink {

DOMNodeId Node::NodeID(base::PassKey<DOMNodeIds>) const {
  return data_ ? const_cast<const ElementRareDataVector*>(data_.Get())->NodeId()
               : kInvalidDOMNodeId;
}
DOMNodeId& Node::EnsureNodeID(base::PassKey<DOMNodeIds>) {
  return UnpackAndRefresh(EnsureRareData().NodeId());
}

bool ContainerNode::HasRestyleFlag(DynamicRestyleFlags mask) const {
  if (const ElementRareDataVector* data = RareData()) {
    return data->HasRestyleFlag(mask);
  }
  return false;
}

bool ContainerNode::HasRestyleFlags() const {
  if (const ElementRareDataVector* data = RareData()) {
    return data->HasRestyleFlags();
  }
  return false;
}

bool ContainerNode::ChildrenOrSiblingsAffectedByFocus() const {
  return HasRestyleFlag(
      DynamicRestyleFlags::kChildrenOrSiblingsAffectedByFocus);
}
void ContainerNode::SetChildrenOrSiblingsAffectedByFocus() {
  SetRestyleFlag(DynamicRestyleFlags::kChildrenOrSiblingsAffectedByFocus);
}

bool ContainerNode::ChildrenOrSiblingsAffectedByFocusVisible() const {
  return HasRestyleFlag(
      DynamicRestyleFlags::kChildrenOrSiblingsAffectedByFocusVisible);
}
void ContainerNode::SetChildrenOrSiblingsAffectedByFocusVisible() {
  SetRestyleFlag(
      DynamicRestyleFlags::kChildrenOrSiblingsAffectedByFocusVisible);
}

bool ContainerNode::ChildrenOrSiblingsAffectedByFocusWithin() const {
  return HasRestyleFlag(
      DynamicRestyleFlags::kChildrenOrSiblingsAffectedByFocusWithin);
}
void ContainerNode::SetChildrenOrSiblingsAffectedByFocusWithin() {
  SetRestyleFlag(DynamicRestyleFlags::kChildrenOrSiblingsAffectedByFocusWithin);
}

bool ContainerNode::ChildrenOrSiblingsAffectedByHover() const {
  return HasRestyleFlag(
      DynamicRestyleFlags::kChildrenOrSiblingsAffectedByHover);
}
void ContainerNode::SetChildrenOrSiblingsAffectedByHover() {
  SetRestyleFlag(DynamicRestyleFlags::kChildrenOrSiblingsAffectedByHover);
}

bool ContainerNode::ChildrenOrSiblingsAffectedByActive() const {
  return HasRestyleFlag(
      DynamicRestyleFlags::kChildrenOrSiblingsAffectedByActive);
}
void ContainerNode::SetChildrenOrSiblingsAffectedByActive() {
  SetRestyleFlag(DynamicRestyleFlags::kChildrenOrSiblingsAffectedByActive);
}

bool ContainerNode::ChildrenOrSiblingsAffectedByDrag() const {
  return HasRestyleFlag(DynamicRestyleFlags::kChildrenOrSiblingsAffectedByDrag);
}
void ContainerNode::SetChildrenOrSiblingsAffectedByDrag() {
  SetRestyleFlag(DynamicRestyleFlags::kChildrenOrSiblingsAffectedByDrag);
}

bool ContainerNode::ChildrenAffectedByFirstChildRules() const {
  return HasRestyleFlag(
      DynamicRestyleFlags::kChildrenAffectedByFirstChildRules);
}
void ContainerNode::SetChildrenAffectedByFirstChildRules() {
  SetRestyleFlag(DynamicRestyleFlags::kChildrenAffectedByFirstChildRules);
}

bool ContainerNode::ChildrenAffectedByLastChildRules() const {
  return HasRestyleFlag(DynamicRestyleFlags::kChildrenAffectedByLastChildRules);
}
void ContainerNode::SetChildrenAffectedByLastChildRules() {
  SetRestyleFlag(DynamicRestyleFlags::kChildrenAffectedByLastChildRules);
}

bool ContainerNode::ChildrenAffectedByDirectAdjacentRules() const {
  return HasRestyleFlag(
      DynamicRestyleFlags::kChildrenAffectedByDirectAdjacentRules);
}
void ContainerNode::SetChildrenAffectedByDirectAdjacentRules() {
  SetRestyleFlag(DynamicRestyleFlags::kChildrenAffectedByDirectAdjacentRules);
}

bool ContainerNode::ChildrenAffectedByIndirectAdjacentRules() const {
  return HasRestyleFlag(
      DynamicRestyleFlags::kChildrenAffectedByIndirectAdjacentRules);
}
void ContainerNode::SetChildrenAffectedByIndirectAdjacentRules() {
  SetRestyleFlag(DynamicRestyleFlags::kChildrenAffectedByIndirectAdjacentRules);
}

bool ContainerNode::ChildrenAffectedByForwardPositionalRules() const {
  return HasRestyleFlag(
      DynamicRestyleFlags::kChildrenAffectedByForwardPositionalRules);
}
void ContainerNode::SetChildrenAffectedByForwardPositionalRules() {
  SetRestyleFlag(
      DynamicRestyleFlags::kChildrenAffectedByForwardPositionalRules);
}

bool ContainerNode::ChildrenAffectedByBackwardPositionalRules() const {
  return HasRestyleFlag(
      DynamicRestyleFlags::kChildrenAffectedByBackwardPositionalRules);
}
void ContainerNode::SetChildrenAffectedByBackwardPositionalRules() {
  SetRestyleFlag(
      DynamicRestyleFlags::kChildrenAffectedByBackwardPositionalRules);
}

bool ContainerNode::AffectedByFirstChildRules() const {
  return HasRestyleFlag(DynamicRestyleFlags::kAffectedByFirstChildRules);
}
void ContainerNode::SetAffectedByFirstChildRules() {
  SetRestyleFlag(DynamicRestyleFlags::kAffectedByFirstChildRules);
}

bool ContainerNode::AffectedByLastChildRules() const {
  return HasRestyleFlag(DynamicRestyleFlags::kAffectedByLastChildRules);
}
void ContainerNode::SetAffectedByLastChildRules() {
  SetRestyleFlag(DynamicRestyleFlags::kAffectedByLastChildRules);
}

inline bool ContainerNode::NeedsAdjacentStyleRecalc() const {
  if (!ChildrenAffectedByDirectAdjacentRules() &&
      !ChildrenAffectedByIndirectAdjacentRules()) {
    return false;
  }
  return ChildNeedsStyleRecalc() || ChildNeedsStyleInvalidation();
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NODE_INL_H_
