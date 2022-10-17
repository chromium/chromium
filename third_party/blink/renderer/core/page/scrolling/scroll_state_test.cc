// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/scroll_state.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

ScrollState* CreateScrollState(double delta_x,
                               double delta_y,
                               bool beginning,
                               bool ending) {
  std::unique_ptr<ScrollStateData> scroll_state_data =
      std::make_unique<ScrollStateData>();
  scroll_state_data->delta_x = delta_x;
  scroll_state_data->delta_y = delta_y;
  scroll_state_data->is_beginning = beginning;
  scroll_state_data->is_ending = ending;
  return MakeGarbageCollected<ScrollState>(std::move(scroll_state_data));
}

class ScrollStateTest : public testing::Test {};

TEST_F(ScrollStateTest, ConsumeDeltaNative) {
  const float kDeltaX = 12.3;
  const float kDeltaY = 3.9;

  const float kDeltaXToConsume = 1.2;
  const float kDeltaYToConsume = 2.3;

  ScrollState* scroll_state = CreateScrollState(kDeltaX, kDeltaY, false, false);
  EXPECT_FLOAT_EQ(kDeltaX, scroll_state->deltaX());
  EXPECT_FLOAT_EQ(kDeltaY, scroll_state->deltaY());
  EXPECT_FALSE(scroll_state->DeltaConsumedForScrollSequence());
  EXPECT_FALSE(scroll_state->FullyConsumed());

  scroll_state->ConsumeDeltaNative(0, 0);
  EXPECT_FLOAT_EQ(kDeltaX, scroll_state->deltaX());
  EXPECT_FLOAT_EQ(kDeltaY, scroll_state->deltaY());
  EXPECT_FALSE(scroll_state->DeltaConsumedForScrollSequence());
  EXPECT_FALSE(scroll_state->FullyConsumed());

  scroll_state->ConsumeDeltaNative(kDeltaXToConsume, 0);
  EXPECT_FLOAT_EQ(kDeltaX - kDeltaXToConsume, scroll_state->deltaX());
  EXPECT_FLOAT_EQ(kDeltaY, scroll_state->deltaY());
  EXPECT_TRUE(scroll_state->DeltaConsumedForScrollSequence());
  EXPECT_FALSE(scroll_state->FullyConsumed());

  scroll_state->ConsumeDeltaNative(0, kDeltaYToConsume);
  EXPECT_FLOAT_EQ(kDeltaX - kDeltaXToConsume, scroll_state->deltaX());
  EXPECT_FLOAT_EQ(kDeltaY - kDeltaYToConsume, scroll_state->deltaY());
  EXPECT_TRUE(scroll_state->DeltaConsumedForScrollSequence());
  EXPECT_FALSE(scroll_state->FullyConsumed());

  scroll_state->ConsumeDeltaNative(scroll_state->deltaX(),
                                   scroll_state->deltaY());
  EXPECT_TRUE(scroll_state->DeltaConsumedForScrollSequence());
  EXPECT_TRUE(scroll_state->FullyConsumed());
}

TEST_F(ScrollStateTest, CurrentNativeScrollingElement) {
  ScrollState* scroll_state = CreateScrollState(0, 0, false, false);
  ScopedNullExecutionContext execution_context;
  auto* element = MakeGarbageCollected<Element>(
      QualifiedName::Null(),
      Document::CreateForTest(execution_context.GetExecutionContext()));
  scroll_state->SetCurrentNativeScrollingNode(element);

  EXPECT_EQ(element, scroll_state->CurrentNativeScrollingNode());
}

TEST_F(ScrollStateTest, FullyConsumed) {
  ScrollState* scroll_state_begin = CreateScrollState(0, 0, true, false);
  ScrollState* scroll_state = CreateScrollState(0, 0, false, false);
  ScrollState* scroll_state_end = CreateScrollState(0, 0, false, true);
  EXPECT_FALSE(scroll_state_begin->FullyConsumed());
  EXPECT_TRUE(scroll_state->FullyConsumed());
  EXPECT_FALSE(scroll_state_end->FullyConsumed());
}

}  // namespace

}  // namespace blink
