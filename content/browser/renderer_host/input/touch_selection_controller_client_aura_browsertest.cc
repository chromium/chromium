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
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/renderer_host/render_widget_host_view_event_handler.h"
#include "content/browser/renderer_host/text_input_manager.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/synchronize_visual_properties_interceptor.h"
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
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/test/motion_event_test_utils.h"
#include "ui/touch_selection/touch_selection_controller_test_api.h"
#include "ui/touch_selection/touch_selection_metrics.h"

namespace content {
namespace {

// Character dimensions in px, from the font size in `touch_selection.html`.
constexpr int kCharacterWidth = 15;
constexpr int kCharacterHeight = 15;

bool JSONToPoint(const std::string& str, gfx::PointF* point) {
  std::optional<base::Value> value = base::JSONReader::Read(str);
  if (!value)
    return false;
  base::Value::Dict* root = value->GetIfDict();
  if (!root)
    return false;
  std::optional<double> x = root->FindDouble("x");
  std::optional<double> y = root->FindDouble("y");
  if (!x || !y)
    return false;
  point->set_x(*x);
  point->set_y(*y);
  return true;
}

gfx::RectF ConvertRectFToChildCoords(RenderWidgetHostViewAura* parent,
                                     RenderWidgetHostViewChildFrame* child,
                                     const gfx::RectF& rect) {
  return gfx::BoundingRect(
      child->TransformRootPointToViewCoordSpace(rect.origin()),
      child->TransformRootPointToViewCoordSpace(rect.bottom_right()));
}

// Converts a point from `view` coordinates to the coordinate system used by
// `generator_delegate`.
gfx::Point ConvertPointFromView(
    RenderWidgetHostViewAura* view,
    const ui::test::EventGeneratorDelegate* generator_delegate,
    const gfx::PointF& point_in_view) {
  gfx::Point point_in_generator = gfx::ToRoundedPoint(point_in_view);
  generator_delegate->ConvertPointFromTarget(view->GetNativeView(),
                                             &point_in_generator);
  return gfx::ScaleToRoundedPoint(point_in_generator,
                                  view->GetDeviceScaleFactor());
}

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
    : public TouchSelectionControllerClientAura,
      public TextInputManager::Observer {
 public:
  TestTouchSelectionControllerClientAura(RenderWidgetHostViewAura* rwhva,
                                         bool enable_all_menu_commands)
      : TouchSelectionControllerClientAura(rwhva),
        expected_event_(ui::SELECTION_HANDLES_SHOWN),
        enable_all_menu_commands_(enable_all_menu_commands) {
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
    DCHECK(!menu_run_loop_);
    waiting_for_handle_context_menu_ = true;
    menu_run_loop_ = std::make_unique<base::RunLoop>();
  }

  bool HandleContextMenu(const ContextMenuParams& params) override {
    bool handled =
        TouchSelectionControllerClientAura::HandleContextMenu(params);
    if (menu_run_loop_ && waiting_for_handle_context_menu_) {
      waiting_for_handle_context_menu_ = false;
      menu_run_loop_->Quit();
    }
    return handled;
  }

  void InitWaitForSelectionUpdate() {
    DCHECK(!run_loop_);
    // Wait for selection change to ensure that the selected text is updated.
    waiting_for_selection_change_ = true;
    // Wait for bounds update to ensure that the TouchSelectionController has
    // processed the selection update (since it uses these bounds e.g. for
    // handle placement and to initiate long press drag selection).
    waiting_for_selection_bounds_update_ = true;
    rwhva_->GetTextInputManager()->AddObserver(this);
    run_loop_ = std::make_unique<base::RunLoop>();
  }

  void UpdateClientSelectionBounds(const gfx::SelectionBound& start,
                                   const gfx::SelectionBound& end) override {
    TouchSelectionControllerClientAura::UpdateClientSelectionBounds(start, end);
    if (run_loop_ && waiting_for_selection_bounds_update_) {
      waiting_for_selection_bounds_update_ = false;
      // Only quit the run loop once the selection and bounds have both updated.
      if (!waiting_for_selection_change_) {
        run_loop_->Quit();
      }
    }
  }

  void OnTextSelectionChanged(TextInputManager* text_input_manager,
                              RenderWidgetHostViewBase* updated_view) override {
    if (run_loop_ && waiting_for_selection_change_) {
      text_input_manager->RemoveObserver(this);
      waiting_for_selection_change_ = false;
      // Only quit the run loop once the selection and bounds have both updated.
      if (!waiting_for_selection_bounds_update_) {
        run_loop_->Quit();
      }
    }
  }

  void Wait() {
    DCHECK(run_loop_);
    run_loop_->Run();
    run_loop_.reset();
  }

  void WaitForHandleContextMenu() {
    DCHECK(menu_run_loop_);
    menu_run_loop_->Run();
    menu_run_loop_.reset();
  }

  ui::TouchSelectionMenuClient* GetActiveMenuClient() {
    return active_menu_client_;
  }

  bool IsMagnifierVisible() const {
    return touch_selection_magnifier_ != nullptr;
  }

  bool IsHandlingSelectionDrag() const { return handle_drag_in_progress_; }

 private:
  // TouchSelectionControllerClientAura:
  void OnSelectionEvent(ui::SelectionEventType event) override {
    TouchSelectionControllerClientAura::OnSelectionEvent(event);
    if (run_loop_ && event == expected_event_)
      run_loop_->Quit();
  }

  bool IsCommandIdEnabled(int command_id) const override {
    if (enable_all_menu_commands_) {
      return true;
    }
    return TouchSelectionControllerClientAura::IsCommandIdEnabled(command_id);
  }

  bool waiting_for_handle_context_menu_ = false;
  bool waiting_for_selection_change_ = false;
  bool waiting_for_selection_bounds_update_ = false;
  ui::SelectionEventType expected_event_;
  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<base::RunLoop> menu_run_loop_;

  // When set to true, the TouchSelectionControllerClientAura criteria for
  // enabling menu commands is overridden and we instead enable all menu
  // commands. This is so that we can test behaviour related to showing/hiding
  // the menu without worrying about whether commands will be shown or not.
  bool enable_all_menu_commands_ = false;
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

  gfx::PointF GetPointInText(int cursor_index) const {
    gfx::PointF point;
    JSONToPoint(EvalJs(shell(), "get_top_left_of_text()").ExtractString(),
                &point);
    point.Offset(cursor_index * kCharacterWidth, 0.5f * kCharacterHeight);
    return point;
  }

  gfx::PointF GetPointInTextfield(int cursor_index) const {
    gfx::PointF point;
    JSONToPoint(EvalJs(shell(), "get_top_left_of_textfield()").ExtractString(),
                &point);
    point.Offset(cursor_index * kCharacterWidth, 0.5f * kCharacterHeight);
    return point;
  }

  gfx::PointF GetPointInsideEmptyTextfield() const {
    gfx::PointF point;
    JSONToPoint(
        EvalJs(shell(), "get_top_left_of_empty_textfield()").ExtractString(),
        &point);
    // Offset the point so that it is within the textfield.
    point.Offset(0.5f * kCharacterWidth, 0.5f * kCharacterHeight);
    return point;
  }

  RenderWidgetHostViewAura* GetRenderWidgetHostViewAura() {
    return static_cast<RenderWidgetHostViewAura*>(
        shell()->web_contents()->GetRenderWidgetHostView());
  }

  TestTouchSelectionControllerClientAura* selection_controller_client() {
    return static_cast<TestTouchSelectionControllerClientAura*>(
        GetRenderWidgetHostViewAura()->selection_controller_client());
  }

  // Performs a tap to place the cursor at `point`.
  void TapAndWaitForCursor(ui::test::EventGenerator& generator,
                           const gfx::Point& point) {
    selection_controller_client()->InitWaitForSelectionUpdate();
    generator.GestureTapAt(point);
    selection_controller_client()->Wait();
  }

  // Performs a long press to select the word at `point`.
  void SelectWithLongPress(ui::test::EventGenerator& generator,
                           const gfx::Point& point) {
    selection_controller_client()->InitWaitForSelectionUpdate();
    generator.PressTouch(point);
    selection_controller_client()->Wait();
  }

  // Performs a double press to select the word at `point`.
  void SelectWithDoublePress(ui::test::EventGenerator& generator,
                             const gfx::Point& point) {
    // Perform the first press and release, then wait for a selection update
    // before pressing again. This is to ensure that the next selection update
    // corresponds to the second press (otherwise, we might return too early if
    // the first selection update only happens after the second press). Note
    // that the event generator uses its own mock clock to set the touch event
    // times, so there shouldn't be an issue with double press timing even if
    // the first selection update is delayed.
    selection_controller_client()->InitWaitForSelectionUpdate();
    generator.PressTouch(point);
    generator.ReleaseTouch();
    selection_controller_client()->Wait();

    // Perform a second press to select the word at `point`.
    selection_controller_client()->InitWaitForSelectionUpdate();
    generator.PressTouch(point);
    selection_controller_client()->Wait();
  }

  // Performs touch moves to initiate selection dragging after a long press or
  // double press gesture. Note that the first two touch moves after the
  // initiating long press or double press don't produce selection updates,
  // since they are instead used to set up the selection drag (e.g. set the
  // selection base and extent).
  void InitiateTouchSelectionDragging(ui::test::EventGenerator& generator) {
    generator.MoveTouchBy(10, 0);
    generator.MoveTouchBy(10, 0);
  }

  // Performs a touch move to adjust the selection and waits for the
  // corresponding selection update. This assumes that there is currently a
  // selection drag in progress.
  void DragAndWaitForSelectionUpdate(ui::test::EventGenerator& generator,
                                     int x,
                                     int y) {
    CHECK(selection_controller_client()->IsHandlingSelectionDrag());
    selection_controller_client()->InitWaitForSelectionUpdate();
    generator.MoveTouchBy(x, y);
    selection_controller_client()->Wait();
  }

  void InitSelectionController(bool enable_all_menu_commands) {
    RenderWidgetHostViewAura* rwhva = GetRenderWidgetHostViewAura();
    rwhva->SetSelectionControllerClientForTest(
        std::make_unique<TestTouchSelectionControllerClientAura>(
            rwhva, enable_all_menu_commands));
    // Simulate the start of a motion event sequence, since the tests assume it.
    rwhva->selection_controller()->WillHandleTouchEvent(
        ui::test::MockMotionEvent(ui::MotionEvent::Action::DOWN));
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    if (!ui::TouchSelectionMenuRunner::GetInstance())
      menu_runner_ = std::make_unique<TestTouchSelectionMenuRunner>();
    // Set a small tap slop.
    ui::GestureConfiguration::GetInstance()
        ->set_max_touch_move_in_pixels_for_click(5);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

 private:
  void TearDownOnMainThread() override {
    menu_runner_ = nullptr;
    ContentBrowserTest::TearDownOnMainThread();
  }

  std::unique_ptr<TestTouchSelectionMenuRunner> menu_runner_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(TouchSelectionControllerClientAuraTest,
                       InitiallyInactive) {
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/touch_selection.html"));
  InitSelectionController(true);

  EXPECT_EQ(
      GetRenderWidgetHostViewAura()->selection_controller()->active_status(),
      ui::TouchSelectionController::INACTIVE);
  EXPECT_EQ(GetRenderWidgetHostViewAura()
                ->selection_controller()
                ->GetVisibleRectBetweenBounds(),
            gfx::RectF());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
}

// Tests that long-pressing on a text brings up selection handles and the quick
// menu properly.
IN_PROC_BROWSER_TEST_F(TouchSelectionControllerClientAuraTest,
                       LongPressSelection) {
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/touch_selection.html"));
  InitSelectionController(true);

  // Long-press to select some text.
  RenderWidgetHostViewAura* rwhva = GetRenderWidgetHostViewAura();
  ui::test::EventGenerator generator(rwhva->GetNativeView()->GetRootWindow());
  SelectWithLongPress(generator,
                      ConvertPointFromView(rwhva, generator.delegate(),
                                           GetPointInText(/*cursor_index=*/2)));

  // Touch selection should be active.
  EXPECT_EQ(rwhva->selection_controller()->active_status(),
            ui::TouchSelectionController::SELECTION_ACTIVE);
  EXPECT_EQ(rwhva->GetSelectedText(), u"Some");
  EXPECT_EQ(rwhva->selection_controller()->GetVisibleRectBetweenBounds().size(),
            gfx::SizeF(4 * kCharacterWidth, kCharacterHeight));
  // Selection handles and menu should not be shown while the long press is
  // still being held down.
  ui::TouchSelectionControllerTestApi selection_controller_test_api(
      rwhva->selection_controller());
  EXPECT_FALSE(selection_controller_test_api.GetStartVisible());
  EXPECT_FALSE(selection_controller_test_api.GetEndVisible());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Release the long press.
  generator.ReleaseTouch();

  // Touch selection handles and menu should now be showing.
  EXPECT_EQ(rwhva->selection_controller()->active_status(),
            ui::TouchSelectionController::SELECTION_ACTIVE);
  EXPECT_EQ(rwhva->GetSelectedText(), u"Some");
  EXPECT_EQ(rwhva->selection_controller()->GetVisibleRectBetweenBounds().size(),
            gfx::SizeF(4 * kCharacterWidth, kCharacterHeight));
  EXPECT_TRUE(selection_controller_test_api.GetStartVisible());
  EXPECT_TRUE(selection_controller_test_api.GetEndVisible());
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
}

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

  gfx::PointF GetPointInTextInFrame(RenderFrameHostImpl* frame,
                                    int cursor_index) {
    gfx::PointF point_in_text;
    JSONToPoint(EvalJs(frame, "get_top_left_of_text()").ExtractString(),
                &point_in_text);
    point_in_text.Offset(cursor_index * kCharacterWidth,
                         0.5f * kCharacterHeight);
    return point_in_text;
  }

  gfx::Point ConvertPointFromChildFrame(
      RenderFrameHostImpl* child_frame,
      const ui::test::EventGeneratorDelegate* generator_delegate,
      const gfx::PointF& point_in_child_view) {
    return ConvertPointFromView(
        GetRenderWidgetHostViewAura(), generator_delegate,
        static_cast<RenderWidgetHostViewChildFrame*>(
            child_frame->GetRenderWidgetHost()->GetView())
            ->TransformPointToRootCoordSpaceF(point_in_child_view));
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

  InitSelectionController(true);

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

  RenderWidgetHostViewAura* parent_view = GetRenderWidgetHostViewAura();
  EXPECT_EQ(ui::TouchSelectionController::INACTIVE,
            parent_view->selection_controller()->active_status());
  EXPECT_EQ(gfx::RectF(),
            parent_view->selection_controller()->GetVisibleRectBetweenBounds());

  // Long press some text in the iframe to show selection handles.
  ui::test::EventGenerator generator(
      GetRenderWidgetHostViewAura()->GetNativeView()->GetRootWindow());
  const gfx::Point point_in_text = ConvertPointFromChildFrame(
      child->current_frame_host(), generator.delegate(),
      GetPointInTextInFrame(child->current_frame_host(), /*cursor_index=*/2));
  SelectWithLongPress(generator, point_in_text);
  generator.ReleaseTouch();

  // Check that selection is active and the quick menu is showing.
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            parent_view->selection_controller()->active_status());
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
  EXPECT_NE(gfx::RectF(),
            parent_view->selection_controller()->GetVisibleRectBetweenBounds());

  // Check that selection handles are cleared after tapping inside/outside the
  // iframe.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::SELECTION_HANDLES_CLEARED);
  const gfx::RectF rect_between_selection_bounds =
      parent_view->selection_controller()->GetRectBetweenBounds();
  if (GetParam()) {
    // Tap a point outside the iframe that doesn't overlap with the selection
    // handles or menu.
    const gfx::PointF point_outside_iframe(
        child_view->TransformPointToRootCoordSpaceF(gfx::PointF(-20.f, 0)).x(),
        rect_between_selection_bounds.left_center().y());
    generator.GestureTapAt(ConvertPointFromView(
        parent_view, generator.delegate(), point_outside_iframe));
  } else {
    // Tap a point inside the iframe that doesn't overlap with the selection
    // handles or menu.
    const gfx::PointF point_inside_iframe(
        rect_between_selection_bounds.right_center().x() + 10,
        rect_between_selection_bounds.right_center().y());
    generator.GestureTapAt(ConvertPointFromView(
        parent_view, generator.delegate(), point_inside_iframe));
  }
  selection_controller_client()->Wait();

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

  InitSelectionController(true);

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

  EXPECT_EQ(child_url, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());

  RenderWidgetHostViewAura* parent_view = GetRenderWidgetHostViewAura();
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

  // Long press some text in the iframe to show selection handles.
  ui::test::EventGenerator generator(
      GetRenderWidgetHostViewAura()->GetNativeView()->GetRootWindow());
  const gfx::Point point_in_text = ConvertPointFromChildFrame(
      child->current_frame_host(), generator.delegate(),
      GetPointInTextInFrame(child->current_frame_host(), /*cursor_index=*/2));
  SelectWithLongPress(generator, point_in_text);
  generator.ReleaseTouch();

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
      ui::EventType::kTouchPressed, scroll_start_position,
      ui::EventTimeForNow(),
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
      ui::EventType::kTouchMoved, scroll_end_position, ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  parent_view->OnTouchEvent(&touch_move);
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            selection_controller->active_status());
  EXPECT_FALSE(selection_controller_test_api.temporarily_hidden());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Start scrolling: touch handles should get hidden, while touch selection is
  // still active.
  ui::GestureEventDetails scroll_begin_details(
      ui::EventType::kGestureScrollBegin);
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
      ui::EventType::kGestureScrollUpdate, scroll_delta.x(), scroll_delta.y());
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
  ui::GestureEventDetails scroll_end_details(ui::EventType::kGestureScrollEnd);
  scroll_end_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent scroll_end(scroll_end_position.x(), scroll_end_position.y(),
                              0, ui::EventTimeForNow(), scroll_end_details);
  parent_view->OnGestureEvent(&scroll_end);
  EXPECT_FALSE(selection_controller_test_api.temporarily_hidden());
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            parent_view->selection_controller()->active_status());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // 3) Send touch-end.
  ui::TouchEvent touch_up(ui::EventType::kTouchReleased, scroll_end_position,
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

// Tests that the selection handles in a child view have their bounds updated
// when the main view is resized.
IN_PROC_BROWSER_TEST_P(TouchSelectionControllerClientAuraSiteIsolationTest,
                       SelectionHandlesIsolatedIframeMainViewResized) {
  // Set the test page up.
  const GURL test_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  EXPECT_EQ(DepictFrameTree(*root),
            " Site A\n"
            "   +--Site A\n"
            "Where A = http://a.com/");
  TestNavigationObserver observer(shell()->web_contents());
  EXPECT_EQ(root->child_count(), 1u);
  FrameTreeNode* child = root->child_at(0);

  InitSelectionController(true);
  RenderWidgetHostViewAura* parent_view = GetRenderWidgetHostViewAura();
  parent_view->SetSize(gfx::Size(600, 500));
  WaitForHitTestData(root->current_frame_host());

  const GURL child_url(
      embedded_test_server()->GetURL("b.com", "/touch_selection.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(child, child_url));
  EXPECT_EQ(DepictFrameTree(*root),
            " Site A ------------ proxies for B\n"
            "   +--Site B ------- proxies for A\n"
            "Where A = http://a.com/\n"
            "      B = http://b.com/");

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

  // Long press some text in the iframe to show selection handles.
  ui::test::EventGenerator generator(
      GetRenderWidgetHostViewAura()->GetNativeView()->GetRootWindow());
  const gfx::Point point_in_text = ConvertPointFromChildFrame(
      child->current_frame_host(), generator.delegate(),
      GetPointInTextInFrame(child->current_frame_host(), /*cursor_index=*/2));
  SelectWithLongPress(generator, point_in_text);
  generator.ReleaseTouch();

  // Selection handles should be shown.
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            parent_view->selection_controller()->active_status());
  const gfx::RectF initial_selection_bounds_in_parent_coords =
      parent_view->selection_controller()->GetRectBetweenBounds();
  EXPECT_NE(initial_selection_bounds_in_parent_coords, gfx::RectF());

  // Compute selection bounds in child view coordinates, to help determine the
  // expected selection bounds after resizing the parent view.
  const gfx::RectF initial_selection_bounds_in_child_coords =
      ConvertRectFToChildCoords(parent_view, child_view,
                                initial_selection_bounds_in_parent_coords);

  // Resize the parent view.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::SELECTION_HANDLES_MOVED);
  parent_view->SetSize(gfx::Size(200, 200));
  selection_controller_client()->Wait();

  // Resizing the parent view changes the position of the child view within this
  // parent, so we expect the selection handles to have moved and the selection
  // bounds in the parent view to have changed.
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            parent_view->selection_controller()->active_status());
  const gfx::RectF new_selection_bounds_in_parent_coords =
      parent_view->selection_controller()->GetRectBetweenBounds();
  EXPECT_NE(new_selection_bounds_in_parent_coords, gfx::RectF());
  EXPECT_NE(new_selection_bounds_in_parent_coords,
            initial_selection_bounds_in_parent_coords);
  // The selection bounds should remain the same in child view coordinates.
  EXPECT_EQ(ConvertRectFToChildCoords(parent_view, child_view,
                                      new_selection_bounds_in_parent_coords),
            initial_selection_bounds_in_child_coords);
}

IN_PROC_BROWSER_TEST_P(TouchSelectionControllerClientAuraSiteIsolationTest,
                       TouchSelectionDeactivatedAfterReload) {
  const GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  WaitForHitTestData(root->current_frame_host());

  InitSelectionController(true);

  {
    // Load touch selection test page into iframe.
    const GURL child_url(
        embedded_test_server()->GetURL("b.com", "/touch_selection.html"));
    EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), child_url));
    RenderFrameHostImpl* iframe = root->child_at(0)->current_frame_host();
    WaitForHitTestData(iframe);

    // Long press some text in the iframe to show selection handles.
    ui::test::EventGenerator generator(
        GetRenderWidgetHostViewAura()->GetNativeView()->GetRootWindow());
    const gfx::Point point_in_text = ConvertPointFromChildFrame(
        iframe, generator.delegate(),
        GetPointInTextInFrame(iframe, /*cursor_index=*/2));
    SelectWithLongPress(generator, point_in_text);
    generator.ReleaseTouch();
  }

  // Touch selection handles and menu should be active.
  EXPECT_EQ(
      ui::TouchSelectionController::SELECTION_ACTIVE,
      GetRenderWidgetHostViewAura()->selection_controller()->active_status());
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  {
    // Reload web contents.
    TestNavigationObserver reload_observer(shell()->web_contents());
    shell()->web_contents()->GetController().Reload(
        content::ReloadType::NORMAL, false /*check_for_repost=*/);
    reload_observer.Wait();
  }

  InitSelectionController(true);
  // Touch selection handles and menu should be deactivated.
  EXPECT_EQ(
      ui::TouchSelectionController::INACTIVE,
      GetRenderWidgetHostViewAura()->selection_controller()->active_status());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
}

// Tests that tapping in a textfield brings up the insertion handle, but not the
// quick menu, initially. Then, successive taps on the insertion handle toggle
// the quick menu visibility.
IN_PROC_BROWSER_TEST_F(TouchSelectionControllerClientAuraTest,
                       BasicInsertionFollowedByTapsOnHandle) {
  // Set the test page up.
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/touch_selection.html"));
  InitSelectionController(true);

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

  gfx::Point point = gfx::ToRoundedPoint(GetPointInTextfield(2));
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
// Tests that the text selection can be adjusted by touch dragging after a long
// press gesture on readable text.
IN_PROC_BROWSER_TEST_F(TouchSelectionControllerClientAuraTest,
                       LongPressDragSelectionReadableText) {
  // Set the test page up.
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/touch_selection.html"));

  InitSelectionController(false);
  RenderWidgetHostViewAura* rwhva = GetRenderWidgetHostViewAura();
  gfx::NativeView native_view = rwhva->GetNativeView();
  ui::test::EventGenerator generator(native_view->GetRootWindow());

  // Long pressing on readable text should select the closest word.
  gfx::Point point_in_readable_text = gfx::ToRoundedPoint(GetPointInText(2));
  generator.delegate()->ConvertPointFromTarget(native_view,
                                               &point_in_readable_text);
  SelectWithLongPress(generator, point_in_readable_text);
  EXPECT_EQ(rwhva->GetSelectedText(), u"Some");
  EXPECT_EQ(rwhva->selection_controller()->GetRectBetweenBounds().size(),
            gfx::SizeF(4 * kCharacterWidth, kCharacterHeight));

  // Drag after the long press to adjust the selection.
  InitiateTouchSelectionDragging(generator);

  // Move the end of the selection from "Some|" to "Some text|".
  DragAndWaitForSelectionUpdate(generator, 5 * kCharacterWidth, 0);
  EXPECT_EQ(rwhva->GetSelectedText(), u"Some text");
  EXPECT_EQ(rwhva->selection_controller()->GetRectBetweenBounds().size(),
            gfx::SizeF(9 * kCharacterWidth, kCharacterHeight));

  // Move the end of the selection from "Some text|" to "Some t|".
  DragAndWaitForSelectionUpdate(generator, -3 * kCharacterWidth, 0);
  EXPECT_EQ(rwhva->GetSelectedText(), u"Some t");
  EXPECT_EQ(rwhva->selection_controller()->GetRectBetweenBounds().size(),
            gfx::SizeF(6 * kCharacterWidth, kCharacterHeight));
}

// Tests that the text selection can be adjusted by touch dragging after a long
// press gesture on editable text.
IN_PROC_BROWSER_TEST_F(TouchSelectionControllerClientAuraTest,
                       LongPressDragSelectionEditableText) {
  // Set the test page up.
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/touch_selection.html"));

  InitSelectionController(false);
  RenderWidgetHostViewAura* rwhva = GetRenderWidgetHostViewAura();
  gfx::NativeView native_view = rwhva->GetNativeView();
  ui::test::EventGenerator generator(native_view->GetRootWindow());

  // Long pressing on editable text should select the closest word.
  gfx::Point point_in_textfield = gfx::ToRoundedPoint(GetPointInTextfield(2));
  generator.delegate()->ConvertPointFromTarget(native_view,
                                               &point_in_textfield);
  SelectWithLongPress(generator, point_in_textfield);
  EXPECT_EQ(rwhva->GetSelectedText(), u"Some");
  EXPECT_EQ(rwhva->selection_controller()->GetRectBetweenBounds().size(),
            gfx::SizeF(4 * kCharacterWidth, kCharacterHeight));

  // Drag after the long press to adjust the selection.
  InitiateTouchSelectionDragging(generator);

  // Move the end of the selection from "Some|" to "Some editable|".
  DragAndWaitForSelectionUpdate(generator, 9 * kCharacterWidth, 0);
  EXPECT_EQ(rwhva->GetSelectedText(), u"Some editable");
  EXPECT_EQ(rwhva->selection_controller()->GetRectBetweenBounds().size(),
            gfx::SizeF(13 * kCharacterWidth, kCharacterHeight));

  // Drag the end of the selection from "Some editable|" to "Some editabl|".
  DragAndWaitForSelectionUpdate(generator, -kCharacterWidth, 0);
  EXPECT_EQ(rwhva->GetSelectedText(), u"Some editabl");
  EXPECT_EQ(rwhva->selection_controller()->GetRectBetweenBounds().size(),
            gfx::SizeF(12 * kCharacterWidth, kCharacterHeight));
}

// Tests that the text selection can be adjusted by touch dragging after a
// double press gesture on editable text.
IN_PROC_BROWSER_TEST_F(TouchSelectionControllerClientAuraTest,
                       DoublePressDragSelection) {
  // Set the test page up.
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/touch_selection.html"));

  InitSelectionController(false);
  RenderWidgetHostViewAura* rwhva = GetRenderWidgetHostViewAura();
  gfx::NativeView native_view = rwhva->GetNativeView();
  ui::test::EventGenerator generator(native_view->GetRootWindow());

  // Double pressing on editable text should select the closest word.
  gfx::Point point_in_textfield = gfx::ToRoundedPoint(GetPointInTextfield(2));
  generator.delegate()->ConvertPointFromTarget(native_view,
                                               &point_in_textfield);
  SelectWithDoublePress(generator, point_in_textfield);
  EXPECT_EQ(rwhva->GetSelectedText(), u"Some");
  EXPECT_EQ(rwhva->selection_controller()->GetRectBetweenBounds().size(),
            gfx::SizeF(4 * kCharacterWidth, kCharacterHeight));

  // Drag after the double press to adjust the selection.
  InitiateTouchSelectionDragging(generator);

  // Move the end of the selection from "Some|" to "Some editable|".
  DragAndWaitForSelectionUpdate(generator, 9 * kCharacterWidth, 0);
  EXPECT_EQ(rwhva->GetSelectedText(), u"Some editable");
  EXPECT_EQ(rwhva->selection_controller()->GetRectBetweenBounds().size(),
            gfx::SizeF(13 * kCharacterWidth, kCharacterHeight));

  // Drag the end of the selection from "Some editable|" to "Some editabl|".
  DragAndWaitForSelectionUpdate(generator, -kCharacterWidth, 0);
  EXPECT_EQ(rwhva->GetSelectedText(), u"Some editabl");
  EXPECT_EQ(rwhva->selection_controller()->GetRectBetweenBounds().size(),
            gfx::SizeF(12 * kCharacterWidth, kCharacterHeight));
}

// Tests that touch selection dragging adjusts the selection using a direction
// strategy (roughly, expands by word and shrinks by character).
IN_PROC_BROWSER_TEST_F(TouchSelectionControllerClientAuraTest,
                       SelectionDraggingDirectionStrategy) {
  // Set the test page up.
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/touch_selection.html"));

  InitSelectionController(false);
  RenderWidgetHostViewAura* rwhva = GetRenderWidgetHostViewAura();
  gfx::NativeView native_view = rwhva->GetNativeView();
  ui::test::EventGenerator generator(native_view->GetRootWindow());

  // Double press in editable text to select the closest word.
  gfx::Point point_in_textfield = gfx::ToRoundedPoint(GetPointInTextfield(2));
  generator.delegate()->ConvertPointFromTarget(native_view,
                                               &point_in_textfield);
  SelectWithDoublePress(generator, point_in_textfield);
  EXPECT_EQ(rwhva->GetSelectedText(), u"Some");
  EXPECT_EQ(rwhva->selection_controller()->GetRectBetweenBounds().size(),
            gfx::SizeF(4 * kCharacterWidth, kCharacterHeight));

  // Drag the end of the selection from "Some|" to "Some edita|". The selection
  // should expand to the closest word boundary. Note that this creates an
  // offset of 3 characters between the drag position and end of the selection.
  InitiateTouchSelectionDragging(generator);
  DragAndWaitForSelectionUpdate(generator, 6 * kCharacterWidth, 0);
  EXPECT_EQ(rwhva->GetSelectedText(), u"Some editable");
  EXPECT_EQ(rwhva->selection_controller()->GetRectBetweenBounds().size(),
            gfx::SizeF(13 * kCharacterWidth, kCharacterHeight));

  // Drag the end of the selection from "Some editable|" to "Some ed|". The
  // selection should shrink to the nearest character boundary, preserving the
  // offset between the drag position and end of the selection.
  DragAndWaitForSelectionUpdate(generator, -6 * kCharacterWidth, 0);
  EXPECT_EQ(rwhva->GetSelectedText(), u"Some ed");
  EXPECT_EQ(rwhva->selection_controller()->GetRectBetweenBounds().size(),
            gfx::SizeF(7 * kCharacterWidth, kCharacterHeight));

  // Drag to remove the 3 character offset and move the end of the selection
  // from "Some ed|" to "Some edit|". Since we are adjusting within a word, the
  // selection expands with character granularity.
  DragAndWaitForSelectionUpdate(generator, 5 * kCharacterWidth, 0);
  EXPECT_EQ(rwhva->GetSelectedText(), u"Some edit");
  EXPECT_EQ(rwhva->selection_controller()->GetRectBetweenBounds().size(),
            gfx::SizeF(9 * kCharacterWidth, kCharacterHeight));

  // Drag the end of the selection from "Some edit|" to "Some editable tex|".
  // Since we have expanded past a word boundary, the selection should expand
  // with word granularity again.
  DragAndWaitForSelectionUpdate(generator, 8 * kCharacterWidth, 0);
  EXPECT_EQ(rwhva->GetSelectedText(), u"Some editable text");
  EXPECT_EQ(rwhva->selection_controller()->GetRectBetweenBounds().size(),
            gfx::SizeF(18 * kCharacterWidth, kCharacterHeight));
}

// Tests that a magnifier is shown when touch selection dragging.
IN_PROC_BROWSER_TEST_F(TouchSelectionControllerClientAuraTest,
                       SelectionDraggingShowsMagnifier) {
  // Set the test page up.
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/touch_selection.html"));

  InitSelectionController(false);
  RenderWidgetHostViewAura* rwhva = GetRenderWidgetHostViewAura();
  gfx::NativeView native_view = rwhva->GetNativeView();
  ui::test::EventGenerator generator(native_view->GetRootWindow());

  // Double press in textfield then start touch selection dragging.
  gfx::Point point_in_textfield = gfx::ToRoundedPoint(GetPointInTextfield(2));
  generator.delegate()->ConvertPointFromTarget(native_view,
                                               &point_in_textfield);
  SelectWithDoublePress(generator, point_in_textfield);
  InitiateTouchSelectionDragging(generator);

  // Drag to move the selection.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::SELECTION_HANDLES_MOVED);
  generator.MoveTouchBy(8 * kCharacterWidth, 0);
  selection_controller_client()->Wait();
  // Magnifier should be shown while dragging.
  EXPECT_TRUE(selection_controller_client()->IsMagnifierVisible());

  // Release touch to end the drag.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::SELECTION_HANDLE_DRAG_STOPPED);
  generator.ReleaseTouch();
  selection_controller_client()->Wait();
  // Magnifier should be hidden after dragging stops.
  EXPECT_FALSE(selection_controller_client()->IsMagnifierVisible());
}

// Tests that tapping the caret toggles showing and hiding the quick menu.
IN_PROC_BROWSER_TEST_F(TouchSelectionControllerClientAuraTest, TapOnCaret) {
  // Set the test page up.
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/touch_selection.html"));
  InitSelectionController(true);

  RenderWidgetHostViewAura* rwhva = GetRenderWidgetHostViewAura();
  const gfx::NativeView native_view = rwhva->GetNativeView();
  ui::test::EventGenerator generator(native_view->GetRootWindow());

  // Mouse click inside the textfield to make a caret appear.
  selection_controller_client()->InitWaitForSelectionUpdate();
  gfx::Point point = gfx::ToRoundedPoint(GetPointInTextfield(2));
  generator.delegate()->ConvertPointFromTarget(native_view, &point);
  generator.MoveMouseTo(point);
  generator.PressLeftButton();
  selection_controller_client()->Wait();
  EXPECT_EQ(rwhva->selection_controller()->active_status(),
            ui::TouchSelectionController::INACTIVE);
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Tap the caret to show an insertion handle and quick menu.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::INSERTION_HANDLE_SHOWN);
  selection_controller_client()->InitWaitForHandleContextMenu();
  generator.GestureTapAt(point);
  // Wait for both the insertion handle to be shown and for the context menu
  // event to be handled, since these might not occur in a fixed order.
  selection_controller_client()->Wait();
  selection_controller_client()->WaitForHandleContextMenu();
  EXPECT_EQ(rwhva->selection_controller()->active_status(),
            ui::TouchSelectionController::INSERTION_ACTIVE);
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Tap the caret again to hide the quick menu. We advance the clock before
  // tapping again to avoid the tap being treated as a double tap.
  generator.AdvanceClock(base::Milliseconds(1000));
  selection_controller_client()->InitWaitForHandleContextMenu();
  generator.GestureTapAt(point);
  selection_controller_client()->WaitForHandleContextMenu();
  // The insertion handle should still be shown but menu should be hidden.
  EXPECT_EQ(rwhva->selection_controller()->active_status(),
            ui::TouchSelectionController::INSERTION_ACTIVE);
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
}

// Tests that the insertion handle and menu are hidden when a mouse event
// occurs, then can be shown again by tapping on the caret.
IN_PROC_BROWSER_TEST_F(TouchSelectionControllerClientAuraTest,
                       HandleVisibilityAfterMouseEvent) {
  // Set the test page up.
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/touch_selection.html"));
  InitSelectionController(true);

  RenderWidgetHostViewAura* rwhva = GetRenderWidgetHostViewAura();
  const gfx::NativeView native_view = rwhva->GetNativeView();
  ui::test::EventGenerator generator(native_view->GetRootWindow());

  // Tap inside the textfield to place a caret and show an insertion handle.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::INSERTION_HANDLE_SHOWN);
  gfx::Point caret_location = gfx::ToRoundedPoint(GetPointInTextfield(2));
  generator.delegate()->ConvertPointFromTarget(native_view, &caret_location);
  generator.GestureTapAt(caret_location);
  selection_controller_client()->Wait();

  const gfx::RectF caret_bounds =
      rwhva->selection_controller()->GetVisibleRectBetweenBounds();
  EXPECT_NE(caret_bounds, gfx::RectF());
  EXPECT_EQ(rwhva->selection_controller()->active_status(),
            ui::TouchSelectionController::INSERTION_ACTIVE);
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Move the mouse to hide the insertion handle.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::INSERTION_HANDLE_CLEARED);
  generator.SendMouseEnter();
  generator.MoveMouseBy(100, 100);
  selection_controller_client()->Wait();

  EXPECT_EQ(rwhva->selection_controller()->GetVisibleRectBetweenBounds(),
            gfx::RectF());
  EXPECT_EQ(rwhva->selection_controller()->active_status(),
            ui::TouchSelectionController::INACTIVE);
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Tap the caret to show the menu and make the insertion handle reappear. We
  // advance the clock before tapping to avoid it being treated as a double tap.
  generator.AdvanceClock(base::Milliseconds(1000));
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::INSERTION_HANDLE_SHOWN);
  generator.GestureTapAt(caret_location);
  selection_controller_client()->Wait();

  EXPECT_EQ(rwhva->selection_controller()->GetVisibleRectBetweenBounds(),
            caret_bounds);
  EXPECT_EQ(rwhva->selection_controller()->active_status(),
            ui::TouchSelectionController::INSERTION_ACTIVE);
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Tests that touch selection dragging records a histogram entry.
IN_PROC_BROWSER_TEST_F(TouchSelectionControllerClientAuraTest,
                       SelectionDraggingMetrics) {
  // Set the test page up.
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/touch_selection.html"));

  base::HistogramTester histogram_tester;
  InitSelectionController(false);
  RenderWidgetHostViewAura* rwhva = GetRenderWidgetHostViewAura();
  gfx::NativeView native_view = rwhva->GetNativeView();
  ui::test::EventGenerator generator(native_view->GetRootWindow());
  gfx::Point point_in_textfield = gfx::ToRoundedPoint(GetPointInTextfield(2));
  generator.delegate()->ConvertPointFromTarget(native_view,
                                               &point_in_textfield);

  // Long press drag selection.
  SelectWithLongPress(generator, point_in_textfield);
  InitiateTouchSelectionDragging(generator);
  DragAndWaitForSelectionUpdate(generator, 9 * kCharacterWidth, 0);
  generator.ReleaseTouch();
  histogram_tester.ExpectBucketCount(ui::kTouchSelectionDragTypeHistogramName,
                                     ui::TouchSelectionDragType::kLongPressDrag,
                                     1);
  histogram_tester.ExpectTotalCount(ui::kTouchSelectionDragTypeHistogramName,
                                    1);

  // Double press drag selection. Close the menu if needed so that it doesn't
  // get in the way of the double press.
  ui::TouchSelectionMenuRunner::GetInstance()->CloseMenu();
  SelectWithDoublePress(generator, point_in_textfield);
  InitiateTouchSelectionDragging(generator);
  DragAndWaitForSelectionUpdate(generator, 10 * kCharacterWidth, 0);
  generator.ReleaseTouch();
  histogram_tester.ExpectBucketCount(
      ui::kTouchSelectionDragTypeHistogramName,
      ui::TouchSelectionDragType::kDoublePressDrag, 1);
  histogram_tester.ExpectTotalCount(ui::kTouchSelectionDragTypeHistogramName,
                                    2);

  // Start typing to end the touch selection session.
  generator.PressAndReleaseKey(ui::VKEY_A);
  histogram_tester.ExpectUniqueSample(
      ui::kTouchSelectionSessionTouchDownCountHistogramName, 3, 1);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Tests that the quick menu is hidden whenever a touch point is active.
// Flaky: https://crbug.com/803576
IN_PROC_BROWSER_TEST_F(TouchSelectionControllerClientAuraTest,
                       DISABLED_QuickMenuHiddenOnTouch) {
  // Set the test page up.
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/touch_selection.html"));
  InitSelectionController(true);

  RenderWidgetHostViewAura* rwhva = GetRenderWidgetHostViewAura();
  EXPECT_EQ(ui::TouchSelectionController::INACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
  EXPECT_EQ(gfx::RectF(),
            rwhva->selection_controller()->GetVisibleRectBetweenBounds());

  // Long-press on the text and wait for selection handles to appear.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::SELECTION_HANDLES_SHOWN);

  gfx::PointF point = GetPointInText(2);
  ui::GestureEventDetails long_press_details(ui::EventType::kGestureLongPress);
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
IN_PROC_BROWSER_TEST_F(TouchSelectionControllerClientAuraTest, HiddenOnScroll) {
  // Set the test page up.
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/touch_selection.html"));
  InitSelectionController(true);

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

  gfx::PointF point = GetPointInText(2);
  ui::GestureEventDetails long_press_details(ui::EventType::kGestureLongPress);
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
      ui::EventType::kTouchPressed, gfx::Point(10, 10), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  rwhva->OnTouchEvent(&touch_down);
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(selection_controller_test_api.temporarily_hidden());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Start scrolling: touch handles should get hidden, while touch selection is
  // still active.
  ui::GestureEventDetails scroll_begin_details(
      ui::EventType::kGestureScrollBegin);
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
  ui::GestureEventDetails scroll_end_details(ui::EventType::kGestureScrollEnd);
  scroll_end_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent scroll_end(10, 10, 0, ui::EventTimeForNow(),
                              scroll_end_details);
  rwhva->OnGestureEvent(&scroll_end);
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(selection_controller_test_api.temporarily_hidden());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Lift the finger up: the quick menu should re-appear.
  ui::TouchEvent touch_up(ui::EventType::kTouchReleased, gfx::Point(10, 10),
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
IN_PROC_BROWSER_TEST_F(TouchSelectionControllerClientAuraTest,
                       SwipeToMoveCursorMagnifier) {
  // Set the test page up.
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/touch_selection.html"));
  InitSelectionController(true);

  RenderWidgetHostViewAura* rwhva = GetRenderWidgetHostViewAura();
  gfx::NativeView native_view = rwhva->GetNativeView();
  ui::test::EventGenerator generator(native_view->GetRootWindow());
  EXPECT_FALSE(selection_controller_client()->IsMagnifierVisible());

  // Tap to focus the textfield.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::INSERTION_HANDLE_SHOWN);
  gfx::Point start = gfx::ToRoundedPoint(GetPointInTextfield(2));
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
        if (event_type == ui::EventType::kGestureScrollBegin) {
          selection_controller_client()->InitWaitForSelectionEvent(
              ui::INSERTION_HANDLE_MOVED);
        } else if (event_type == ui::EventType::kGestureScrollUpdate) {
          selection_controller_client()->Wait();
          EXPECT_TRUE(selection_controller_client()->IsMagnifierVisible());
          selection_controller_client()->InitWaitForSelectionEvent(
              ui::INSERTION_HANDLE_MOVED);
        }
      }));
  EXPECT_FALSE(selection_controller_client()->IsMagnifierVisible());
}

// Tests that the select all menu command works correctly.
IN_PROC_BROWSER_TEST_F(TouchSelectionControllerClientAuraTest,
                       SelectAllCommand) {
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/touch_selection.html"));
  InitSelectionController(false);
  RenderWidgetHostViewAura* rwhva = GetRenderWidgetHostViewAura();
  ui::TouchSelectionControllerTestApi selection_controller_test_api(
      rwhva->selection_controller());
  ui::test::EventGenerator generator(rwhva->GetNativeView()->GetRootWindow());

  // Tap inside the textfield and wait for the insertion cursor to appear.
  TapAndWaitForCursor(generator,
                      ConvertPointFromView(rwhva, generator.delegate(),
                                           GetPointInTextfield(2)));

  // Select all command should be enabled.
  EXPECT_TRUE(
      selection_controller_client()->GetActiveMenuClient()->IsCommandIdEnabled(
          ui::TouchEditable::kSelectAll));

  // Execute select all command.
  selection_controller_client()->InitWaitForSelectionUpdate();
  selection_controller_client()->GetActiveMenuClient()->ExecuteCommand(
      ui::TouchEditable::kSelectAll, 0);
  selection_controller_client()->Wait();

  // All text in the textfield should be selected. Touch handles and quick menu
  // should be shown and the select all command should now be disabled since all
  // text is already selected.
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
  EXPECT_TRUE(selection_controller_test_api.GetStartVisible());
  EXPECT_TRUE(selection_controller_test_api.GetEndVisible());
  EXPECT_EQ(
      selection_controller_client()->GetActiveMenuClient()->GetSelectedText(),
      u"Some editable text");
  EXPECT_FALSE(
      selection_controller_client()->GetActiveMenuClient()->IsCommandIdEnabled(
          ui::TouchEditable::kSelectAll));
}

// Tests that the select word menu command works correctly.
IN_PROC_BROWSER_TEST_F(TouchSelectionControllerClientAuraTest,
                       SelectWordCommand) {
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/touch_selection.html"));
  InitSelectionController(false);
  RenderWidgetHostViewAura* rwhva = GetRenderWidgetHostViewAura();
  ui::TouchSelectionControllerTestApi selection_controller_test_api(
      rwhva->selection_controller());
  ui::test::EventGenerator generator(rwhva->GetNativeView()->GetRootWindow());

  // Tap inside the textfield and wait for the insertion cursor to appear.
  TapAndWaitForCursor(generator,
                      ConvertPointFromView(rwhva, generator.delegate(),
                                           GetPointInTextfield(2)));

  // Select word command should be enabled.
  EXPECT_TRUE(
      selection_controller_client()->GetActiveMenuClient()->IsCommandIdEnabled(
          ui::TouchEditable::kSelectWord));

  // Execute select word command.
  selection_controller_client()->InitWaitForSelectionUpdate();
  selection_controller_client()->GetActiveMenuClient()->ExecuteCommand(
      ui::TouchEditable::kSelectWord, 0);
  selection_controller_client()->Wait();

  // The closest word should be selected. Touch handles and quick menu should be
  // shown and the select word command should now be disabled since there is a
  // non-empty selection.
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
  EXPECT_TRUE(selection_controller_test_api.GetStartVisible());
  EXPECT_TRUE(selection_controller_test_api.GetEndVisible());
  EXPECT_EQ(
      selection_controller_client()->GetActiveMenuClient()->GetSelectedText(),
      u"Some");
  EXPECT_FALSE(
      selection_controller_client()->GetActiveMenuClient()->IsCommandIdEnabled(
          ui::TouchEditable::kSelectWord));
}

// Tests that the select all and select word commands in the quick menu are
// disabled for empty textfields.
IN_PROC_BROWSER_TEST_F(TouchSelectionControllerClientAuraTest,
                       SelectCommandsEmptyTextfield) {
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/touch_selection.html"));
  InitSelectionController(false);
  RenderWidgetHostViewAura* rwhva = GetRenderWidgetHostViewAura();
  ui::test::EventGenerator generator(rwhva->GetNativeView()->GetRootWindow());

  // Long-press on an empty textfield, then release to make an insertion handle
  // appear.
  SelectWithLongPress(generator,
                      ConvertPointFromView(rwhva, generator.delegate(),
                                           GetPointInsideEmptyTextfield()));
  generator.ReleaseTouch();

  // Select all and select word commands should be disabled.
  EXPECT_FALSE(
      selection_controller_client()->GetActiveMenuClient()->IsCommandIdEnabled(
          ui::TouchEditable::kSelectAll));
  EXPECT_FALSE(
      selection_controller_client()->GetActiveMenuClient()->IsCommandIdEnabled(
          ui::TouchEditable::kSelectWord));
}

class TouchSelectionControllerClientAuraScaleFactorTest
    : public TouchSelectionControllerClientAuraTest {
 public:
  static constexpr float kScaleFactor = 2.0f;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    TouchSelectionControllerClientAuraTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "2");
  }
};

// Tests that selection handles are properly positioned at 2x DSF.
IN_PROC_BROWSER_TEST_F(TouchSelectionControllerClientAuraScaleFactorTest,
                       SelectionHandleCoordinates) {
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/touch_selection.html"));
  InitSelectionController(true);

  RenderWidgetHostViewAura* rwhva = GetRenderWidgetHostViewAura();
  const ui::TouchSelectionController* controller =
      rwhva->selection_controller();
  ui::test::EventGenerator generator(rwhva->GetNativeView()->GetRootWindow());

  // Long-press to select some text, then release to make selection handles
  // appear.
  SelectWithLongPress(
      generator,
      ConvertPointFromView(rwhva, generator.delegate(), GetPointInText(2)));
  generator.ReleaseTouch();

  // Selection bounds should be non-empty.
  const gfx::RectF initial_selection_bounds =
      controller->GetVisibleRectBetweenBounds();
  EXPECT_GT(initial_selection_bounds.width(), 0);
  EXPECT_EQ(initial_selection_bounds.height(), kCharacterHeight);

  // Handles should be shown just below the selection.
  const float start_handle_top = controller->GetStartHandleRect().y();
  const float end_handle_top = controller->GetEndHandleRect().y();
  EXPECT_EQ(start_handle_top, end_handle_top);
  EXPECT_LE(initial_selection_bounds.bottom(), end_handle_top);
  EXPECT_LE(end_handle_top, initial_selection_bounds.bottom() + 10);
}

// Tests that selection handle coordinates are updated after dragging at 2x DSF.
IN_PROC_BROWSER_TEST_F(TouchSelectionControllerClientAuraScaleFactorTest,
                       SelectionHandleCoordinatesAfterDrag) {
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/touch_selection.html"));
  InitSelectionController(true);

  RenderWidgetHostViewAura* rwhva = GetRenderWidgetHostViewAura();
  const ui::TouchSelectionController* controller =
      rwhva->selection_controller();
  ui::test::EventGenerator generator(rwhva->GetNativeView()->GetRootWindow());

  // Long-press to select some text, then release to make selection handles
  // appear.
  SelectWithLongPress(
      generator,
      ConvertPointFromView(rwhva, generator.delegate(), GetPointInText(2)));
  generator.ReleaseTouch();
  const gfx::RectF start_handle_rect = controller->GetStartHandleRect();
  const gfx::RectF end_handle_rect = controller->GetEndHandleRect();
  // Drag to move the end handle one character left. Close the menu if needed
  // before dragging so that it doesn't get in the way.
  ui::TouchSelectionMenuRunner::GetInstance()->CloseMenu();
  generator.PressTouch(ConvertPointFromView(rwhva, generator.delegate(),
                                            end_handle_rect.CenterPoint()));
  DragAndWaitForSelectionUpdate(generator, -kScaleFactor * kCharacterWidth, 0);

  // The start handle should have remained in its initial position while the end
  // handle should have been dragged one character left.
  EXPECT_EQ(controller->GetStartHandleRect(), start_handle_rect);
  EXPECT_EQ(controller->GetEndHandleRect(),
            end_handle_rect - gfx::Vector2dF(kCharacterWidth, 0));
}

// Tests that the menu is correctly shown after dragging a selection handle.
IN_PROC_BROWSER_TEST_F(TouchSelectionControllerClientAuraScaleFactorTest,
                       SelectionHandleDragShowsMenu) {
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/touch_selection.html"));

  InitSelectionController(true);
  RenderWidgetHostViewAura* rwhva = GetRenderWidgetHostViewAura();
  const ui::TouchSelectionController* controller =
      rwhva->selection_controller();
  ui::test::EventGenerator generator(rwhva->GetNativeView()->GetRootWindow());

  // Long-press to select some text, then release to make selection handles
  // appear.
  SelectWithLongPress(
      generator,
      ConvertPointFromView(rwhva, generator.delegate(), GetPointInText(2)));
  generator.ReleaseTouch();

  // Menu should be shown if no drag is in progress.
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Drag to move the end handle one character left. Close the menu before
  // dragging so that it doesn't get in the way.
  ui::TouchSelectionMenuRunner::GetInstance()->CloseMenu();
  generator.PressTouch(
      ConvertPointFromView(rwhva, generator.delegate(),
                           controller->GetEndHandleRect().CenterPoint()));
  DragAndWaitForSelectionUpdate(generator, -kScaleFactor * kCharacterWidth, 0);

  // Menu should remain hidden while dragging.
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Release touch to end the drag.
  generator.ReleaseTouch();

  // Menu should be shown after drag finishes.
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
}

// Tests that the magnifier is correctly shown when dragging a selection handle.
IN_PROC_BROWSER_TEST_F(TouchSelectionControllerClientAuraScaleFactorTest,
                       SelectionHandleDragShowsMagnifier) {
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/touch_selection.html"));

  InitSelectionController(true);
  RenderWidgetHostViewAura* rwhva = GetRenderWidgetHostViewAura();
  const ui::TouchSelectionController* controller =
      rwhva->selection_controller();
  ui::test::EventGenerator generator(rwhva->GetNativeView()->GetRootWindow());

  // Long-press to select some text, then release to make selection handles
  // appear.
  SelectWithLongPress(
      generator,
      ConvertPointFromView(rwhva, generator.delegate(), GetPointInText(2)));
  generator.ReleaseTouch();

  // Magnifier should be hidden if no drag is in progress.
  EXPECT_FALSE(selection_controller_client()->IsMagnifierVisible());

  // Drag to move the end handle one character left. Close the menu before
  // dragging so that it doesn't get in the way.
  ui::TouchSelectionMenuRunner::GetInstance()->CloseMenu();
  generator.PressTouch(
      ConvertPointFromView(rwhva, generator.delegate(),
                           controller->GetEndHandleRect().CenterPoint()));
  DragAndWaitForSelectionUpdate(generator, -kScaleFactor * kCharacterWidth, 0);

  // Magnifier should be shown while dragging the handle.
  EXPECT_TRUE(selection_controller_client()->IsMagnifierVisible());

  // Release touch to end the drag.
  generator.ReleaseTouch();

  // Magnifier should be hidden after the drag is released.
  EXPECT_FALSE(selection_controller_client()->IsMagnifierVisible());
}

// Tests that insertion handles are properly positioned at 2x DSF.
IN_PROC_BROWSER_TEST_F(TouchSelectionControllerClientAuraScaleFactorTest,
                       InsertionHandleCoordinates) {
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/touch_selection.html"));
  InitSelectionController(true);
  RenderWidgetHostViewAura* rwhva = GetRenderWidgetHostViewAura();
  ui::test::EventGenerator generator(rwhva->GetNativeView()->GetRootWindow());

  // Tap inside a textfield and wait for the insertion cursor to appear.
  TapAndWaitForCursor(generator,
                      ConvertPointFromView(rwhva, generator.delegate(),
                                           GetPointInTextfield(2)));

  // Cursor bounds should be a zero-width rect.
  const gfx::RectF cursor_bounds =
      rwhva->selection_controller()->GetVisibleRectBetweenBounds();
  EXPECT_EQ(cursor_bounds.size(), gfx::SizeF(0, kCharacterHeight));
  // Insertion handle should be shown just below the cursor.
  const gfx::RectF handle_rect =
      rwhva->selection_controller()->GetEndHandleRect();
  EXPECT_LE(cursor_bounds.bottom(), handle_rect.y());
  EXPECT_LE(handle_rect.y(), cursor_bounds.bottom() + 10);

  // Drag to move the cursor handle one character right.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::INSERTION_HANDLE_MOVED);
  generator.PressTouch(ConvertPointFromView(rwhva, generator.delegate(),
                                            handle_rect.CenterPoint()));
  generator.MoveTouchBy(kScaleFactor * kCharacterWidth, 0);
  selection_controller_client()->Wait();

  // Cursor handle should have moved one character right.
  EXPECT_EQ(rwhva->selection_controller()->GetEndHandleRect(),
            handle_rect + gfx::Vector2dF(kCharacterWidth, 0));
}

// Tests that the magnifier is correctly shown when dragging a insertion handle.
IN_PROC_BROWSER_TEST_F(TouchSelectionControllerClientAuraScaleFactorTest,
                       InsertionHandleDragShowsMagnifier) {
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/touch_selection.html"));
  InitSelectionController(true);
  RenderWidgetHostViewAura* rwhva = GetRenderWidgetHostViewAura();
  ui::test::EventGenerator generator(rwhva->GetNativeView()->GetRootWindow());

  // Tap inside a textfield and wait for the insertion cursor to appear.
  TapAndWaitForCursor(generator,
                      ConvertPointFromView(rwhva, generator.delegate(),
                                           GetPointInTextfield(2)));

  // Magnifier should be hidden if no drag is in progress.
  EXPECT_FALSE(selection_controller_client()->IsMagnifierVisible());

  // Drag to move the cursor handle one character right.
  const gfx::RectF handle_rect =
      rwhva->selection_controller()->GetEndHandleRect();
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::INSERTION_HANDLE_MOVED);
  generator.PressTouch(ConvertPointFromView(rwhva, generator.delegate(),
                                            handle_rect.CenterPoint()));
  generator.MoveTouchBy(kScaleFactor * kCharacterWidth, 0);
  selection_controller_client()->Wait();

  // Magnifier should be shown while dragging the handle.
  EXPECT_TRUE(selection_controller_client()->IsMagnifierVisible());

  // Release touch to end the drag.
  generator.ReleaseTouch();

  // Magnifier should be hidden after the drag is released.
  EXPECT_FALSE(selection_controller_client()->IsMagnifierVisible());
}

}  // namespace content
