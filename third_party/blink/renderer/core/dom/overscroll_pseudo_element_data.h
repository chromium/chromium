// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_OVERSCROLL_PSEUDO_ELEMENT_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_OVERSCROLL_PSEUDO_ELEMENT_DATA_H_

#include "base/check_op.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/indexed_pseudo_element.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

class PseudoElement;

// OverscrollPseudoElementData holds the PseudoElements constructed for the
// overscroll-area property associated with their originating element.
// In particular, when an element has `overscroll-area: --name1, --name2;`
// we will create an ::overscroll-area-parent for each of --name1 and
// --name2 which allow scrolling into those overscroll areas,
// producing the following layout tree structure:
// <div id="scroller">
//   <::overscroll-area-parent(--foo)></::overscroll-area-parent(--foo)>
//   <::overscroll-area-parent(--bar)></::overscroll-area-parent(--bar)>
//   <div id="scroller-child"></div>
// </div>

class OverscrollPseudoElementData final
    : public GarbageCollected<OverscrollPseudoElementData> {
 public:
  OverscrollPseudoElementData() = default;
  OverscrollPseudoElementData(const OverscrollPseudoElementData&) = delete;
  OverscrollPseudoElementData& operator=(const OverscrollPseudoElementData&) =
      delete;

  void AddPseudoElement(IndexedPseudoElement*);
  PseudoElement* GetPseudoElement(wtf_size_t idx) const;
  const OverscrollAreaParentPseudoElementsVector& GetOverscrollParents() const {
    return overscroll_parents_;
  }

  bool HasPseudoElements() const;
  // Remove all but the first |to_keep| overscroll parent pseudos.
  void ClearPseudoElements(wtf_size_t to_keep = 0);
  void Trace(Visitor* visitor) const { visitor->Trace(overscroll_parents_); }

  size_t size() const { return overscroll_parents_.size(); }

 private:
  OverscrollAreaParentPseudoElementsVector overscroll_parents_;
};

inline bool OverscrollPseudoElementData::HasPseudoElements() const {
  return !overscroll_parents_.empty();
}

inline void OverscrollPseudoElementData::ClearPseudoElements(
    wtf_size_t to_keep) {
  for (wtf_size_t i = to_keep; i < overscroll_parents_.size(); ++i) {
    overscroll_parents_[i]->Dispose();
  }
  if (to_keep) {
    overscroll_parents_.Shrink(to_keep);
  } else {
    overscroll_parents_.clear();
  }
}

inline void OverscrollPseudoElementData::AddPseudoElement(
    IndexedPseudoElement* element) {
  overscroll_parents_.push_back(element);
}

inline PseudoElement* OverscrollPseudoElementData::GetPseudoElement(
    wtf_size_t idx) const {
  return overscroll_parents_[idx];
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_OVERSCROLL_PSEUDO_ELEMENT_DATA_H_
