// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_read_only.h"

namespace blink {

IntersectionObserverEntry::IntersectionObserverEntry(
    const IntersectionGeometry& geometry,
    DOMHighResTimeStamp time,
    Element* target)
    : geometry_(geometry), time_(time), target_(target) {}

DOMRectReadOnly* IntersectionObserverEntry::boundingClientRect() const {
  return DOMRectReadOnly::FromRectF(gfx::RectF(geometry_.TargetRect()));
}

DOMRectReadOnly* IntersectionObserverEntry::rootBounds() const {
  if (geometry_.ShouldReportRootBounds())
    return DOMRectReadOnly::FromRectF(gfx::RectF(geometry_.RootRect()));
  return nullptr;
}

DOMRectReadOnly* IntersectionObserverEntry::intersectionRect() const {
  return DOMRectReadOnly::FromRectF(gfx::RectF(geometry_.IntersectionRect()));
}

void IntersectionObserverEntry::Trace(Visitor* visitor) const {
  visitor->Trace(target_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
