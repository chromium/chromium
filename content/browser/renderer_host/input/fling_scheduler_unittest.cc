// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/fling_scheduler.h"

#include "build/build_config.h"
#include "content/browser/renderer_host/agent_scheduling_group_host.h"
#include "content/browser/site_instance_group.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/mock_render_widget_host_delegate.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_render_widget_host.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "ui/display/win/test/scoped_screen_win.h"
#elif BUILDFLAG(IS_CHROMEOS)
#include "ui/display/test/test_screen.h"
#endif

namespace content {

class FakeFlingScheduler : public FlingScheduler {
 public:
  FakeFlingScheduler(RenderWidgetHostImpl* host) : FlingScheduler(host) {}

  FakeFlingScheduler(const FakeFlingScheduler&) = delete;
  FakeFlingScheduler& operator=(const FakeFlingScheduler&) = delete;

  void ScheduleFlingProgress(
      base::WeakPtr<input::FlingController> fling_controller) override {
    FlingScheduler::ScheduleFlingProgress(fling_controller);
    fling_in_progress_ = true;
  }

  void DidStopFlingingOnBrowser(
      base::WeakPtr<input::FlingController> fling_controller) override {
    FlingScheduler::DidStopFlingingOnBrowser(fling_controller);
    fling_in_progress_ = false;
  }

  bool fling_in_progress() const { return fling_in_progress_; }

  ui::Compositor* compositor() { return GetCompositor(); }
  ui::Compositor* observed_compositor() { return observed_compositor_; }

  base::WeakPtr<input::FlingController> fling_controller() const {
    return fling_controller_;
  }

 private:
  bool fling_in_progress_ = false;
};

class FlingSchedulerTest : public testing::Test,
                           public input::FlingControllerEventSenderClient {
 public:
  FlingSchedulerTest() {}

  FlingSchedulerTest(const FlingSchedulerTest&) = delete;
  FlingSchedulerTest& operator=(const FlingSchedulerTest&) = delete;

  void SetUp() override {
    view_ = CreateView();
    widget_host_->SetView(view_.get());

    fling_scheduler_ = std::make_unique<FakeFlingScheduler>(widget_host_.get());
    fling_controller_ = std::make_unique<input::FlingController>(
        this, fling_scheduler_.get(), input::FlingController::Config());
  }

  void TearDown() override {
    fling_controller_.reset();
    fling_scheduler_.reset();
    view_.release()->Destroy();  // 'delete this' is called internally.
    widget_host_->ShutdownAndDestroyWidget(false);
    widget_host_.reset();
    process_host_->Cleanup();
    site_instance_group_.reset();
    process_host_.reset();
    browser_context_.reset();

    base::RunLoop().RunUntilIdle();
  }

  void SimulateFlingStart(const gfx::Vector2dF& velocity) {
    blink::WebGestureEvent fling_start(
        blink::WebInputEvent::Type::kGestureFlingStart, 0,
        base::TimeTicks::Now(), blink::WebGestureDevice::kTouchscreen);
    fling_start.data.fling_start.velocity_x = velocity.x();
    fling_start.data.fling_start.velocity_y = velocity.y();
    input::GestureEventWithLatencyInfo fling_start_with_latency(fling_start);
    fling_controller_->ObserveAndMaybeConsumeGestureEvent(
        fling_start_with_latency);
  }

  void SimulateFlingCancel() {
    blink::WebGestureEvent fling_cancel(
        blink::WebInputEvent::Type::kGestureFlingCancel, 0,
        base::TimeTicks::Now(), blink::WebGestureDevice::kTouchscreen);
    fling_cancel.data.fling_cancel.prevent_boosting = true;
    input::GestureEventWithLatencyInfo fling_cancel_with_latency(fling_cancel);
    fling_controller_->ObserveAndMaybeConsumeGestureEvent(
        fling_cancel_with_latency);
  }

  // FlingControllerEventSenderClient
  void SendGeneratedWheelEvent(
      const input::MouseWheelEventWithLatencyInfo& wheel_event) override {}
  void SendGeneratedGestureScrollEvents(
      const input::GestureEventWithLatencyInfo& gesture_event) override {}
  gfx::Size GetRootWidgetViewportSize() override {
    return gfx::Size(1920, 1080);
  }

  input::FlingController* fling_controller() { return fling_controller_.get(); }
  FakeFlingScheduler* fling_scheduler() { return fling_scheduler_.get(); }

 private:
  std::unique_ptr<TestRenderWidgetHostView> CreateView() {
    browser_context_ = std::make_unique<TestBrowserContext>();
    process_host_ =
        std::make_unique<MockRenderProcessHost>(browser_context_.get());
    process_host_->Init();
    site_instance_group_ =
        base::WrapRefCounted(SiteInstanceGroup::CreateForTesting(
            browser_context_.get(), process_host_.get()));
    int32_t routing_id = process_host_->GetNextRoutingID();
    delegate_ = std::make_unique<MockRenderWidgetHostDelegate>();
    widget_host_ = TestRenderWidgetHost::Create(
        /* frame_tree= */ nullptr, delegate_.get(),
        RenderWidgetHostImpl::DefaultFrameSinkId(*site_instance_group_,
                                                 routing_id),
        site_instance_group_->GetSafeRef(), routing_id, /* hidden= */ false,
        /* renderer_initiated_creation= */ false);
    delegate_->set_widget_host(widget_host_.get());
    return std::make_unique<TestRenderWidgetHostView>(widget_host_.get());
  }

  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<RenderWidgetHostImpl> widget_host_;
  std::unique_ptr<FakeFlingScheduler> fling_scheduler_;
  std::unique_ptr<input::FlingController> fling_controller_;
  std::unique_ptr<MockRenderProcessHost> process_host_;
  scoped_refptr<SiteInstanceGroup> site_instance_group_;
  std::unique_ptr<TestRenderWidgetHostView> view_;
  std::unique_ptr<MockRenderWidgetHostDelegate> delegate_;
#if BUILDFLAG(IS_WIN)
  // This is necessary for static methods of `display::ScreenWin`.
  display::win::test::ScopedScreenWin scoped_screen_win_;
#elif BUILDFLAG(IS_CHROMEOS)
  // This is necessary on ChromeOS as it needs to access tablet mode info.
  display::test::TestScreen test_screen_{/*create_dispay=*/true,
                                         /*register_screen=*/true};
#endif
};

TEST_F(FlingSchedulerTest, ScheduleNextFlingProgress) {
  base::TimeTicks progress_time = base::TimeTicks::Now();
  SimulateFlingStart(gfx::Vector2dF(1000, 0));
  EXPECT_TRUE(fling_scheduler()->fling_in_progress());
  EXPECT_EQ(fling_controller(), fling_scheduler()->fling_controller().get());
  EXPECT_EQ(fling_scheduler()->compositor(),
            fling_scheduler()->observed_compositor());

  progress_time += base::Milliseconds(17);
  fling_controller()->ProgressFling(progress_time);
  EXPECT_TRUE(fling_scheduler()->fling_in_progress());
}

TEST_F(FlingSchedulerTest, FlingCancelled) {
  SimulateFlingStart(gfx::Vector2dF(1000, 0));
  EXPECT_TRUE(fling_scheduler()->fling_in_progress());
  EXPECT_EQ(fling_controller(), fling_scheduler()->fling_controller().get());
  EXPECT_EQ(fling_scheduler()->compositor(),
            fling_scheduler()->observed_compositor());

  SimulateFlingCancel();
  EXPECT_FALSE(fling_scheduler()->fling_in_progress());
  EXPECT_EQ(nullptr, fling_scheduler()->fling_controller());
  EXPECT_EQ(nullptr, fling_scheduler()->observed_compositor());
}

}  // namespace content
