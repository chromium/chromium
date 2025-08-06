// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_INTEREST_HINT_PSEUDO_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_INTEREST_HINT_PSEUDO_ELEMENT_H_

#include "third_party/blink/renderer/core/dom/pseudo_element.h"

namespace blink {

class InterestHintPseudoElement : public PseudoElement {
 public:
  InterestHintPseudoElement(Element* originating_element, PseudoId pseudo_id);

  bool IsInterestHintPseudoElement() const final { return true; }

  int DefaultTabIndex() const override { return 0; }
  void DefaultEventHandler(Event&) override;
  bool HasActivationBehavior() const final { return true; }
  bool WillRespondToMouseClickEvents() override { return true; }
  Node* InnerNodeForHitTesting() final { return this; }

  FocusableState SupportsFocus(UpdateBehavior update_behavior) const final;

 private:
  void HandleButtonActivation();
};

template <>
struct DowncastTraits<InterestHintPseudoElement> {
  static bool AllowFrom(const Node& node) {
    return node.IsInterestHintPseudoElement();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_INTEREST_HINT_PSEUDO_ELEMENT_H_
