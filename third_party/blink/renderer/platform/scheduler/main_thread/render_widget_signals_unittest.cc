// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/render_widget_signals.h"

#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/widget_scheduler_impl.h"

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
  scoped_refptr<WidgetSchedulerImpl> widget1_scheduler =
      base::MakeRefCounted<WidgetSchedulerImpl>(
          /*main_thread_scheduler_impl=*/nullptr, render_widget_signals_.get());
  Mock::VerifyAndClearExpectations(mock_observer_.get());

  EXPECT_CALL(*mock_observer_, SetAllRenderWidgetsHidden(true)).Times(1);
  widget1_scheduler->Shutdown();
}

TEST_F(RenderWidgetSignalsTest, RenderWidget_Hidden) {
  IgnoreWidgetCreationCallbacks();
  scoped_refptr<WidgetSchedulerImpl> widget1_scheduler =
      base::MakeRefCounted<WidgetSchedulerImpl>(
          /*main_thread_scheduler_impl=*/nullptr, render_widget_signals_.get());
  Mock::VerifyAndClearExpectations(mock_observer_.get());

  EXPECT_CALL(*mock_observer_, SetAllRenderWidgetsHidden(true)).Times(1);
  widget1_scheduler->SetHidden(true);
  Mock::VerifyAndClearExpectations(mock_observer_.get());

  IgnoreWidgetDestructionCallbacks();
  widget1_scheduler->Shutdown();
}

TEST_F(RenderWidgetSignalsTest, RenderWidget_HiddenThreeTimesShownOnce) {
  IgnoreWidgetCreationCallbacks();
  scoped_refptr<WidgetSchedulerImpl> widget1_scheduler =
      base::MakeRefCounted<WidgetSchedulerImpl>(
          /*main_thread_scheduler_impl=*/nullptr, render_widget_signals_.get());
  Mock::VerifyAndClearExpectations(mock_observer_.get());

  EXPECT_CALL(*mock_observer_, SetAllRenderWidgetsHidden(true)).Times(1);
  widget1_scheduler->SetHidden(true);
  widget1_scheduler->SetHidden(true);
  widget1_scheduler->SetHidden(true);
  Mock::VerifyAndClearExpectations(mock_observer_.get());

  EXPECT_CALL(*mock_observer_, SetAllRenderWidgetsHidden(false)).Times(1);
  widget1_scheduler->SetHidden(false);
  Mock::VerifyAndClearExpectations(mock_observer_.get());

  IgnoreWidgetDestructionCallbacks();
  widget1_scheduler->Shutdown();
}

TEST_F(RenderWidgetSignalsTest, MultipleRenderWidgetsBecomeHiddenThenVisible) {
  IgnoreWidgetCreationCallbacks();
  scoped_refptr<WidgetSchedulerImpl> widget1_scheduler =
      base::MakeRefCounted<WidgetSchedulerImpl>(
          /*main_thread_scheduler_impl=*/nullptr, render_widget_signals_.get());
  scoped_refptr<WidgetSchedulerImpl> widget2_scheduler =
      base::MakeRefCounted<WidgetSchedulerImpl>(
          /*main_thread_scheduler_impl=*/nullptr, render_widget_signals_.get());
  scoped_refptr<WidgetSchedulerImpl> widget3_scheduler =
      base::MakeRefCounted<WidgetSchedulerImpl>(
          /*main_thread_scheduler_impl=*/nullptr, render_widget_signals_.get());
  Mock::VerifyAndClearExpectations(mock_observer_.get());

  // Widgets are initially assumed to be visible so start hiding them, we should
  // not get any calls to SetAllRenderWidgetsHidden till the last one is hidden.
  EXPECT_CALL(*mock_observer_, SetAllRenderWidgetsHidden(_)).Times(0);
  widget1_scheduler->SetHidden(true);
  widget2_scheduler->SetHidden(true);
  Mock::VerifyAndClearExpectations(mock_observer_.get());

  EXPECT_CALL(*mock_observer_, SetAllRenderWidgetsHidden(true)).Times(1);
  widget3_scheduler->SetHidden(true);
  Mock::VerifyAndClearExpectations(mock_observer_.get());

  // We should get a call back once the first widget is unhidden and no more
  // after that.
  EXPECT_CALL(*mock_observer_, SetAllRenderWidgetsHidden(false)).Times(1);
  widget1_scheduler->SetHidden(false);
  Mock::VerifyAndClearExpectations(mock_observer_.get());

  EXPECT_CALL(*mock_observer_, SetAllRenderWidgetsHidden(_)).Times(0);
  widget2_scheduler->SetHidden(false);
  widget3_scheduler->SetHidden(false);
  Mock::VerifyAndClearExpectations(mock_observer_.get());

  IgnoreWidgetDestructionCallbacks();
  widget1_scheduler->Shutdown();
  widget2_scheduler->Shutdown();
  widget3_scheduler->Shutdown();
}

}  // namespace render_widget_signals_unittest
}  // namespace scheduler
}  // namespace blink
