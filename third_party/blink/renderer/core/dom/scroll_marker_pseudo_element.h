// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SCROLL_MARKER_PSEUDO_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SCROLL_MARKER_PSEUDO_ELEMENT_H_

#include "third_party/blink/renderer/core/dom/pseudo_element.h"

namespace blink {

class ScrollMarkerGroupPseudoElement;

class ScrollMarkerPseudoElement : public PseudoElement {
 public:
  explicit ScrollMarkerPseudoElement(Element* originating_element)
      : PseudoElement(originating_element, kPseudoIdScrollMarker) {
    SetTabIndexExplicitly();
  }

  bool IsScrollMarkerPseudoElement() const final { return true; }

  void SetSelected(bool value);
  bool IsSelected() const { return is_selected_; }
  int DefaultTabIndex() const override { return 0; }
  FocusableState SupportsFocus(UpdateBehavior) const final;
  void DefaultEventHandler(Event&) override;
  bool HasActivationBehavior() const final { return true; }
  bool WillRespondToMouseClickEvents() override { return true; }
  Node* InnerNodeForHitTesting() final { return this; }
  void SetScrollMarkerGroup(
      ScrollMarkerGroupPseudoElement* scroll_marker_group);
  ScrollMarkerGroupPseudoElement* ScrollMarkerGroup() const {
    return scroll_marker_group_;
  }

  void Dispose() final;
  void Trace(Visitor* v) const final;

 private:
  bool is_selected_ = false;
  WeakMember<ScrollMarkerGroupPseudoElement> scroll_marker_group_;
};

template <>
struct DowncastTraits<ScrollMarkerPseudoElement> {
  static bool AllowFrom(const Node& node) {
    return node.IsScrollMarkerPseudoElement();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SCROLL_MARKER_PSEUDO_ELEMENT_H_
