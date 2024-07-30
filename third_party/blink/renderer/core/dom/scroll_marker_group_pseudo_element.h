// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SCROLL_MARKER_GROUP_PSEUDO_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SCROLL_MARKER_GROUP_PSEUDO_ELEMENT_H_

#include "third_party/blink/renderer/core/dom/pseudo_element.h"

namespace blink {

// Represents ::scroll-marker-group pseudo element and manages
// implicit focus group, formed by ::scroll-marker pseudo elements.
// This focus group is needed to cycle through its element with
// arrow keys.
class ScrollMarkerGroupPseudoElement : public PseudoElement {
 public:
  // pseudo_id is needed, as ::scroll-marker-group can be after or before.
  ScrollMarkerGroupPseudoElement(Element* originating_element,
                                 PseudoId pseudo_id)
      : PseudoElement(originating_element, pseudo_id) {}

  bool IsScrollMarkerGroupPseudoElement() const final { return true; }

  // TODO(332396355): Replace Element with ScrollMarker and remove CHECK.
  void AddToFocusGroup(PseudoElement& scroll_marker) {
    CHECK(scroll_marker.IsScrollMarkerPseudoElement());
    focus_group_.push_back(scroll_marker);
  }
  Element* FindFocusableElementForward(const Element& current) {
    if (wtf_size_t id = focus_group_.Find(current); id != kNotFound) {
      return focus_group_[id == focus_group_.size() - 1 ? 0u : id + 1];
    }
    return nullptr;
  }
  Element* FindFocusableElementBackward(const Element& current) {
    if (wtf_size_t id = focus_group_.Find(current); id != kNotFound) {
      return focus_group_[id == 0u ? focus_group_.size() - 1 : id - 1];
    }
    return nullptr;
  }
  void ClearFocusGroup() { focus_group_.clear(); }

  void Trace(Visitor* v) const final {
    v->Trace(focus_group_);
    PseudoElement::Trace(v);
  }

 private:
  // TODO(332396355): Add spec link, once it's created.
  HeapVector<Member<Element>> focus_group_;
};

template <>
struct DowncastTraits<ScrollMarkerGroupPseudoElement> {
  static bool AllowFrom(const Node& node) {
    return node.IsScrollMarkerGroupPseudoElement();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SCROLL_MARKER_GROUP_PSEUDO_ELEMENT_H_
