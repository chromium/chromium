// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_OVERSCROLL_OVERSCROLL_AREA_TRACKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_OVERSCROLL_OVERSCROLL_AREA_TRACKER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/element.h"
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

  void AddOverscroll(Element*);
  void RemoveOverscroll(Element*);
  void RemoveAllOverscroll();

  const VectorOf<Element>& DOMSortedElements();

  bool NeedsLayoutTreeRebuild() const { return needs_layout_tree_rebuild_; }
  void ClearNeedsLayoutTreeRebuild() { needs_layout_tree_rebuild_ = false; }

  void Trace(Visitor*) const override;

 private:
  friend class OverscrollAreaTrackerTest;

  Member<Element> container_;

  VectorOf<Element> overscroll_members_;
  bool needs_dom_sort_ = false;
  bool needs_layout_tree_rebuild_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_OVERSCROLL_OVERSCROLL_AREA_TRACKER_H_
