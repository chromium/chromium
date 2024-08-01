// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/scroll_marker_group_pseudo_element.h"

namespace blink {
void ScrollMarkerGroupPseudoElement::Trace(Visitor* v) const {
  v->Trace(focus_group_);
  PseudoElement::Trace(v);
}

void ScrollMarkerGroupPseudoElement::AddToFocusGroup(
    ScrollMarkerPseudoElement& scroll_marker) {
  focus_group_.push_back(scroll_marker);
}

ScrollMarkerPseudoElement*
ScrollMarkerGroupPseudoElement::FindFocusableElementForward(
    const Element& current) {
  if (wtf_size_t id = focus_group_.Find(current); id != kNotFound) {
    return focus_group_[id == focus_group_.size() - 1 ? 0u : id + 1];
  }
  return nullptr;
}

ScrollMarkerPseudoElement*
ScrollMarkerGroupPseudoElement::FindFocusableElementBackward(
    const Element& current) {
  if (wtf_size_t id = focus_group_.Find(current); id != kNotFound) {
    return focus_group_[id == 0u ? focus_group_.size() - 1 : id - 1];
  }
  return nullptr;
}

void ScrollMarkerGroupPseudoElement::RemoveFromFocusGroup(
    const ScrollMarkerPseudoElement& scroll_marker) {
  if (wtf_size_t id = focus_group_.Find(scroll_marker); id != kNotFound) {
    focus_group_.EraseAt(id);
  }
}

void ScrollMarkerGroupPseudoElement::Dispose() {
  for (ScrollMarkerPseudoElement* scroll_marker : focus_group_) {
    scroll_marker->SetScrollMarkerGroup(nullptr);
  }
  PseudoElement::Dispose();
}

void ScrollMarkerGroupPseudoElement::ClearFocusGroup() {
  focus_group_.clear();
}

}  // namespace blink
