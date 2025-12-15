// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/overscroll/overscroll_area_tracker.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

OverscrollAreaTracker::OverscrollAreaTracker(Element* element)
    : container_(element) {}

void OverscrollAreaTracker::AddOverscroll(Element* element) {
  CHECK(!element->OverscrollContainer());
  element->SetOverscrollContainer(container_);
  overscroll_members_.push_back(element);
  needs_dom_sort_ = overscroll_members_.size() > 1;
}

const VectorOf<Element>& OverscrollAreaTracker::DOMSortedElements() {
  if (needs_dom_sort_) {
    // TODO(crbug.com/463970475): Implement.
    needs_dom_sort_ = false;
  }
  return overscroll_members_;
}

void OverscrollAreaTracker::RemoveOverscroll(Element* element) {
  CHECK_EQ(element->OverscrollContainer(), container_);
  element->ClearOverscrollContainer();
  Erase(overscroll_members_, element);
  needs_dom_sort_ = needs_dom_sort_ && overscroll_members_.size() > 1;
}

void OverscrollAreaTracker::Trace(Visitor* visitor) const {
  ElementRareDataField::Trace(visitor);

  visitor->Trace(container_);
  visitor->Trace(overscroll_members_);
}

}  // namespace blink
