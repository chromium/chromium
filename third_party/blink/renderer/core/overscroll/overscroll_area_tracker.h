// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_OVERSCROLL_OVERSCROLL_AREA_TRACKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_OVERSCROLL_OVERSCROLL_AREA_TRACKER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/element_rare_data_field.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap_traits.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class Element;

class CORE_EXPORT OverscrollAreaTracker
    : public GarbageCollected<OverscrollAreaTracker>,
      public ElementRareDataField {
 public:
  explicit OverscrollAreaTracker(Element*);

  // Adds a new overscroll element into our overscroll. The element is `element`
  // and `activator` is a button/link/activatable item that would scroll this
  // into view.
  void AddOverscroll(Element* element, Element* activator);

  // When this element is no longer an overscroll container, this is called to
  // propagate its overscroll elements to an ancestor.
  void PropagateOverscrollToAncestor();

  // When this element becomes an overscroll container, this is called to take
  // any relevant overscroll elements from its ancestor.
  void TakeOverscrollFromAncestor();

  void Trace(Visitor*) const override;

 private:
  friend class OverscrollAreaTrackerTest;

  Member<Element> container_;

  struct OverscrollMember : public GarbageCollected<OverscrollMember> {
    AtomicString token;
    Member<Element> overscroll_element;
    Member<Element> activator;

    void Trace(Visitor*) const;
  };

  VectorOf<OverscrollMember> overscroll_members_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_OVERSCROLL_OVERSCROLL_AREA_TRACKER_H_
