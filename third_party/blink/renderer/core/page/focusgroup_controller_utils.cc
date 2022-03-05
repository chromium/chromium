// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/focusgroup_controller_utils.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/focusgroup_flags.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"

namespace blink {

FocusgroupDirection FocusgroupControllerUtils::FocusgroupDirectionForEvent(
    KeyboardEvent* event) {
  DCHECK(event);
  if (event->ctrlKey() || event->metaKey() || event->shiftKey())
    return FocusgroupDirection::kNone;

  // TODO(bebeaudr): Support RTL. Will it be as simple as inverting the
  // direction associated with the left and right arrows when in a RTL element?
  if (event->key() == "ArrowDown")
    return FocusgroupDirection::kForwardVertical;
  else if (event->key() == "ArrowRight")
    return FocusgroupDirection::kForwardHorizontal;
  else if (event->key() == "ArrowUp")
    return FocusgroupDirection::kBackwardVertical;
  else if (event->key() == "ArrowLeft")
    return FocusgroupDirection::kBackwardHorizontal;

  return FocusgroupDirection::kNone;
}

bool FocusgroupControllerUtils::IsDirectionForward(
    FocusgroupDirection direction) {
  return direction == FocusgroupDirection::kForwardHorizontal ||
         direction == FocusgroupDirection::kForwardVertical;
}

bool FocusgroupControllerUtils::IsDirectionBackward(
    FocusgroupDirection direction) {
  return direction == FocusgroupDirection::kBackwardHorizontal ||
         direction == FocusgroupDirection::kBackwardVertical;
}

bool FocusgroupControllerUtils::IsDirectionHorizontal(
    FocusgroupDirection direction) {
  return direction == FocusgroupDirection::kBackwardHorizontal ||
         direction == FocusgroupDirection::kForwardHorizontal;
}

bool FocusgroupControllerUtils::IsDirectionVertical(
    FocusgroupDirection direction) {
  return direction == FocusgroupDirection::kBackwardVertical ||
         direction == FocusgroupDirection::kForwardVertical;
}

bool FocusgroupControllerUtils::IsAxisSupported(FocusgroupFlags flags,
                                                FocusgroupDirection direction) {
  return ((flags & FocusgroupFlags::kHorizontal) &&
          IsDirectionHorizontal(direction)) ||
         ((flags & FocusgroupFlags::kVertical) &&
          IsDirectionVertical(direction));
}

bool FocusgroupControllerUtils::WrapsInDirection(
    FocusgroupFlags flags,
    FocusgroupDirection direction) {
  return ((flags & FocusgroupFlags::kWrapHorizontally) &&
          IsDirectionHorizontal(direction)) ||
         ((flags & FocusgroupFlags::kWrapVertically) &&
          IsDirectionVertical(direction));
}

Element* FocusgroupControllerUtils::FindNearestFocusgroupAncestor(
    const Element* element) {
  if (!element)
    return nullptr;

  for (Element* ancestor = FlatTreeTraversal::ParentElement(*element); ancestor;
       ancestor = FlatTreeTraversal::ParentElement(*ancestor)) {
    FocusgroupFlags ancestor_flags = ancestor->GetFocusgroupFlags();
    if (ancestor_flags != FocusgroupFlags::kNone)
      return ancestor;
  }

  return nullptr;
}

Element* FocusgroupControllerUtils::NextElement(const Element* current,
                                                bool skip_subtree) {
  Node* node;
  if (skip_subtree)
    node = FlatTreeTraversal::NextSkippingChildren(*current);
  else
    node = FlatTreeTraversal::Next(*current);

  Element* next_element;
  // Here, we don't need to skip the subtree when getting the next element since
  // we've already skipped the subtree we wanted to skipped by calling
  // NextSkippingChildren above.
  for (; node; node = FlatTreeTraversal::Next(*node)) {
    next_element = DynamicTo<Element>(node);
    if (next_element)
      return next_element;
  }
  return nullptr;
}

bool FocusgroupControllerUtils::IsFocusgroupItem(const Element* element) {
  if (!element || !element->IsFocusable())
    return false;

  // All children of a focusgroup are considered focusgroup item if they are
  // focusable.
  Element* parent = FlatTreeTraversal::ParentElement(*element);
  FocusgroupFlags parent_flags = parent->GetFocusgroupFlags();
  return parent_flags != FocusgroupFlags::kNone;
}

}  // namespace blink