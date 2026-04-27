// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_INTEREST_BUTTON_PSEUDO_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_INTEREST_BUTTON_PSEUDO_ELEMENT_H_

#include "third_party/blink/renderer/core/dom/pseudo_element.h"

namespace blink {

class InterestButtonPseudoElement : public PseudoElement {
 public:
  InterestButtonPseudoElement(Element* originating_element, PseudoId pseudo_id);

  bool IsInterestButtonPseudoElement() const final { return true; }

  int DefaultTabIndex() const override { return 0; }
  void DefaultEventHandler(Event&) override;
  bool HasActivationBehavior() const final { return true; }
  bool WillRespondToMouseClickEvents() override { return true; }

  FocusableState SupportsFocus(UpdateBehavior update_behavior) const final;

 private:
  void HandleButtonActivation();
};

template <>
struct DowncastTraits<InterestButtonPseudoElement> {
  static bool AllowFrom(const Node& node) {
    return node.IsInterestButtonPseudoElement();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_INTEREST_BUTTON_PSEUDO_ELEMENT_H_
