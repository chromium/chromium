// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_SCROLL_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_SCROLL_STATE_H_

#include <memory>
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/scroll/scroll_state_data.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class ScrollStateInit;

class CORE_EXPORT ScrollState final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ScrollState* Create(ScrollStateInit*);

  explicit ScrollState(std::unique_ptr<ScrollStateData>);
  ~ScrollState() override = default;

  // Web exposed methods.

  // Reduce deltas by x, y.
  void consumeDelta(double x, double y, ExceptionState&);
  // Pops the first element off of |m_scrollChain| and calls |distributeScroll|
  // on it.
  void distributeToScrollChainDescendant();
  int positionX() { return data_->position_x; }
  int positionY() { return data_->position_y; }
  // Positive when scrolling right.
  double deltaX() const { return data_->delta_x; }
  // Positive when scrolling down.
  double deltaY() const { return data_->delta_y; }
  // Positive when scrolling right.
  double deltaXHint() const { return data_->delta_x_hint; }
  // Positive when scrolling down.
  double deltaYHint() const { return data_->delta_y_hint; }
  // Indicates the smallest delta the input device can produce. 0 for
  // unquantized inputs. Deprecated, only exists for script binding
  // compatibility. Use delta_granularity instead for usage within Blink.
  double deltaGranularity() const {
    return static_cast<double>(data_->delta_granularity);
  }
  ui::ScrollGranularity delta_granularity() const {
    return data_->delta_granularity;
  }
  // Positive if moving right.
  double velocityX() const { return data_->velocity_x; }
  // Positive if moving down.
  double velocityY() const { return data_->velocity_y; }
  // True for events dispatched after the users's gesture has finished.
  bool inInertialPhase() const { return data_->is_in_inertial_phase; }
  // True if this is the first event for this scroll.
  bool isBeginning() const { return data_->is_beginning; }
  // True if this is the last event for this scroll.
  bool isEnding() const { return data_->is_ending; }
  // True if this scroll is the direct result of user input.
  bool fromUserInput() const { return data_->from_user_input; }
  // True if this scroll is the result of the user interacting directly with
  // the screen, e.g., via touch.
  bool isDirectManipulation() const { return data_->is_direct_manipulation; }

  // Non web exposed methods.
  void ConsumeDeltaNative(double x, double y);

  // TODO(tdresser): this needs to be web exposed. See crbug.com/483091.
  void SetScrollChain(Deque<DOMNodeId> scroll_chain) {
    scroll_chain_ = scroll_chain;
  }

  Node* CurrentNativeScrollingNode();
  void SetCurrentNativeScrollingNode(Node*);

  bool DeltaConsumedForScrollSequence() const {
    return data_->delta_consumed_for_scroll_sequence;
  }

  // Scroll begin and end must propagate to all nodes to ensure
  // their state is updated.
  bool FullyConsumed() const {
    return !data_->delta_x && !data_->delta_y && !data_->is_ending &&
           !data_->is_beginning;
  }

  ScrollStateData* Data() const { return data_.get(); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(node_);
    ScriptWrappable::Trace(visitor);
  }

 private:
  ScrollState() = delete;

  std::unique_ptr<ScrollStateData> data_;
  Deque<DOMNodeId> scroll_chain_;
  Member<Node> node_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_SCROLL_STATE_H_
