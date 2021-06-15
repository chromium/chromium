// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/render_widget_signals.h"

#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/web_render_widget_scheduling_state.h"

using testing::AnyNumber;
using testing::Mock;
using testing::_;

namespace blink {
namespace scheduler {
// To avoid symbol collisions in jumbo builds.
namespace render_widget_signals_unittest {

class MockObserver : public RenderWidgetSignals::Observer {
 public:
  MockObserver() = default;
  MockObserver(const MockObserver&) = delete;
  MockObserver& operator=(const MockObserver&) = delete;
  ~MockObserver() override = default;

  MOCK_METHOD1(SetAllRenderWidgetsHidden, void(bool hidden));
  MOCK_METHOD1(SetHasVisibleRenderWidgetWithTouchHandler,
               void(bool has_visible_render_widget_with_touch_handler));
};

class RenderWidgetSignalsTest : public testing::Test {
 public:
  RenderWidgetSignalsTest() = default;
  ~RenderWidgetSignalsTest() override = default;

  void SetUp() override {
    mock_observer_ = std::make_unique<MockObserver>();
    render_widget_signals_ =
        std::make_unique<RenderWidgetSignals>(mock_observer_.get());
  }

  void IgnoreWidgetCreationCallbacks() {
    EXPECT_CALL(*mock_observer_, SetAllRenderWidgetsHidden(false))
        .Times(AnyNumber());
  }

  void IgnoreWidgetDestructionCallbacks() {
    EXPECT_CALL(*mock_observer_, SetAllRenderWidgetsHidden(true))
        .Times(AnyNumber());
  }

  std::unique_ptr<MockObserver> mock_observer_;
  std::unique_ptr<RenderWidgetSignals> render_widget_signals_;
};

TEST_F(RenderWidgetSignalsTest, RenderWidgetSchedulingStateLifeCycle) {
  EXPECT_CALL(*mock_observer_, SetAllRenderWidgetsHidden(false)).Times(1);
  std::unique_ptr<WebRenderWidgetSchedulingState> widget1_state =
      render_widget_signals_->NewRenderWidgetSchedulingState();
  Mock::VerifyAndClearExpectations(mock_observer_.get());

  EXPECT_CALL(*mock_observer_, SetAllRenderWidgetsHidden(true)).Times(1);
}

TEST_F(RenderWidgetSignalsTest, RenderWidget_Hidden) {
  IgnoreWidgetCreationCallbacks();
  std::unique_ptr<WebRenderWidgetSchedulingState> widget1_state =
      render_widget_signals_->NewRenderWidgetSchedulingState();
  Mock::VerifyAndClearExpectations(mock_observer_.get());

  EXPECT_CALL(*mock_observer_, SetAllRenderWidgetsHidden(true)).Times(1);
  widget1_state->SetHidden(true);
  Mock::VerifyAndClearExpectations(mock_observer_.get());

  IgnoreWidgetDestructionCallbacks();
}

TEST_F(RenderWidgetSignalsTest, RenderWidget_HiddenThreeTimesShownOnce) {
  IgnoreWidgetCreationCallbacks();
  std::unique_ptr<WebRenderWidgetSchedulingState> widget1_state =
      render_widget_signals_->NewRenderWidgetSchedulingState();
  Mock::VerifyAndClearExpectations(mock_observer_.get());

  EXPECT_CALL(*mock_observer_, SetAllRenderWidgetsHidden(true)).Times(1);
  widget1_state->SetHidden(true);
  widget1_state->SetHidden(true);
  widget1_state->SetHidden(true);
  Mock::VerifyAndClearExpectations(mock_observer_.get());

  EXPECT_CALL(*mock_observer_, SetAllRenderWidgetsHidden(false)).Times(1);
  widget1_state->SetHidden(false);
  Mock::VerifyAndClearExpectations(mock_observer_.get());

  IgnoreWidgetDestructionCallbacks();
}

TEST_F(RenderWidgetSignalsTest, MultipleRenderWidgetsBecomeHiddenThenVisible) {
  IgnoreWidgetCreationCallbacks();
  std::unique_ptr<WebRenderWidgetSchedulingState> widget1_state =
      render_widget_signals_->NewRenderWidgetSchedulingState();
  std::unique_ptr<WebRenderWidgetSchedulingState> widget2_state =
      render_widget_signals_->NewRenderWidgetSchedulingState();
  std::unique_ptr<WebRenderWidgetSchedulingState> widget3_state =
      render_widget_signals_->NewRenderWidgetSchedulingState();
  Mock::VerifyAndClearExpectations(mock_observer_.get());

  // Widgets are initially assumed to be visible so start hiding them, we should
  // not get any calls to SetAllRenderWidgetsHidden till the last one is hidden.
  EXPECT_CALL(*mock_observer_, SetAllRenderWidgetsHidden(_)).Times(0);
  widget1_state->SetHidden(true);
  widget2_state->SetHidden(true);
  Mock::VerifyAndClearExpectations(mock_observer_.get());

  EXPECT_CALL(*mock_observer_, SetAllRenderWidgetsHidden(true)).Times(1);
  widget3_state->SetHidden(true);
  Mock::VerifyAndClearExpectations(mock_observer_.get());

  // We should get a call back once the first widget is unhidden and no more
  // after that.
  EXPECT_CALL(*mock_observer_, SetAllRenderWidgetsHidden(false)).Times(1);
  widget1_state->SetHidden(false);
  Mock::VerifyAndClearExpectations(mock_observer_.get());

  EXPECT_CALL(*mock_observer_, SetAllRenderWidgetsHidden(_)).Times(0);
  widget2_state->SetHidden(false);
  widget3_state->SetHidden(false);
  Mock::VerifyAndClearExpectations(mock_observer_.get());

  IgnoreWidgetDestructionCallbacks();
}

TEST_F(RenderWidgetSignalsTest, TouchHandlerAddedAndRemoved_VisibleWidget) {
  IgnoreWidgetCreationCallbacks();

  std::unique_ptr<WebRenderWidgetSchedulingState> widget_state =
      render_widget_signals_->NewRenderWidgetSchedulingState();
  Mock::VerifyAndClearExpectations(mock_observer_.get());

  EXPECT_CALL(*mock_observer_, SetHasVisibleRenderWidgetWithTouchHandler(true))
      .Times(1);
  widget_state->SetHasTouchHandler(true);
  Mock::VerifyAndClearExpectations(mock_observer_.get());

  EXPECT_CALL(*mock_observer_, SetHasVisibleRenderWidgetWithTouchHandler(false))
      .Times(1);
  widget_state->SetHasTouchHandler(false);
  Mock::VerifyAndClearExpectations(mock_observer_.get());

  IgnoreWidgetDestructionCallbacks();
}

TEST_F(RenderWidgetSignalsTest,
       TouchHandlerAddedThriceAndRemovedOnce_VisibleWidget) {
  IgnoreWidgetCreationCallbacks();

  std::unique_ptr<WebRenderWidgetSchedulingState> widget_state =
      render_widget_signals_->NewRenderWidgetSchedulingState();
  Mock::VerifyAndClearExpectations(mock_observer_.get());

  EXPECT_CALL(*mock_observer_, SetHasVisibleRenderWidgetWithTouchHandler(true))
      .Times(1);
  widget_state->SetHasTouchHandler(true);
  widget_state->SetHasTouchHandler(true);
  widget_state->SetHasTouchHandler(true);
  Mock::VerifyAndClearExpectations(mock_observer_.get());

  EXPECT_CALL(*mock_observer_, SetHasVisibleRenderWidgetWithTouchHandler(false))
      .Times(1);
  widget_state->SetHasTouchHandler(false);
  Mock::VerifyAndClearExpectations(mock_observer_.get());

  IgnoreWidgetDestructionCallbacks();
}

TEST_F(RenderWidgetSignalsTest, TouchHandlerAddedAndRemoved_HiddenWidget) {
  IgnoreWidgetCreationCallbacks();

  std::unique_ptr<WebRenderWidgetSchedulingState> widget_state =
      render_widget_signals_->NewRenderWidgetSchedulingState();
  EXPECT_CALL(*mock_observer_, SetAllRenderWidgetsHidden(true)).Times(1);
  widget_state->SetHidden(true);
  Mock::VerifyAndClearExpectations(mock_observer_.get());

  EXPECT_CALL(*mock_observer_, SetHasVisibleRenderWidgetWithTouchHandler(_))
      .Times(0);
  widget_state->SetHasTouchHandler(true);
  widget_state->SetHasTouchHandler(false);
  Mock::VerifyAndClearExpectations(mock_observer_.get());

  IgnoreWidgetDestructionCallbacks();
}

TEST_F(RenderWidgetSignalsTest,
       MultipleTouchHandlerAddedAndRemoved_VisibleWidgets) {
  IgnoreWidgetCreationCallbacks();

  std::unique_ptr<WebRenderWidgetSchedulingState> widget1_state =
      render_widget_signals_->NewRenderWidgetSchedulingState();
  std::unique_ptr<WebRenderWidgetSchedulingState> widget2_state =
      render_widget_signals_->NewRenderWidgetSchedulingState();
  std::unique_ptr<WebRenderWidgetSchedulingState> widget3_state =
      render_widget_signals_->NewRenderWidgetSchedulingState();
  Mock::VerifyAndClearExpectations(mock_observer_.get());

  // We should only get a callback for the first widget with a touch handler.
  EXPECT_CALL(*mock_observer_, SetHasVisibleRenderWidgetWithTouchHandler(true))
      .Times(1);
  widget1_state->SetHasTouchHandler(true);
  Mock::VerifyAndClearExpectations(mock_observer_.get());

  EXPECT_CALL(*mock_observer_, SetHasVisibleRenderWidgetWithTouchHandler(_))
      .Times(0);
  widget2_state->SetHasTouchHandler(true);
  widget3_state->SetHasTouchHandler(true);
  Mock::VerifyAndClearExpectations(mock_observer_.get());

  // We should only get a callback when the last touch handler is removed.
  EXPECT_CALL(*mock_observer_, SetHasVisibleRenderWidgetWithTouchHandler(_))
      .Times(0);
  widget1_state->SetHasTouchHandler(false);
  widget2_state->SetHasTouchHandler(false);
  Mock::VerifyAndClearExpectations(mock_observer_.get());

  EXPECT_CALL(*mock_observer_, SetHasVisibleRenderWidgetWithTouchHandler(false))
      .Times(1);
  widget3_state->SetHasTouchHandler(false);
  Mock::VerifyAndClearExpectations(mock_observer_.get());

  IgnoreWidgetDestructionCallbacks();
}

TEST_F(RenderWidgetSignalsTest,
       TouchHandlerAddedThenWigetDeleted_VisibleWidget) {
  IgnoreWidgetCreationCallbacks();

  std::unique_ptr<WebRenderWidgetSchedulingState> widget_state =
      render_widget_signals_->NewRenderWidgetSchedulingState();
  Mock::VerifyAndClearExpectations(mock_observer_.get());

  EXPECT_CALL(*mock_observer_, SetHasVisibleRenderWidgetWithTouchHandler(true))
      .Times(1);
  widget_state->SetHasTouchHandler(true);
  Mock::VerifyAndClearExpectations(mock_observer_.get());

  EXPECT_CALL(*mock_observer_, SetHasVisibleRenderWidgetWithTouchHandler(false))
      .Times(1);
  IgnoreWidgetDestructionCallbacks();
}

TEST_F(RenderWidgetSignalsTest,
       TouchHandlerAddedThenWigetDeleted_HiddenWidget) {
  IgnoreWidgetCreationCallbacks();

  std::unique_ptr<WebRenderWidgetSchedulingState> widget_state =
      render_widget_signals_->NewRenderWidgetSchedulingState();
  EXPECT_CALL(*mock_observer_, SetAllRenderWidgetsHidden(true)).Times(1);
  widget_state->SetHidden(true);
  Mock::VerifyAndClearExpectations(mock_observer_.get());

  EXPECT_CALL(*mock_observer_, SetHasVisibleRenderWidgetWithTouchHandler(_))
      .Times(0);
  IgnoreWidgetDestructionCallbacks();
}

}  // namespace render_widget_signals_unittest
}  // namespace scheduler
}  // namespace blink
