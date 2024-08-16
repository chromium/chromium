// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SCROLL_BUTTON_PSEUDO_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SCROLL_BUTTON_PSEUDO_ELEMENT_H_

#include "third_party/blink/renderer/core/dom/pseudo_element.h"

namespace blink {

class ScrollMarkerGroupPseudoElement;

class ScrollButtonPseudoElement : public PseudoElement {
 public:
  ScrollButtonPseudoElement(Element* originating_element, PseudoId pseudo_id)
      : PseudoElement(originating_element, pseudo_id) {
    SetTabIndexExplicitly();
  }

  bool IsScrollButtonPseudoElement() const final { return true; }

  int DefaultTabIndex() const override { return 0; }
  void DefaultEventHandler(Event&) override;
  bool HasActivationBehavior() const final { return true; }
  bool WillRespondToMouseClickEvents() override { return true; }
  Node* InnerNodeForHitTesting() final { return this; }
  void SetScrollMarkerGroup(
      ScrollMarkerGroupPseudoElement* scroll_marker_group) {
    scroll_marker_group_ = scroll_marker_group;
  }
  ScrollMarkerGroupPseudoElement* ScrollMarkerGroup() const {
    return scroll_marker_group_;
  }

  void Trace(Visitor* v) const final;

 private:
  WeakMember<ScrollMarkerGroupPseudoElement> scroll_marker_group_;
};

template <>
struct DowncastTraits<ScrollButtonPseudoElement> {
  static bool AllowFrom(const Node& node) {
    return node.IsScrollButtonPseudoElement();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SCROLL_BUTTON_PSEUDO_ELEMENT_H_
