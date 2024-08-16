// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/scroll_marker_group_pseudo_element.h"

namespace blink {
void ScrollMarkerGroupPseudoElement::Trace(Visitor* v) const {
  v->Trace(selected_marker_);
  v->Trace(focus_group_);
  PseudoElement::Trace(v);
}

void ScrollMarkerGroupPseudoElement::AddToFocusGroup(
    ScrollMarkerPseudoElement& scroll_marker) {
  scroll_marker.SetScrollMarkerGroup(this);
  focus_group_.push_back(scroll_marker);
}

ScrollMarkerPseudoElement* ScrollMarkerGroupPseudoElement::FindNextScrollMarker(
    const Element& current) {
  if (wtf_size_t index = focus_group_.Find(current); index != kNotFound) {
    return focus_group_[index == focus_group_.size() - 1 ? 0u : index + 1];
  }
  return nullptr;
}

ScrollMarkerPseudoElement*
ScrollMarkerGroupPseudoElement::FindPreviousScrollMarker(
    const Element& current) {
  if (wtf_size_t index = focus_group_.Find(current); index != kNotFound) {
    return focus_group_[index == 0u ? focus_group_.size() - 1 : index - 1];
  }
  return nullptr;
}

void ScrollMarkerGroupPseudoElement::RemoveFromFocusGroup(
    const ScrollMarkerPseudoElement& scroll_marker) {
  if (wtf_size_t index = focus_group_.Find(scroll_marker); index != kNotFound) {
    focus_group_.EraseAt(index);
    if (selected_marker_ == scroll_marker) {
      selected_marker_->SetSelected(false);
      if (index == focus_group_.size()) {
        if (index == 0) {
          selected_marker_ = nullptr;
          return;
        }
        --index;
      }
      selected_marker_ = focus_group_[index];
      selected_marker_->SetSelected(true);
    }
  }
}

void ScrollMarkerGroupPseudoElement::SetSelected(
    ScrollMarkerPseudoElement& scroll_marker) {
  if (selected_marker_ == scroll_marker) {
    return;
  }
  if (selected_marker_) {
    selected_marker_->SetSelected(false);
  }
  scroll_marker.SetSelected(true);
  selected_marker_ = scroll_marker;
}

void ScrollMarkerGroupPseudoElement::Dispose() {
  HeapVector<Member<ScrollMarkerPseudoElement>> focus_group =
      std::move(focus_group_);
  for (ScrollMarkerPseudoElement* scroll_marker : focus_group) {
    scroll_marker->SetScrollMarkerGroup(nullptr);
  }
  if (selected_marker_) {
    selected_marker_->SetSelected(false);
    selected_marker_ = nullptr;
  }
  PseudoElement::Dispose();
}

void ScrollMarkerGroupPseudoElement::ClearFocusGroup() {
  if (selected_marker_) {
    selected_marker_->SetSelected(false);
    selected_marker_ = nullptr;
  }
  focus_group_.clear();
}

}  // namespace blink
