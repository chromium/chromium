// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/fling_scheduler.h"

#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/mock_render_widget_host_delegate.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_render_widget_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class FakeFlingScheduler : public FlingScheduler {
 public:
  FakeFlingScheduler(RenderWidgetHostImpl* host) : FlingScheduler(host) {}

  void ScheduleFlingProgress(
      base::WeakPtr<FlingController> fling_controller) override {
    FlingScheduler::ScheduleFlingProgress(fling_controller);
    fling_in_progress_ = true;
  }

  void DidStopFlingingOnBrowser(
      base::WeakPtr<FlingController> fling_controller) override {
    FlingScheduler::DidStopFlingingOnBrowser(fling_controller);
    fling_in_progress_ = false;
  }

  bool fling_in_progress() const { return fling_in_progress_; }

  ui::Compositor* compositor() { return GetCompositor(); }
  ui::Compositor* observed_compositor() { return observed_compositor_; }

  base::WeakPtr<FlingController> fling_controller() const {
    return fling_controller_;
  }

 private:
  bool fling_in_progress_ = false;

  DISALLOW_COPY_AND_ASSIGN(FakeFlingScheduler);
};

class FlingSchedulerTest : public testing::Test,
                           public FlingControllerEventSenderClient {
 public:
  FlingSchedulerTest() {}
  void SetUp() override {
    view_ = CreateView();
    widget_host_->SetView(view_);

    fling_scheduler_ = std::make_unique<FakeFlingScheduler>(widget_host_);
    fling_controller_ = std::make_unique<FlingController>(
        this, fling_scheduler_.get(), FlingController::Config());
  }

  void TearDown() override {
    view_->Destroy();
    widget_host_->ShutdownAndDestroyWidget(true);
    browser_context_.reset();

    base::RunLoop().RunUntilIdle();
  }

  TestRenderWidgetHostView* CreateView() {
    browser_context_ = std::make_unique<TestBrowserContext>();
    process_host_ = new MockRenderProcessHost(browser_context_.get());
    process_host_->Init();
    int32_t routing_id = process_host_->GetNextRoutingID();
    delegate_ = std::make_unique<MockRenderWidgetHostDelegate>();
    widget_host_ = TestRenderWidgetHost::Create(delegate_.get(), process_host_,
                                                routing_id, false)
                       .release();
    delegate_->set_widget_host(widget_host_);
    return new TestRenderWidgetHostView(widget_host_);
  }

  void SimulateFlingStart(const gfx::Vector2dF& velocity) {
    blink::WebGestureEvent fling_start(blink::WebInputEvent::kGestureFlingStart,
                                       0, base::TimeTicks::Now(),
                                       blink::WebGestureDevice::kTouchscreen);
    fling_start.data.fling_start.velocity_x = velocity.x();
    fling_start.data.fling_start.velocity_y = velocity.y();
    GestureEventWithLatencyInfo fling_start_with_latency(fling_start);
    fling_controller_->ObserveAndMaybeConsumeGestureEvent(
        fling_start_with_latency);
  }

  void SimulateFlingCancel() {
    blink::WebGestureEvent fling_cancel(
        blink::WebInputEvent::kGestureFlingCancel, 0, base::TimeTicks::Now(),
        blink::WebGestureDevice::kTouchscreen);
    fling_cancel.data.fling_cancel.prevent_boosting = true;
    GestureEventWithLatencyInfo fling_cancel_with_latency(fling_cancel);
    fling_controller_->ObserveAndMaybeConsumeGestureEvent(
        fling_cancel_with_latency);
  }

  // FlingControllerEventSenderClient
  void SendGeneratedWheelEvent(
      const MouseWheelEventWithLatencyInfo& wheel_event) override {}
  void SendGeneratedGestureScrollEvents(
      const GestureEventWithLatencyInfo& gesture_event) override {}
  gfx::Size GetRootWidgetViewportSize() override {
    return gfx::Size(1920, 1080);
  }

  std::unique_ptr<FlingController> fling_controller_;
  std::unique_ptr<FakeFlingScheduler> fling_scheduler_;

 private:
  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  RenderWidgetHostImpl* widget_host_;
  MockRenderProcessHost* process_host_;
  TestRenderWidgetHostView* view_;
  std::unique_ptr<MockRenderWidgetHostDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(FlingSchedulerTest);
};

TEST_F(FlingSchedulerTest, ScheduleNextFlingProgress) {
  base::TimeTicks progress_time = base::TimeTicks::Now();
  SimulateFlingStart(gfx::Vector2dF(1000, 0));
  EXPECT_TRUE(fling_scheduler_->fling_in_progress());
  EXPECT_EQ(fling_controller_.get(),
            fling_scheduler_->fling_controller().get());
  EXPECT_EQ(fling_scheduler_->compositor(),
            fling_scheduler_->observed_compositor());

  progress_time += base::TimeDelta::FromMilliseconds(17);
  fling_controller_->ProgressFling(progress_time);
  EXPECT_TRUE(fling_scheduler_->fling_in_progress());
}

TEST_F(FlingSchedulerTest, FlingCancelled) {
  SimulateFlingStart(gfx::Vector2dF(1000, 0));
  EXPECT_TRUE(fling_scheduler_->fling_in_progress());
  EXPECT_EQ(fling_controller_.get(),
            fling_scheduler_->fling_controller().get());
  EXPECT_EQ(fling_scheduler_->compositor(),
            fling_scheduler_->observed_compositor());

  SimulateFlingCancel();
  EXPECT_FALSE(fling_scheduler_->fling_in_progress());
  EXPECT_EQ(nullptr, fling_scheduler_->fling_controller());
  EXPECT_EQ(nullptr, fling_scheduler_->observed_compositor());
}

}  // namespace content
