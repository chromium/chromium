// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/overscroll/overscroll_area_tracker.h"

#include "base/token.h"
#include "third_party/blink/renderer/core/dom/element.h"

namespace blink {

OverscrollAreaTracker::OverscrollAreaTracker(Element* element)
    : container_(element) {}

void OverscrollAreaTracker::AddOverscroll(Element* element,
                                          Element* activator) {
  auto* member = MakeGarbageCollected<OverscrollMember>();
  member->overscroll_element = element;
  member->activator = activator;
  member->token = AtomicString(base::Token::CreateRandom().ToString().data());
  // TODO(crbug.com/463970475): This should be in DOM order of `element`. See
  // `getAnimations()` for how it sorts things, and use it here.
  overscroll_members_.push_back(member);
}

void OverscrollAreaTracker::PropagateOverscrollToAncestor() {
  // TODO(crbug.com/463972821): Implement.
}

void OverscrollAreaTracker::TakeOverscrollFromAncestor() {
  // TODO(crbug.com/463972324): Implement.
}

void OverscrollAreaTracker::Trace(Visitor* visitor) const {
  ElementRareDataField::Trace(visitor);

  visitor->Trace(container_);
  visitor->Trace(overscroll_members_);
}

void OverscrollAreaTracker::OverscrollMember::Trace(Visitor* visitor) const {
  visitor->Trace(overscroll_element);
  visitor->Trace(activator);
}

}  // namespace blink
