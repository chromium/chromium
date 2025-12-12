// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SCROLL_BUTTON_PSEUDO_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SCROLL_BUTTON_PSEUDO_ELEMENT_H_

#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/frame/post_layout_snapshot_client.h"

namespace blink {

class ScrollButtonPseudoElement : public PseudoElement,
                                  public PostLayoutSnapshotClient {
 public:
  ScrollButtonPseudoElement(Element* originating_element, PseudoId pseudo_id);

  bool IsScrollButtonPseudoElement() const final { return true; }

  int DefaultTabIndex() const override { return 0; }
  void DefaultEventHandler(Event&) override;
  bool HasActivationBehavior() const final { return true; }
  bool WillRespondToMouseClickEvents() override { return true; }
  Node* InnerNodeForHitTesting() final { return this; }

  bool IsEnabled() const { return enabled_; }
  bool IsDisabledFormControl() const final { return !IsEnabled(); }
  FocusableState SupportsFocus(UpdateBehavior update_behavior) const final;

  // PostLayoutSnapshotClient:
  bool UpdateSnapshot() override;
  bool ShouldScheduleNextService() override;

  void Trace(Visitor* v) const final;

 private:
  void HandleButtonActivation();

  bool enabled_ = true;
};

template <>
struct DowncastTraits<ScrollButtonPseudoElement> {
  static bool AllowFrom(const Node& node) {
    return node.IsScrollButtonPseudoElement();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SCROLL_BUTTON_PSEUDO_ELEMENT_H_
