// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touch_selection_controller_client_aura.h"

#include <memory>

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/renderer_host/render_widget_host_view_event_handler.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/pointer/touch_editing_controller.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/display_switches.h"
#include "ui/events/event_sink.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/test/motion_event_test_utils.h"
#include "ui/touch_selection/touch_selection_controller_test_api.h"
#include "ui/touch_selection/touch_selection_magnifier_runner.h"

namespace content {
namespace {

bool JSONToPoint(const std::string& str, gfx::PointF* point) {
  absl::optional<base::Value> value = base::JSONReader::Read(str);
  if (!value)
    return false;
  base::Value::Dict* root = value->GetIfDict();
  if (!root)
    return false;
  absl::optional<double> x = root->FindDouble("x");
  absl::optional<double> y = root->FindDouble("y");
  if (!x || !y)
    return false;
  point->set_x(*x);
  point->set_y(*y);
  return true;
}

// A mock touch selection magnifier runner to use whenever a default one is not
// installed.
class TestTouchSelectionMagnifierRunner
    : public ui::TouchSelectionMagnifierRunner {
 public:
  TestTouchSelectionMagnifierRunner() = default;

  TestTouchSelectionMagnifierRunner(const TestTouchSelectionMagnifierRunner&) =
      delete;
  TestTouchSelectionMagnifierRunner& operator=(
      const TestTouchSelectionMagnifierRunner&) = delete;

  ~TestTouchSelectionMagnifierRunner() override = default;

 private:
  void ShowMagnifier(aura::Window* context,
                     const gfx::SelectionBound& focus_bound) override {
    magnifier_running_ = true;
  }

  void CloseMagnifier() override { magnifier_running_ = false; }

  bool IsRunning() const override { return magnifier_running_; }

  bool magnifier_running_ = false;
};

// A mock touch selection menu runner to use whenever a default one is not
// installed.
class TestTouchSelectionMenuRunner : public ui::TouchSelectionMenuRunner {
 public:
  TestTouchSelectionMenuRunner() : menu_opened_(false) {}

  TestTouchSelectionMenuRunner(const TestTouchSelectionMenuRunner&) = delete;
  TestTouchSelectionMenuRunner& operator=(const TestTouchSelectionMenuRunner&) =
      delete;

  ~TestTouchSelectionMenuRunner() override {}

 private:
  bool IsMenuAvailable(
      const ui::TouchSelectionMenuClient* client) const override {
    return true;
  }

  void OpenMenu(base::WeakPtr<ui::TouchSelectionMenuClient> client,
                const gfx::Rect& anchor_rect,
                const gfx::Size& handle_image_size,
                aura::Window* context) override {
    menu_opened_ = true;
  }

  void CloseMenu() override { menu_opened_ = false; }

  bool IsRunning() const override { return menu_opened_; }

  bool menu_opened_;
};

}  // namespace

class TestTouchSelectionControllerClientAura
    : public TouchSelectionControllerClientAura {
 public:
  explicit TestTouchSelectionControllerClientAura(
      RenderWidgetHostViewAura* rwhva)
      : TouchSelectionControllerClientAura(rwhva),
        expected_event_(ui::SELECTION_HANDLES_SHOWN) {
    show_quick_menu_immediately_for_test_ = true;
  }

  TestTouchSelectionControllerClientAura(
      const TestTouchSelectionControllerClientAura&) = delete;
  TestTouchSelectionControllerClientAura& operator=(
      const TestTouchSelectionControllerClientAura&) = delete;

  ~TestTouchSelectionControllerClientAura() override {}

  void InitWaitForSelectionEvent(ui::SelectionEventType expected_event) {
    DCHECK(!run_loop_);
    expected_event_ = expected_event;
    run_loop_ = std::make_unique<base::RunLoop>();
  }

  void InitWaitForHandleContextMenu() {
    DCHECK(!run_loop_);
    waiting_for_handle_context_menu_ = true;
    run_loop_ = std::make_unique<base::RunLoop>();
  }

  bool HandleContextMenu(const ContextMenuParams& params) override {
    bool handled =
        TouchSelectionControllerClientAura::HandleContextMenu(params);
    if (run_loop_ && waiting_for_handle_context_menu_) {
      waiting_for_handle_context_menu_ = false;
      run_loop_->Quit();
    }
    return handled;
  }

  void Wait() {
    DCHECK(run_loop_);
    run_loop_->Run();
    run_loop_.reset();
  }

  ui::TouchSelectionMenuClient* GetActiveMenuClient() {
    return active_menu_client_;
  }

 private:
  // TouchSelectionControllerClientAura:
  void OnSelectionEvent(ui::SelectionEventType event) override {
    TouchSelectionControllerClientAura::OnSelectionEvent(event);
    if (run_loop_ && event == expected_event_)
      run_loop_->Quit();
  }

  bool IsCommandIdEnabled(int command_id) const override {
    // Return true so that quick menu has something to show.
    return true;
  }

  bool waiting_for_handle_context_menu_ = false;
  ui::SelectionEventType expected_event_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

class TouchSelectionControllerClientAuraTest : public ContentBrowserTest {
 public:
  TouchSelectionControllerClientAuraTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kTouchTextEditingRedesign);
  }

  TouchSelectionControllerClientAuraTest(
      const TouchSelectionControllerClientAuraTest&) = delete;
  TouchSelectionControllerClientAuraTest& operator=(
      const TouchSelectionControllerClientAuraTest&) = delete;

  ~TouchSelectionControllerClientAuraTest() override {}

 protected:
  // Starts the test server and navigates to the given url. Sets a large enough
  // size to the root window.  Returns after the navigation to the url is
  // complete.
  void StartTestWithPage(const std::string& url) {
    ASSERT_TRUE(embedded_test_server()->Start());
    GURL test_url(embedded_test_server()->GetURL(url));
    EXPECT_TRUE(NavigateToURL(shell(), test_url));
    aura::Window* content = shell()->web_contents()->GetContentNativeView();
    content->GetHost()->SetBoundsInPixels(gfx::Rect(800, 600));
  }

  gfx::PointF GetPointInsideText() {
    gfx::PointF point;
    JSONToPoint(EvalJs(shell(), "get_point_inside_text()").ExtractString(),
                &point);
    return point;
  }

  gfx::PointF GetPointInsideTextfield() {
    gfx::PointF point;
    JSONToPoint(EvalJs(shell(), "get_point_inside_textfield()").ExtractString(),
                &point);
    return point;
  }

  RenderWidgetHostViewAura* GetRenderWidgetHostViewAura() {
    return static_cast<RenderWidgetHostViewAura*>(
        shell()->web_contents()->GetRenderWidgetHostView());
  }

  TestTouchSelectionControllerClientAura* selection_controller_client() {
    return selection_controller_client_;
  }

  void InitSelectionController() {
    RenderWidgetHostViewAura* rwhva = GetRenderWidgetHostViewAura();
    selection_controller_client_ =
        new TestTouchSelectionControllerClientAura(rwhva);
    rwhva->SetSelectionControllerClientForTest(
        base::WrapUnique(selection_controller_client_.get()));
    // Simulate the start of a motion event sequence, since the tests assume it.
    rwhva->selection_controller()->WillHandleTouchEvent(
        ui::test::MockMotionEvent(ui::MotionEvent::Action::DOWN));
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    if (!ui::TouchSelectionMagnifierRunner::GetInstance())
      magnifier_runner_ = std::make_unique<TestTouchSelectionMagnifierRunner>();
    if (!ui::TouchSelectionMenuRunner::GetInstance())
      menu_runner_ = std::make_unique<TestTouchSelectionMenuRunner>();
  }

 private:
  void TearDownOnMainThread() override {
    magnifier_runner_ = nullptr;
    menu_runner_ = nullptr;
    selection_controller_client_ = nullptr;
    ContentBrowserTest::TearDownOnMainThread();
  }

  std::unique_ptr<TestTouchSelectionMagnifierRunner> magnifier_runner_;
  std::unique_ptr<TestTouchSelectionMenuRunner> menu_runner_;

  raw_ptr<TestTouchSelectionControllerClientAura> selection_controller_client_ =
      nullptr;

  base::test::ScopedFeatureList scoped_feature_list_;
};

class TouchSelectionControllerClientAuraCAPFeatureTest
    : public TouchSelectionControllerClientAuraTest,
      public testing::WithParamInterface<bool> {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    TouchSelectionControllerClientAuraTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(GetParam()
                                        ? switches::kEnableBlinkFeatures
                                        : switches::kDisableBlinkFeatures,
                                    "CompositeAfterPaint");
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }
};

// Tests that long-pressing on a text brings up selection handles and the quick
// menu properly.
IN_PROC_BROWSER_TEST_P(TouchSelectionControllerClientAuraCAPFeatureTest,
                       BasicSelection) {
  // Set the test page up.
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/touch_selection.html"));
  InitSelectionController();

  RenderWidgetHostViewAura* rwhva = GetRenderWidgetHostViewAura();
  EXPECT_EQ(ui::TouchSelectionController::INACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
  EXPECT_EQ(gfx::RectF(),
            rwhva->selection_controller()->GetVisibleRectBetweenBounds());

  // Long-press on the text and wait for handles to appear.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::SELECTION_HANDLES_SHOWN);

  gfx::PointF point = GetPointInsideText();
  ui::GestureEventDetails long_press_details(ui::ET_GESTURE_LONG_PRESS);
  long_press_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent long_press(point.x(), point.y(), 0, ui::EventTimeForNow(),
                              long_press_details);
  rwhva->OnGestureEvent(&long_press);

  selection_controller_client()->Wait();

  // Check that selection is active and the quick menu is showing.
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
  EXPECT_NE(gfx::RectF(),
            rwhva->selection_controller()->GetVisibleRectBetweenBounds());
}

INSTANTIATE_TEST_SUITE_P(TouchSelectionForCAPFeatureTests,
                         TouchSelectionControllerClientAuraCAPFeatureTest,
                         testing::Bool());

class GestureEventWaiter : public RenderWidgetHost::InputEventObserver {
 public:
  explicit GestureEventWaiter(RenderWidgetHost* rwh,
                              blink::WebInputEvent::Type target_event_type)
      : rwh_(static_cast<RenderWidgetHostImpl*>(rwh)->GetWeakPtr()),
        target_event_type_(target_event_type),
        gesture_event_type_seen_(false),
        gesture_event_type_ack_seen_(false) {
    rwh->AddInputEventObserver(this);
  }
  ~GestureEventWaiter() override {
    if (rwh_)
      rwh_->RemoveInputEventObserver(this);
  }

  void OnInputEvent(const blink::WebInputEvent& event) override {
    if (event.GetType() == target_event_type_) {
      gesture_event_type_seen_ = true;
      if (run_loop_)
        run_loop_->Quit();
    }
  }

  void OnInputEventAck(blink::mojom::InputEventResultSource,
                       blink::mojom::InputEventResultState,
                       const blink::WebInputEvent& event) override {
    if (event.GetType() == target_event_type_) {
      gesture_event_type_ack_seen_ = true;
      if (run_loop_)
        run_loop_->Quit();
    }
  }

  void Wait() {
    if (gesture_event_type_seen_)
      return;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_.reset();
  }

  void WaitForAck() {
    if (gesture_event_type_ack_seen_)
      return;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_.reset();
  }

 private:
  base::WeakPtr<RenderWidgetHostImpl> rwh_;
  std::unique_ptr<base::RunLoop> run_loop_;
  blink::WebInputEvent::Type target_event_type_;
  bool gesture_event_type_seen_;
  bool gesture_event_type_ack_seen_;
};

class TouchSelectionControllerClientAuraSiteIsolationTest
    : public TouchSelectionControllerClientAuraTest,
      public testing::WithParamInterface<bool> {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    TouchSelectionControllerClientAuraTest::SetUpCommandLine(command_line);
    IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    TouchSelectionControllerClientAuraTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SelectWithLongPress(gfx::Point point,
                           RenderWidgetHostViewBase* expected_target) {
    // Get main frame view for event insertion.
    RenderWidgetHostViewAura* main_view = GetRenderWidgetHostViewAura();

    GestureEventWaiter long_press_waiter(
        expected_target->GetRenderWidgetHost(),
        blink::WebInputEvent::Type::kGestureLongPress);
    SendTouch(main_view, ui::ET_TOUCH_PRESSED, point);
    // Wait until we see the out-bound LongPress on its way to the renderer, so
    // we know it's ok to send the TOUCH_RELEASED to end the sequence.
    long_press_waiter.Wait();
    SendTouch(main_view, ui::ET_TOUCH_RELEASED, point);
    // Now wait for the LongPress ack to return from the renderer, so our caller
    // knows the LongPress event has been consumed and any relevant selection
    // performed.
    long_press_waiter.WaitForAck();
  }

  void SimpleTap(gfx::Point point, RenderWidgetHostViewBase* expected_target) {
    // Get main frame view for event insertion.
    RenderWidgetHostViewAura* main_view = GetRenderWidgetHostViewAura();

    GestureEventWaiter tap_down_waiter(
        expected_target->GetRenderWidgetHost(),
        blink::WebInputEvent::Type::kGestureTapDown);
    GestureEventWaiter tap_waiter(expected_target->GetRenderWidgetHost(),
                                  blink::WebInputEvent::Type::kGestureTap);
    SendTouch(main_view, ui::ET_TOUCH_PRESSED, point);
    tap_down_waiter.Wait();
    SendTouch(main_view, ui::ET_TOUCH_RELEASED, point);
    tap_waiter.WaitForAck();
  }

 private:
  void SendTouch(RenderWidgetHostViewAura* view,
                 ui::EventType type,
                 gfx::Point point) {
    DCHECK(type >= ui::ET_TOUCH_RELEASED && type <= ui::ET_TOUCH_CANCELLED);
    // If we want the GestureRecognizer to create the gestures for us, we must
    // register the outgoing touch event with it by sending it through the
    // window's event dispatching system.
    aura::Window* shell_window = shell()->window();
    aura::Window* content_window = view->GetNativeView();
    aura::Window::ConvertPointToTarget(content_window, shell_window, &point);
    ui::EventSink* sink = content_window->GetHost()->GetEventSink();
    ui::TouchEvent touch(type, point, ui::EventTimeForNow(),
                         ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    ui::EventDispatchDetails details = sink->OnEventFromSource(&touch);
    ASSERT_FALSE(details.dispatcher_destroyed);
  }
};

INSTANTIATE_TEST_SUITE_P(TouchSelectionForCrossProcessFramesTests,
                         TouchSelectionControllerClientAuraSiteIsolationTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(TouchSelectionControllerClientAuraSiteIsolationTest,
                       BasicSelectionIsolatedIframe) {
  GURL test_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  EXPECT_EQ(
      " Site A\n"
      "   +--Site A\n"
      "Where A = http://a.com/",
      DepictFrameTree(*root));
  TestNavigationObserver observer(shell()->web_contents());
  EXPECT_EQ(1u, root->child_count());
  FrameTreeNode* child = root->child_at(0);

  RenderWidgetHostViewAura* parent_view =
      static_cast<RenderWidgetHostViewAura*>(
          root->current_frame_host()->GetRenderWidgetHost()->GetView());
  TestTouchSelectionControllerClientAura* parent_selection_controller_client =
      new TestTouchSelectionControllerClientAura(parent_view);
  parent_view->SetSelectionControllerClientForTest(
      base::WrapUnique(parent_selection_controller_client));

  // We need to load the desired subframe and then wait until it's stable, i.e.
  // generates no new frames for some reasonable time period: a stray frame
  // between touch selection's pre-handling of GestureLongPress and the
  // expected frame containing the selected region can confuse the
  // TouchSelectionController, causing it to fail to show selection handles.
  // Note this is an issue with the TouchSelectionController in general, and
  // not a property of this test.
  GURL child_url(
      embedded_test_server()->GetURL("b.com", "/touch_selection.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(child, child_url));
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(*root));

  // The child will change with the cross-site navigation. It shouldn't change
  // after this.
  child = root->child_at(0);
  WaitForHitTestData(child->current_frame_host());

  RenderWidgetHostViewChildFrame* child_view =
      static_cast<RenderWidgetHostViewChildFrame*>(
          child->current_frame_host()->GetRenderWidgetHost()->GetView());

  EXPECT_EQ(child_url, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());

  EXPECT_EQ(ui::TouchSelectionController::INACTIVE,
            parent_view->selection_controller()->active_status());
  EXPECT_EQ(gfx::RectF(),
            parent_view->selection_controller()->GetVisibleRectBetweenBounds());

  // Find the location of some text to select.
  gfx::PointF point_f;
  JSONToPoint(EvalJs(child->current_frame_host(), "get_point_inside_text()")
                  .ExtractString(),
              &point_f);
  point_f = child_view->TransformPointToRootCoordSpaceF(point_f);

  // Initiate selection with a sequence of events that go through the targeting
  // system.
  parent_selection_controller_client->InitWaitForSelectionEvent(
      ui::SELECTION_HANDLES_SHOWN);

  SelectWithLongPress(gfx::Point(point_f.x(), point_f.y()), child_view);

  parent_selection_controller_client->Wait();

  // Check that selection is active and the quick menu is showing.
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            parent_view->selection_controller()->active_status());
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
  EXPECT_NE(gfx::RectF(),
            parent_view->selection_controller()->GetVisibleRectBetweenBounds());

  // Tap inside/outside the iframe and make sure the selection handles go away.
  parent_selection_controller_client->InitWaitForSelectionEvent(
      ui::SELECTION_HANDLES_CLEARED);
  if (GetParam()) {
    gfx::PointF point_outside_iframe =
        child_view->TransformPointToRootCoordSpaceF(gfx::PointF(-1.f, -1.f));
    SimpleTap(gfx::Point(point_outside_iframe.x(), point_outside_iframe.y()),
              parent_view);
  } else {
    gfx::PointF point_inside_iframe =
        child_view->TransformPointToRootCoordSpaceF(gfx::PointF(+1.f, +1.f));
    SimpleTap(gfx::Point(point_inside_iframe.x(), point_inside_iframe.y()),
              child_view);
  }
  parent_selection_controller_client->Wait();

  EXPECT_EQ(ui::TouchSelectionController::INACTIVE,
            parent_view->selection_controller()->active_status());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
  EXPECT_EQ(gfx::RectF(),
            parent_view->selection_controller()->GetVisibleRectBetweenBounds());
}

// Failing in sanitizer runs: https://crbug.com/1405296
IN_PROC_BROWSER_TEST_P(TouchSelectionControllerClientAuraSiteIsolationTest,
                       DISABLED_BasicSelectionIsolatedScrollMainframe) {
  GURL test_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  EXPECT_EQ(
      " Site A\n"
      "   +--Site A\n"
      "Where A = http://a.com/",
      DepictFrameTree(*root));
  TestNavigationObserver observer(shell()->web_contents());
  EXPECT_EQ(1u, root->child_count());
  FrameTreeNode* child = root->child_at(0);

  // Make sure mainframe can scroll.
  EXPECT_TRUE(ExecJs(shell()->web_contents(),
                     "document.body.style.height = '900px'; "
                     "document.body.style.overFlowY = 'scroll';"));

  RenderWidgetHostViewAura* parent_view =
      static_cast<RenderWidgetHostViewAura*>(
          root->current_frame_host()->GetRenderWidgetHost()->GetView());

  TestTouchSelectionControllerClientAura* parent_selection_controller_client =
      new TestTouchSelectionControllerClientAura(parent_view);
  parent_view->SetSelectionControllerClientForTest(
      base::WrapUnique(parent_selection_controller_client));

  // We need to load the desired subframe and then wait until it's stable, i.e.
  // generates no new frames for some reasonable time period: a stray frame
  // between touch selection's pre-handling of GestureLongPress and the
  // expected frame containing the selected region can confuse the
  // TouchSelectionController, causing it to fail to show selection handles.
  // Note this is an issue with the TouchSelectionController in general, and
  // not a property of this test.
  GURL child_url(
      embedded_test_server()->GetURL("b.com", "/touch_selection.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(child, child_url));
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(*root));

  // The child will change with the cross-site navigation. It shouldn't change
  // after this.
  child = root->child_at(0);
  WaitForHitTestData(child->current_frame_host());

  RenderWidgetHostViewChildFrame* child_view =
      static_cast<RenderWidgetHostViewChildFrame*>(
          child->current_frame_host()->GetRenderWidgetHost()->GetView());

  EXPECT_EQ(child_url, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());

  ui::TouchSelectionController* selection_controller =
      parent_view->selection_controller();
  EXPECT_EQ(ui::TouchSelectionController::INACTIVE,
            selection_controller->active_status());
  EXPECT_EQ(gfx::RectF(), selection_controller->GetVisibleRectBetweenBounds());

  ui::TouchSelectionControllerTestApi selection_controller_test_api(
      selection_controller);

  RenderFrameProxyHost* child_proxy_host =
      child->render_manager()->GetProxyToParent();
  auto interceptor = std::make_unique<SynchronizeVisualPropertiesInterceptor>(
      child_proxy_host);

  // Find the location of some text to select.
  gfx::PointF point_f;
  JSONToPoint(EvalJs(child->current_frame_host(), "get_point_inside_text()")
                  .ExtractString(),
              &point_f);
  point_f = child_view->TransformPointToRootCoordSpaceF(point_f);

  // Initiate selection with a sequence of events that go through the targeting
  // system.
  parent_selection_controller_client->InitWaitForSelectionEvent(
      ui::SELECTION_HANDLES_SHOWN);

  SelectWithLongPress(gfx::Point(point_f.x(), point_f.y()), child_view);

  parent_selection_controller_client->Wait();

  // Check that selection is active and the quick menu is showing.
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            selection_controller->active_status());
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
  EXPECT_NE(gfx::RectF(), selection_controller->GetVisibleRectBetweenBounds());

  gfx::Point scroll_start_position(10, 10);
  gfx::Point scroll_end_position(10, 0);
  // Initiate a touch scroll of the main frame, and make sure when the selection
  // handles re-appear make sure they have the correct location.
  // 1) Send touch-down.
  ui::TouchEvent touch_down(
      ui::ET_TOUCH_PRESSED, scroll_start_position, ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  parent_view->OnTouchEvent(&touch_down);
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            selection_controller->active_status());
  EXPECT_FALSE(selection_controller_test_api.temporarily_hidden());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
  gfx::PointF initial_start_handle_position =
      selection_controller->GetStartPosition();

  // 2) Send touch-move.
  ui::TouchEvent touch_move(
      ui::ET_TOUCH_MOVED, scroll_end_position, ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  parent_view->OnTouchEvent(&touch_move);
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            selection_controller->active_status());
  EXPECT_FALSE(selection_controller_test_api.temporarily_hidden());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Start scrolling: touch handles should get hidden, while touch selection is
  // still active.
  ui::GestureEventDetails scroll_begin_details(ui::ET_GESTURE_SCROLL_BEGIN);
  scroll_begin_details.set_device_type(
      ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent scroll_begin(scroll_start_position.x(),
                                scroll_start_position.y(), 0,
                                ui::EventTimeForNow(), scroll_begin_details);
  parent_view->OnGestureEvent(&scroll_begin);
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            parent_view->selection_controller()->active_status());
  EXPECT_TRUE(selection_controller_test_api.temporarily_hidden());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // GestureScrollUpdate
  gfx::Vector2dF scroll_delta = scroll_end_position - scroll_start_position;
  ui::GestureEventDetails scroll_update_details(
      ui::ET_GESTURE_SCROLL_UPDATE, scroll_delta.x(), scroll_delta.y());
  scroll_update_details.set_device_type(
      ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent scroll_update(scroll_start_position.x(),
                                 scroll_start_position.y(), 0,
                                 ui::EventTimeForNow(), scroll_update_details);
  parent_view->OnGestureEvent(&scroll_update);
  EXPECT_TRUE(selection_controller_test_api.temporarily_hidden());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Make sure we wait for the scroll to actually happen.
  interceptor->WaitForRect();

  // Since the check below that compares the scroll_delta to the actual handle
  // movement requires use of TransformPointToRootCoordSpaceF() in
  // TouchSelectionControllerClientChildFrame::DidScroll(), we need to
  // make sure the post-scroll frames have rendered before the transform
  // can be trusted. This may point out a general concern with the timing
  // of the main-frame's did-stop-flinging IPC and the rendering of the
  // child frame's compositor frame.
  {
    base::RunLoop loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, loop.QuitClosure(), TestTimeouts::tiny_timeout());
    loop.Run();
  }

  // End scrolling: touch handles should re-appear.
  ui::GestureEventDetails scroll_end_details(ui::ET_GESTURE_SCROLL_END);
  scroll_end_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent scroll_end(scroll_end_position.x(), scroll_end_position.y(),
                              0, ui::EventTimeForNow(), scroll_end_details);
  parent_view->OnGestureEvent(&scroll_end);
  EXPECT_FALSE(selection_controller_test_api.temporarily_hidden());
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            parent_view->selection_controller()->active_status());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // 3) Send touch-end.
  ui::TouchEvent touch_up(ui::ET_TOUCH_RELEASED, scroll_end_position,
                          ui::EventTimeForNow(),
                          ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  parent_view->OnTouchEvent(&touch_up);
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            selection_controller->active_status());
  EXPECT_FALSE(selection_controller_test_api.temporarily_hidden());
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
  EXPECT_NE(gfx::RectF(), selection_controller->GetVisibleRectBetweenBounds());

  // Make sure handles have moved.
  gfx::PointF final_start_handle_position =
      selection_controller->GetStartPosition();
  EXPECT_EQ(scroll_delta,
            final_start_handle_position - initial_start_handle_position);
}

// Tests that tapping in a textfield brings up the insertion handle, but not the
// quick menu, initially. Then, successive taps on the insertion handle toggle
// the quick menu visibility.
IN_PROC_BROWSER_TEST_P(TouchSelectionControllerClientAuraCAPFeatureTest,
                       BasicInsertionFollowedByTapsOnHandle) {
  // Set the test page up.
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/touch_selection.html"));
  InitSelectionController();

  RenderWidgetHostViewAura* rwhva = GetRenderWidgetHostViewAura();
  EXPECT_EQ(ui::TouchSelectionController::INACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
  EXPECT_EQ(gfx::RectF(),
            rwhva->selection_controller()->GetVisibleRectBetweenBounds());

  gfx::NativeView native_view = rwhva->GetNativeView();
  ui::test::EventGenerator generator(native_view->GetRootWindow());

  // Tap inside the textfield and wait for the insertion handle to appear.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::INSERTION_HANDLE_SHOWN);

  gfx::Point point = gfx::ToRoundedPoint(GetPointInsideTextfield());
  generator.delegate()->ConvertPointFromTarget(native_view, &point);
  generator.GestureTapAt(point);

  selection_controller_client()->Wait();

  // Check that insertion is active, but the quick menu is not showing.
  EXPECT_EQ(ui::TouchSelectionController::INSERTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Tap on the insertion handle; the quick menu should appear.
  gfx::Point handle_center = gfx::ToRoundedPoint(
      rwhva->selection_controller()->GetStartHandleRect().CenterPoint());
  generator.delegate()->ConvertPointFromTarget(native_view, &handle_center);
  generator.GestureTapAt(handle_center);
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
  EXPECT_NE(gfx::RectF(),
            rwhva->selection_controller()->GetVisibleRectBetweenBounds());

  // Tap once more on the insertion handle; the quick menu should disappear.
  generator.GestureTapAt(handle_center);
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
}

#if BUILDFLAG(IS_CHROMEOS)
// Tests that tapping the caret toggles showing and hiding the quick menu.
IN_PROC_BROWSER_TEST_P(TouchSelectionControllerClientAuraCAPFeatureTest,
                       TapOnCaret) {
  // Set the test page up.
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/touch_selection.html"));
  InitSelectionController();

  RenderWidgetHostViewAura* rwhva = GetRenderWidgetHostViewAura();
  const gfx::NativeView native_view = rwhva->GetNativeView();
  ui::test::EventGenerator generator(native_view->GetRootWindow());

  // Mouse click inside the textfield to make a caret appear.
  gfx::Point point = gfx::ToRoundedPoint(GetPointInsideTextfield());
  generator.delegate()->ConvertPointFromTarget(native_view, &point);
  generator.MoveMouseTo(point);
  generator.PressLeftButton();
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Tap the caret to show the quick menu.
  selection_controller_client()->InitWaitForHandleContextMenu();
  generator.GestureTapAt(point);
  selection_controller_client()->Wait();
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Tap the caret again to hide the quick menu. We advance the clock before
  // tapping again to avoid the tap being treated as a double tap.
  generator.AdvanceClock(base::Milliseconds(1000));
  selection_controller_client()->InitWaitForHandleContextMenu();
  generator.GestureTapAt(point);
  selection_controller_client()->Wait();
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
}
#endif

// Tests that the quick menu is hidden whenever a touch point is active.
// Flaky: https://crbug.com/803576
IN_PROC_BROWSER_TEST_F(TouchSelectionControllerClientAuraTest,
                       DISABLED_QuickMenuHiddenOnTouch) {
  // Set the test page up.
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/touch_selection.html"));
  InitSelectionController();

  RenderWidgetHostViewAura* rwhva = GetRenderWidgetHostViewAura();
  EXPECT_EQ(ui::TouchSelectionController::INACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
  EXPECT_EQ(gfx::RectF(),
            rwhva->selection_controller()->GetVisibleRectBetweenBounds());

  // Long-press on the text and wait for selection handles to appear.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::SELECTION_HANDLES_SHOWN);

  gfx::PointF point = GetPointInsideText();
  ui::GestureEventDetails long_press_details(ui::ET_GESTURE_LONG_PRESS);
  long_press_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent long_press(point.x(), point.y(), 0, ui::EventTimeForNow(),
                              long_press_details);
  rwhva->OnGestureEvent(&long_press);

  selection_controller_client()->Wait();

  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
  EXPECT_NE(gfx::RectF(),
            rwhva->selection_controller()->GetVisibleRectBetweenBounds());

  ui::test::EventGenerator generator(rwhva->GetNativeView()->GetRootWindow(),
                                     rwhva->GetNativeView());

  // Put the first finger down: the quick menu should get hidden.
  generator.PressTouchId(0);
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Put a second finger down: the quick menu should remain hidden.
  generator.PressTouchId(1);
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Lift the first finger up: the quick menu should still remain hidden.
  generator.ReleaseTouchId(0);
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Lift the second finger up: the quick menu should re-appear.
  generator.ReleaseTouchId(1);
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
  EXPECT_NE(gfx::RectF(),
            rwhva->selection_controller()->GetVisibleRectBetweenBounds());
}

// Tests that the quick menu and touch handles are hidden during an scroll.
IN_PROC_BROWSER_TEST_P(TouchSelectionControllerClientAuraCAPFeatureTest,
                       HiddenOnScroll) {
  // Set the test page up.
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/touch_selection.html"));
  InitSelectionController();

  RenderWidgetHostViewAura* rwhva = GetRenderWidgetHostViewAura();
  ui::TouchSelectionControllerTestApi selection_controller_test_api(
      rwhva->selection_controller());

  EXPECT_EQ(ui::TouchSelectionController::INACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
  EXPECT_EQ(gfx::RectF(),
            rwhva->selection_controller()->GetVisibleRectBetweenBounds());

  // Long-press on the text and wait for selection handles to appear.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::SELECTION_HANDLES_SHOWN);

  gfx::PointF point = GetPointInsideText();
  ui::GestureEventDetails long_press_details(ui::ET_GESTURE_LONG_PRESS);
  long_press_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent long_press(point.x(), point.y(), 0, ui::EventTimeForNow(),
                              long_press_details);
  rwhva->OnGestureEvent(&long_press);

  selection_controller_client()->Wait();

  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(selection_controller_test_api.temporarily_hidden());
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
  EXPECT_NE(gfx::RectF(),
            rwhva->selection_controller()->GetVisibleRectBetweenBounds());

  // Put a finger down: the quick menu should go away, while touch handles stay
  // there.
  ui::TouchEvent touch_down(
      ui::ET_TOUCH_PRESSED, gfx::Point(10, 10), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  rwhva->OnTouchEvent(&touch_down);
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(selection_controller_test_api.temporarily_hidden());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Start scrolling: touch handles should get hidden, while touch selection is
  // still active.
  ui::GestureEventDetails scroll_begin_details(ui::ET_GESTURE_SCROLL_BEGIN);
  scroll_begin_details.set_device_type(
      ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent scroll_begin(10, 10, 0, ui::EventTimeForNow(),
                                scroll_begin_details);
  rwhva->OnGestureEvent(&scroll_begin);
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_TRUE(selection_controller_test_api.temporarily_hidden());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // End scrolling: touch handles should re-appear.
  ui::GestureEventDetails scroll_end_details(ui::ET_GESTURE_SCROLL_END);
  scroll_end_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent scroll_end(10, 10, 0, ui::EventTimeForNow(),
                              scroll_end_details);
  rwhva->OnGestureEvent(&scroll_end);
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(selection_controller_test_api.temporarily_hidden());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Lift the finger up: the quick menu should re-appear.
  ui::TouchEvent touch_up(ui::ET_TOUCH_RELEASED, gfx::Point(10, 10),
                          ui::EventTimeForNow(),
                          ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  rwhva->OnTouchEvent(&touch_up);
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(selection_controller_test_api.temporarily_hidden());
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
  EXPECT_NE(gfx::RectF(),
            rwhva->selection_controller()->GetVisibleRectBetweenBounds());
}

// Tests that the magnifier is correctly shown for a swipe-to-move-cursor
// gesture.
IN_PROC_BROWSER_TEST_P(TouchSelectionControllerClientAuraCAPFeatureTest,
                       SwipeToMoveCursorMagnifier) {
  // Set the test page up.
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/touch_selection.html"));
  InitSelectionController();

  RenderWidgetHostViewAura* rwhva = GetRenderWidgetHostViewAura();
  gfx::NativeView native_view = rwhva->GetNativeView();
  ui::test::EventGenerator generator(native_view->GetRootWindow());
  EXPECT_FALSE(ui::TouchSelectionMagnifierRunner::GetInstance()->IsRunning());

  // Tap to focus the textfield.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::INSERTION_HANDLE_SHOWN);
  gfx::Point start = gfx::ToRoundedPoint(GetPointInsideTextfield());
  generator.delegate()->ConvertPointFromTarget(native_view, &start);
  generator.GestureTapAt(start);
  selection_controller_client()->Wait();

  // Swipe to move the cursor. We advance the clock before swiping to avoid the
  // start of the gesture being interpreted as a double press.
  generator.AdvanceClock(base::Milliseconds(1000));
  generator.GestureScrollSequenceWithCallback(
      start, start + gfx::Vector2d(100, 0), /*duration=*/base::Milliseconds(50),
      /*steps=*/5,
      base::BindLambdaForTesting([&](ui::EventType event_type,
                                     const gfx::Vector2dF& offset) {
        if (event_type == ui::ET_GESTURE_SCROLL_BEGIN) {
          selection_controller_client()->InitWaitForSelectionEvent(
              ui::INSERTION_HANDLE_MOVED);
        } else if (event_type == ui::ET_GESTURE_SCROLL_UPDATE) {
          selection_controller_client()->Wait();
          EXPECT_TRUE(
              ui::TouchSelectionMagnifierRunner::GetInstance()->IsRunning());
          selection_controller_client()->InitWaitForSelectionEvent(
              ui::INSERTION_HANDLE_MOVED);
        }
      }));
  EXPECT_FALSE(ui::TouchSelectionMagnifierRunner::GetInstance()->IsRunning());
}

// Tests that the select all command in the quick menu works correctly and that
// the touch handles and quick menu are shown after the command is executed.
IN_PROC_BROWSER_TEST_P(TouchSelectionControllerClientAuraCAPFeatureTest,
                       SelectAllCommand) {
  // Set the test page up.
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/touch_selection.html"));
  InitSelectionController();

  RenderWidgetHostViewAura* rwhva = GetRenderWidgetHostViewAura();
  ui::TouchSelectionControllerTestApi selection_controller_test_api(
      rwhva->selection_controller());
  EXPECT_EQ(ui::TouchSelectionController::INACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
  EXPECT_EQ(gfx::RectF(),
            rwhva->selection_controller()->GetVisibleRectBetweenBounds());

  gfx::NativeView native_view = rwhva->GetNativeView();
  ui::test::EventGenerator generator(native_view->GetRootWindow());

  // Tap inside the textfield and wait for the insertion handle to appear.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::INSERTION_HANDLE_SHOWN);
  gfx::Point point = gfx::ToRoundedPoint(GetPointInsideTextfield());
  generator.delegate()->ConvertPointFromTarget(native_view, &point);
  generator.GestureTapAt(point);
  selection_controller_client()->Wait();
  EXPECT_EQ(ui::TouchSelectionController::INSERTION_ACTIVE,
            rwhva->selection_controller()->active_status());

  // Execute select all command. All text in textfield should be selected and
  // touch handles and quick menu should be shown.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::SELECTION_HANDLES_SHOWN);
  selection_controller_client()->GetActiveMenuClient()->ExecuteCommand(
      ui::TouchEditable::kSelectAll, 0);
  selection_controller_client()->Wait();
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
  EXPECT_TRUE(selection_controller_test_api.GetStartVisible());
  EXPECT_TRUE(selection_controller_test_api.GetEndVisible());
  EXPECT_EQ(
      selection_controller_client()->GetActiveMenuClient()->GetSelectedText(),
      u"Text in a textfield");
}

// Tests that the select word command in the quick menu works correctly and that
// the touch handles and quick menu are shown after the command is executed.
IN_PROC_BROWSER_TEST_P(TouchSelectionControllerClientAuraCAPFeatureTest,
                       SelectWordCommand) {
  // Set the test page up.
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/touch_selection.html"));
  InitSelectionController();

  RenderWidgetHostViewAura* rwhva = GetRenderWidgetHostViewAura();
  ui::TouchSelectionControllerTestApi selection_controller_test_api(
      rwhva->selection_controller());
  EXPECT_EQ(ui::TouchSelectionController::INACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
  EXPECT_EQ(gfx::RectF(),
            rwhva->selection_controller()->GetVisibleRectBetweenBounds());

  gfx::NativeView native_view = rwhva->GetNativeView();
  ui::test::EventGenerator generator(native_view->GetRootWindow());

  // Tap inside the textfield and wait for the insertion handle to appear.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::INSERTION_HANDLE_SHOWN);
  gfx::Point point = gfx::ToRoundedPoint(GetPointInsideTextfield());
  generator.delegate()->ConvertPointFromTarget(native_view, &point);
  generator.GestureTapAt(point);
  selection_controller_client()->Wait();
  EXPECT_EQ(ui::TouchSelectionController::INSERTION_ACTIVE,
            rwhva->selection_controller()->active_status());

  // Execute select word command. The word around the current caret position
  // should be selected and touch handles and quick menu should be shown.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::SELECTION_HANDLES_SHOWN);
  selection_controller_client()->GetActiveMenuClient()->ExecuteCommand(
      ui::TouchEditable::kSelectWord, 0);
  selection_controller_client()->Wait();
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
  EXPECT_TRUE(selection_controller_test_api.GetStartVisible());
  EXPECT_TRUE(selection_controller_test_api.GetEndVisible());
  EXPECT_EQ(
      selection_controller_client()->GetActiveMenuClient()->GetSelectedText(),
      u"Text");
}

class TouchSelectionControllerClientAuraScaleFactorTest
    : public TouchSelectionControllerClientAuraTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "2");
  }
};

class TouchSelectionControllerClientAuraScaleFactorCAPFeatureTest
    : public TouchSelectionControllerClientAuraScaleFactorTest,
      public testing::WithParamInterface<bool> {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    TouchSelectionControllerClientAuraScaleFactorTest::SetUpCommandLine(
        command_line);
    command_line->AppendSwitchASCII(GetParam()
                                        ? switches::kEnableBlinkFeatures
                                        : switches::kDisableBlinkFeatures,
                                    "CompositeAfterPaint");
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }
};

// Tests that selection handles are properly positioned at 2x DSF and that the
// quick menu and magnifier are updated with the selection handles.
IN_PROC_BROWSER_TEST_P(
    TouchSelectionControllerClientAuraScaleFactorCAPFeatureTest,
    SelectionHandleCoordinates) {
  // Set the test page up.
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/touch_selection.html"));
  InitSelectionController();

  RenderWidgetHostViewAura* rwhva = GetRenderWidgetHostViewAura();

  EXPECT_EQ(ui::TouchSelectionController::INACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(ui::TouchSelectionMagnifierRunner::GetInstance()->IsRunning());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
  EXPECT_EQ(2.f, rwhva->GetDeviceScaleFactor());
  EXPECT_EQ(gfx::RectF(),
            rwhva->selection_controller()->GetVisibleRectBetweenBounds());

  // Long-press on the text and wait for handles to appear.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::SELECTION_HANDLES_SHOWN);
  gfx::PointF point = GetPointInsideText();
  ui::GestureEventDetails long_press_details(ui::ET_GESTURE_LONG_PRESS);
  long_press_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent long_press(point.x(), point.y(), 0, ui::EventTimeForNow(),
                              long_press_details);
  rwhva->OnGestureEvent(&long_press);
  selection_controller_client()->Wait();

  // Check that selection is active and the quick menu is showing.
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
  EXPECT_NE(gfx::RectF(),
            rwhva->selection_controller()->GetVisibleRectBetweenBounds());
  const ui::TouchSelectionController* controller =
      GetRenderWidgetHostViewAura()->selection_controller();

  gfx::PointF start_top = controller->start().edge_start();

  // The selection start should be uppper left, and selection end should be
  // upper right.
  EXPECT_LT(controller->start().edge_start().x(), point.x());
  EXPECT_LT(controller->start().edge_end().x(), point.x());

  EXPECT_LT(point.x(), controller->end().edge_start().x());
  EXPECT_LT(point.x(), controller->end().edge_end().x());

  // Handles are created below the selection. The top position should roughly
  // be within the handle size from the touch position.
  float handle_size =
      controller->start().edge_end().y() - controller->start().edge_start().y();
  float handle_max_bottom = point.y() + handle_size;
  EXPECT_GT(handle_max_bottom, controller->start().edge_start().y());
  EXPECT_GT(handle_max_bottom, controller->end().edge_start().y());

  gfx::Point handle_point = gfx::ToRoundedPoint(
      rwhva->selection_controller()->GetStartHandleRect().CenterPoint());

  // Move the selection handle. Touch the handle first.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::SELECTION_HANDLE_DRAG_STARTED);
  ui::TouchEvent touch_down(
      ui::ET_TOUCH_PRESSED, handle_point, ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  rwhva->OnTouchEvent(&touch_down);
  selection_controller_client()->Wait();

  // The magnifier should be shown when selection handle dragging starts.
  EXPECT_TRUE(ui::TouchSelectionMagnifierRunner::GetInstance()->IsRunning());

  // Move the selection handle.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::SELECTION_HANDLES_MOVED);
  handle_point.Offset(10, 0);
  ui::TouchEvent touch_move(
      ui::ET_TOUCH_MOVED, handle_point, ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  rwhva->OnTouchEvent(&touch_move);
  selection_controller_client()->Wait();

  // The magnifier should still be shown after the selection handle moves.
  EXPECT_TRUE(ui::TouchSelectionMagnifierRunner::GetInstance()->IsRunning());

  // Then release.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::SELECTION_HANDLE_DRAG_STOPPED);
  ui::TouchEvent touch_up(ui::ET_TOUCH_RELEASED, handle_point,
                          ui::EventTimeForNow(),
                          ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  rwhva->OnTouchEvent(&touch_up);
  selection_controller_client()->Wait();

  // The handle should have moved to the right and the magnifier should no
  // longer be shown.
  EXPECT_EQ(start_top.y(), controller->start().edge_start().y());
  EXPECT_LT(start_top.x(), controller->start().edge_start().x());
  EXPECT_FALSE(ui::TouchSelectionMagnifierRunner::GetInstance()->IsRunning());

  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_NE(gfx::RectF(),
            rwhva->selection_controller()->GetVisibleRectBetweenBounds());
}

INSTANTIATE_TEST_SUITE_P(
    TouchSelectionScaleFactorForCAPFeatureTests,
    TouchSelectionControllerClientAuraScaleFactorCAPFeatureTest,
    testing::Bool());

// Tests that insertion handles are properly positioned at 2x DSF and that the
// magnifier is updated with the insertion handle.
IN_PROC_BROWSER_TEST_P(
    TouchSelectionControllerClientAuraScaleFactorCAPFeatureTest,
    InsertionHandleCoordinates) {
  // Set the test page up.
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/touch_selection.html"));
  InitSelectionController();

  RenderWidgetHostViewAura* rwhva = GetRenderWidgetHostViewAura();

  // Tap inside the textfield and wait for the insertion cursor.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::INSERTION_HANDLE_SHOWN);

  gfx::PointF point = GetPointInsideTextfield();

  ui::GestureEventDetails gesture_tap_down_details(ui::ET_GESTURE_TAP_DOWN);
  gesture_tap_down_details.set_device_type(
      ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent gesture_tap_down(2, 2, 0, ui::EventTimeForNow(),
                                    gesture_tap_down_details);
  rwhva->OnGestureEvent(&gesture_tap_down);
  ui::GestureEventDetails tap_details(ui::ET_GESTURE_TAP);
  tap_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  tap_details.set_tap_count(1);
  ui::GestureEvent tap(point.x(), point.y(), 0, ui::EventTimeForNow(),
                       tap_details);
  rwhva->OnGestureEvent(&tap);

  selection_controller_client()->Wait();

  EXPECT_EQ(ui::TouchSelectionController::INSERTION_ACTIVE,
            rwhva->selection_controller()->active_status());

  gfx::RectF initial_handle_rect =
      rwhva->selection_controller()->GetStartHandleRect();

  // Move the insertion handle. Touch the handle first.
  gfx::Point handle_point =
      gfx::ToRoundedPoint(initial_handle_rect.CenterPoint());

  selection_controller_client()->InitWaitForSelectionEvent(
      ui::INSERTION_HANDLE_DRAG_STARTED);
  ui::TouchEvent touch_down(
      ui::ET_TOUCH_PRESSED, handle_point, ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  rwhva->OnTouchEvent(&touch_down);
  selection_controller_client()->Wait();

  // The magnifier should be shown when insertion handle dragging starts.
  EXPECT_TRUE(ui::TouchSelectionMagnifierRunner::GetInstance()->IsRunning());

  // Move it.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::INSERTION_HANDLE_MOVED);
  handle_point.Offset(10, 0);
  ui::TouchEvent touch_move(
      ui::ET_TOUCH_MOVED, handle_point, ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  rwhva->OnTouchEvent(&touch_move);
  selection_controller_client()->Wait();

  // The magnifier should still be shown after the insertion handle moves.
  EXPECT_TRUE(ui::TouchSelectionMagnifierRunner::GetInstance()->IsRunning());

  // Then release.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::INSERTION_HANDLE_DRAG_STOPPED);
  ui::TouchEvent touch_up(ui::ET_TOUCH_RELEASED, handle_point,
                          ui::EventTimeForNow(),
                          ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  rwhva->OnTouchEvent(&touch_up);
  selection_controller_client()->Wait();
  EXPECT_FALSE(ui::TouchSelectionMagnifierRunner::GetInstance()->IsRunning());

  gfx::RectF moved_handle_rect =
      rwhva->selection_controller()->GetStartHandleRect();

  // The handle should have moved to the right and the magnifier should no
  // longer be shown.
  EXPECT_EQ(initial_handle_rect.y(), moved_handle_rect.y());
  EXPECT_LT(initial_handle_rect.x(), moved_handle_rect.x());
  EXPECT_FALSE(ui::TouchSelectionMagnifierRunner::GetInstance()->IsRunning());

  EXPECT_EQ(ui::TouchSelectionController::INSERTION_ACTIVE,
            rwhva->selection_controller()->active_status());
}

}  // namespace content
