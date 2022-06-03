// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/scroll_state.h"

#include <memory>

#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_state_init.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"

namespace blink {

namespace {

Node* NodeForId(DOMNodeId node_id) {
  Node* node = DOMNodeIds::NodeForId(node_id);
  DCHECK(node);
  return node;
}

}  // namespace

ScrollState* ScrollState::Create(ScrollStateInit* init) {
  std::unique_ptr<ScrollStateData> scroll_state_data =
      std::make_unique<ScrollStateData>();
  scroll_state_data->delta_x = init->deltaX();
  scroll_state_data->delta_y = init->deltaY();
  scroll_state_data->delta_x_hint = init->deltaXHint();
  scroll_state_data->delta_y_hint = init->deltaYHint();
  scroll_state_data->position_x = init->positionX();
  scroll_state_data->position_y = init->positionY();
  scroll_state_data->velocity_x = init->velocityX();
  scroll_state_data->velocity_y = init->velocityY();
  scroll_state_data->is_beginning = init->isBeginning();
  scroll_state_data->is_in_inertial_phase = init->isInInertialPhase();
  scroll_state_data->is_ending = init->isEnding();
  scroll_state_data->from_user_input = init->fromUserInput();
  scroll_state_data->is_direct_manipulation = init->isDirectManipulation();
  scroll_state_data->delta_granularity =
      static_cast<ScrollGranularity>(init->deltaGranularity());
  ScrollState* scroll_state =
      MakeGarbageCollected<ScrollState>(std::move(scroll_state_data));
  return scroll_state;
}

ScrollState::ScrollState(std::unique_ptr<ScrollStateData> data)
    : data_(std::move(data)) {}

void ScrollState::consumeDelta(double x,
                               double y,
                               ExceptionState& exception_state) {
  if ((data_->delta_x > 0 && 0 > x) || (data_->delta_x < 0 && 0 < x) ||
      (data_->delta_y > 0 && 0 > y) || (data_->delta_y < 0 && 0 < y)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidModificationError,
        "Can't increase delta using consumeDelta");
    return;
  }
  if (fabs(x) > fabs(data_->delta_x) || fabs(y) > fabs(data_->delta_y)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidModificationError,
        "Can't change direction of delta using consumeDelta");
    return;
  }
  ConsumeDeltaNative(x, y);
}

void ScrollState::distributeToScrollChainDescendant() {
  if (!scroll_chain_.IsEmpty()) {
    DOMNodeId descendant_id = scroll_chain_.TakeFirst();
    NodeForId(descendant_id)->CallDistributeScroll(*this);
  }
}

void ScrollState::ConsumeDeltaNative(double x, double y) {
  data_->delta_x -= x;
  data_->delta_y -= y;

  if (x)
    data_->caused_scroll_x = true;
  if (y)
    data_->caused_scroll_y = true;
  if (x || y)
    data_->delta_consumed_for_scroll_sequence = true;
}

Node* ScrollState::CurrentNativeScrollingNode() {
  if (data_->current_native_scrolling_element() == CompositorElementId()) {
    node_.Clear();
    return nullptr;
  }
  return node_;
}

void ScrollState::SetCurrentNativeScrollingNode(Node* node) {
  node_ = node;
  data_->set_current_native_scrolling_element(
      CompositorElementIdFromDOMNodeId(DOMNodeIds::IdForNode(node)));
}

}  // namespace blink
