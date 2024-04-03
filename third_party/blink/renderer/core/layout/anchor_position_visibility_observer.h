// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_ANCHOR_POSITION_VISIBILITY_OBSERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_ANCHOR_POSITION_VISIBILITY_OBSERVER_H_

#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

class Element;
class IntersectionObserver;
class IntersectionObserverEntry;

// Monitors visibility of an anchor element for an anchored element, to support
// `position-visibility: anchors-visible` (see:
// https://github.com/w3c/csswg-drafts/issues/7758). When the anchor is detected
// as newly-visible or newly-invisible, the anchored element's `PaintLayer` is
// updated via `PaintLayer::SetInvisibleForPositionVisibility`.
// TODO(pdr): The position-visibility of the anchor element should also be based
// on the `visibility` property of `anchor`, which is not tracked here.
class AnchorPositionVisibilityObserver final
    : public GarbageCollected<AnchorPositionVisibilityObserver> {
 public:
  explicit AnchorPositionVisibilityObserver(Element& anchored_element);

  // Sets the currently monitored anchor element.
  void MonitorAnchor(const Element*);

  void Trace(Visitor*) const;

 private:
  void UpdateLayerInvisible(bool);

  void OnVisibilityChanged(
      const HeapVector<Member<IntersectionObserverEntry>>&);

  Member<IntersectionObserver> observer_;
  Member<Element> anchored_element_;
  Member<const Element> anchor_element_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_ANCHOR_POSITION_VISIBILITY_OBSERVER_H_
