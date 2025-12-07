// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_OVERSCROLL_PSEUDO_ELEMENT_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_OVERSCROLL_PSEUDO_ELEMENT_DATA_H_

#include "base/check_op.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

class PseudoElement;

// OverscrollPseudoElementData holds the PseudoElements constructed for the
// overscroll-area property associated with their originating element.
// In particular, when an element has `overscroll-area: --name1, --name2;`
// we will create an ::overscroll-area-parent for each of --name1 and
// --name2 which allow scrolling into those overscroll areas, and a single
// ::overscroll-client-area for the originating element's content,
// producing the following layout tree structure:
// <div id="scroller">
//   <::overscroll-area-parent(--foo)>
//     <::overscroll-area-parent(--bar)>
//       <::overscroll-client-area>
//         <div id="scroller-child"></div>
//       </::overscroll-client-area>
//     </::overscroll-area-parent(--bar)>
//   </::overscroll-area-parent(--foo)>
// </div>

class OverscrollPseudoElementData final
    : public GarbageCollected<OverscrollPseudoElementData> {
 public:
  OverscrollPseudoElementData() = default;
  OverscrollPseudoElementData(const OverscrollPseudoElementData&) = delete;
  OverscrollPseudoElementData& operator=(const OverscrollPseudoElementData&) =
      delete;

  void AddPseudoElement(PseudoId,
                        PseudoElement*,
                        const AtomicString& overscroll_area_name = g_null_atom);
  PseudoElement* GetPseudoElement(
      PseudoId,
      const AtomicString& overscroll_area_name = g_null_atom) const;
  const HeapVector<Member<PseudoElement>>& GetOverscrollParents() const {
    return overscroll_parents_;
  }

  bool HasPseudoElements() const;
  void ClearPseudoElements();
  void Trace(Visitor* visitor) const {
    visitor->Trace(overscroll_client_area_);
    visitor->Trace(overscroll_parents_);
  }

  size_t size() const { return overscroll_parents_.size(); }

 private:
  Member<PseudoElement> overscroll_client_area_;
  HeapVector<Member<PseudoElement>> overscroll_parents_;
  HashMap<AtomicString, size_t> overscroll_parent_id_map_;
};

inline bool OverscrollPseudoElementData::HasPseudoElements() const {
  return overscroll_client_area_ || !overscroll_parents_.empty();
}

inline void OverscrollPseudoElementData::ClearPseudoElements() {
  overscroll_client_area_ = nullptr;
  for (PseudoElement* pseudo : overscroll_parents_) {
    pseudo->Dispose();
  }
  overscroll_parents_.clear();
  overscroll_parent_id_map_.clear();
}

inline void OverscrollPseudoElementData::AddPseudoElement(
    PseudoId pseudo_id,
    PseudoElement* element,
    const AtomicString& overscroll_area_name) {
  switch (pseudo_id) {
    case kPseudoIdOverscrollClientArea:
      CHECK(!overscroll_client_area_);
      overscroll_client_area_ = element;
      break;
    case kPseudoIdOverscrollAreaParent: {
      DCHECK(overscroll_area_name);
      overscroll_parents_.push_back(element);
      auto result = overscroll_parent_id_map_.insert(
          overscroll_area_name, overscroll_parents_.size() - 1);
      CHECK(result.is_new_entry);
      break;
    }
    default:
      NOTREACHED();
  }
}

inline PseudoElement* OverscrollPseudoElementData::GetPseudoElement(
    PseudoId pseudo_id,
    const AtomicString& overscroll_area_name) const {
  switch (pseudo_id) {
    case kPseudoIdOverscrollClientArea:
      return overscroll_client_area_.Get();
    case kPseudoIdOverscrollAreaParent: {
      auto it = overscroll_parent_id_map_.find(overscroll_area_name);
      if (it == overscroll_parent_id_map_.end()) {
        return nullptr;
      }
      return overscroll_parents_[it->value];
    }
    default:
      NOTREACHED();
  }
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_OVERSCROLL_PSEUDO_ELEMENT_DATA_H_
