// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/shell/browser/shell.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/test/event_generator.h"

namespace content {

class EventLatencyBrowserTest : public ContentBrowserTest {
 public:
  EventLatencyBrowserTest() = default;
  ~EventLatencyBrowserTest() override = default;

  EventLatencyBrowserTest(const EventLatencyBrowserTest&) = delete;
  EventLatencyBrowserTest& operator=(const EventLatencyBrowserTest&) = delete;

 protected:
  RenderWidgetHostImpl* GetWidgetHost() {
    return RenderWidgetHostImpl::From(shell()
                                          ->web_contents()
                                          ->GetPrimaryMainFrame()
                                          ->GetRenderViewHost()
                                          ->GetWidget());
  }

  // Starts the test server and navigates to the test page. Returns after the
  // navigation is complete.
  void LoadTestPage() {
    ASSERT_TRUE(embedded_test_server()->Start());

    // Navigate to the test page which has a rAF animation and a main thread
    // animation running.
    GURL test_url =
        embedded_test_server()->GetURL("/event-latency-animation.html");
    EXPECT_TRUE(NavigateToURL(shell(), test_url));

    aura::Window* content = shell()->web_contents()->GetContentNativeView();
    content->GetHost()->SetBoundsInPixels(gfx::Rect(800, 600));

    RenderWidgetHostImpl* host = GetWidgetHost();
    HitTestRegionObserver observer(host->GetFrameSinkId());

    // Wait for the hit test data to be ready.
    observer.WaitForHitTestData();
  }

  void FocusButton() const { ASSERT_TRUE(ExecJs(shell(), "focusButton()")); }

  void FocusInput() const { ASSERT_TRUE(ExecJs(shell(), "focusInput()")); }

  void StartAnimations() const {
    ASSERT_TRUE(ExecJs(shell(), "startAnimations()"));
  }
};

// Tests that if a key-press on a page causes a visual update, appropriate event
// latency metrics are reported.
// TODO(crbug.com/40132021): flaky test.
IN_PROC_BROWSER_TEST_F(EventLatencyBrowserTest, DISABLED_KeyPressOnButton) {
  base::HistogramTester histogram_tester;

  ASSERT_NO_FATAL_FAILURE(LoadTestPage());
  FocusButton();

  ui::test::EventGenerator generator(shell()
                                         ->web_contents()
                                         ->GetRenderWidgetHostView()
                                         ->GetNativeView()
                                         ->GetRootWindow());

  // Press and release the space key. Since the button on the test page is
  // focused, this should change the visuals of the button and generate
  // compositor frames with appropriate event latency metrics.
  generator.PressKey(ui::VKEY_SPACE, 0);
  generator.ReleaseKey(ui::VKEY_SPACE, 0);
  RunUntilInputProcessed(GetWidgetHost());

  FetchHistogramsFromChildProcesses();

  base::HistogramTester::CountsMap expected_counts = {
      {"EventLatency.KeyPressed.BrowserToRendererCompositor", 1},
      {"EventLatency.KeyPressed.BeginImplFrameToSendBeginMainFrame", 1},
      {"EventLatency.KeyPressed.SendBeginMainFrameToCommit", 1},
      {"EventLatency.KeyPressed.SendBeginMainFrameToCommit.HandleInputEvents",
       1},
      {"EventLatency.KeyPressed.SendBeginMainFrameToCommit.Animate", 1},
      {"EventLatency.KeyPressed.SendBeginMainFrameToCommit.StyleUpdate", 1},
      {"EventLatency.KeyPressed.SendBeginMainFrameToCommit.LayoutUpdate", 1},
      {"EventLatency.KeyPressed.SendBeginMainFrameToCommit.Prepaint", 1},
      {"EventLatency.KeyPressed.SendBeginMainFrameToCommit.Composite", 1},
      {"EventLatency.KeyPressed.SendBeginMainFrameToCommit.Paint", 1},
      {"EventLatency.KeyPressed.SendBeginMainFrameToCommit."
       "ScrollingCoordinator",
       1},
      {"EventLatency.KeyPressed.SendBeginMainFrameToCommit.CompositeCommit", 1},
      {"EventLatency.KeyPressed.SendBeginMainFrameToCommit.UpdateLayers", 1},
      {"EventLatency.KeyPressed.SendBeginMainFrameToCommit."
       "BeginMainSentToStarted",
       1},
      {"EventLatency.KeyPressed.Commit", 1},
      {"EventLatency.KeyPressed.EndCommitToActivation", 1},
      {"EventLatency.KeyPressed.Activation", 1},
      {"EventLatency.KeyPressed.EndActivateToSubmitCompositorFrame", 1},
      {"EventLatency.KeyPressed."
       "SubmitCompositorFrameToPresentationCompositorFrame",
       1},
      {"EventLatency.KeyPressed."
       "SubmitCompositorFrameToPresentationCompositorFrame."
       "SubmitToReceiveCompositorFrame",
       1},
      {"EventLatency.KeyPressed."
       "SubmitCompositorFrameToPresentationCompositorFrame."
       "ReceivedCompositorFrameToStartDraw",
       1},
      {"EventLatency.KeyPressed."
       "SubmitCompositorFrameToPresentationCompositorFrame."
       "StartDrawToSwapStart",
       1},
      {"EventLatency.KeyPressed."
       "SubmitCompositorFrameToPresentationCompositorFrame.SwapStartToSwapEnd",
       1},
      {"EventLatency.KeyPressed."
       "SubmitCompositorFrameToPresentationCompositorFrame."
       "SwapEndToPresentationCompositorFrame",
       1},
      {"EventLatency.KeyPressed.TotalLatency", 1},
      {"EventLatency.KeyReleased.BrowserToRendererCompositor", 1},
      {"EventLatency.KeyReleased.BeginImplFrameToSendBeginMainFrame", 1},
      {"EventLatency.KeyReleased.SendBeginMainFrameToCommit", 1},
      {"EventLatency.KeyReleased.SendBeginMainFrameToCommit.HandleInputEvents",
       1},
      {"EventLatency.KeyReleased.SendBeginMainFrameToCommit.Animate", 1},
      {"EventLatency.KeyReleased.SendBeginMainFrameToCommit.StyleUpdate", 1},
      {"EventLatency.KeyReleased.SendBeginMainFrameToCommit.LayoutUpdate", 1},
      {"EventLatency.KeyReleased.SendBeginMainFrameToCommit.Prepaint", 1},
      {"EventLatency.KeyReleased.SendBeginMainFrameToCommit.Composite", 1},
      {"EventLatency.KeyReleased.SendBeginMainFrameToCommit.Paint", 1},
      {"EventLatency.KeyReleased.SendBeginMainFrameToCommit."
       "ScrollingCoordinator",
       1},
      {"EventLatency.KeyReleased.SendBeginMainFrameToCommit.CompositeCommit",
       1},
      {"EventLatency.KeyReleased.SendBeginMainFrameToCommit.UpdateLayers", 1},
      {"EventLatency.KeyReleased.SendBeginMainFrameToCommit."
       "BeginMainSentToStarted",
       1},
      {"EventLatency.KeyReleased.Commit", 1},
      {"EventLatency.KeyReleased.EndCommitToActivation", 1},
      {"EventLatency.KeyReleased.Activation", 1},
      {"EventLatency.KeyReleased.EndActivateToSubmitCompositorFrame", 1},
      {"EventLatency.KeyReleased."
       "SubmitCompositorFrameToPresentationCompositorFrame",
       1},
      {"EventLatency.KeyReleased."
       "SubmitCompositorFrameToPresentationCompositorFrame."
       "SubmitToReceiveCompositorFrame",
       1},
      {"EventLatency.KeyReleased."
       "SubmitCompositorFrameToPresentationCompositorFrame."
       "ReceivedCompositorFrameToStartDraw",
       1},
      {"EventLatency.KeyReleased."
       "SubmitCompositorFrameToPresentationCompositorFrame."
       "StartDrawToSwapStart",
       1},
      {"EventLatency.KeyReleased."
       "SubmitCompositorFrameToPresentationCompositorFrame.SwapStartToSwapEnd",
       1},
      {"EventLatency.KeyReleased."
       "SubmitCompositorFrameToPresentationCompositorFrame."
       "SwapEndToPresentationCompositorFrame",
       1},
      {"EventLatency.KeyReleased.TotalLatency", 1},
      {"EventLatency.TotalLatency", 2},
  };
  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix("EventLatency."),
              testing::ContainerEq(expected_counts));
}

// Tests that if a key-press on a page with an animation causes a visual update,
// appropriate event latency metrics are reported.
// TODO(crbug.com/40128555): Test is flaky.
IN_PROC_BROWSER_TEST_F(EventLatencyBrowserTest,
                       DISABLED_KeyPressOnButtonWithAnimation) {
  base::HistogramTester histogram_tester;

  ASSERT_NO_FATAL_FAILURE(LoadTestPage());
  StartAnimations();
  FocusButton();

  ui::test::EventGenerator generator(shell()
                                         ->web_contents()
                                         ->GetRenderWidgetHostView()
                                         ->GetNativeView()
                                         ->GetRootWindow());

  // Press and release the space key. Since the button on the test page is
  // focused, this should change the visuals of the button and generate
  // compositor frames with appropriate event latency metrics.
  generator.PressKey(ui::VKEY_SPACE, 0);
  generator.ReleaseKey(ui::VKEY_SPACE, 0);
  RunUntilInputProcessed(GetWidgetHost());

  FetchHistogramsFromChildProcesses();

  base::HistogramTester::CountsMap expected_counts = {
      {"EventLatency.KeyPressed.BrowserToRendererCompositor", 1},
      {"EventLatency.KeyPressed.BeginImplFrameToSendBeginMainFrame", 1},
      {"EventLatency.KeyPressed.SendBeginMainFrameToCommit", 1},
      {"EventLatency.KeyPressed.SendBeginMainFrameToCommit.HandleInputEvents",
       1},
      {"EventLatency.KeyPressed.SendBeginMainFrameToCommit.Animate", 1},
      {"EventLatency.KeyPressed.SendBeginMainFrameToCommit.StyleUpdate", 1},
      {"EventLatency.KeyPressed.SendBeginMainFrameToCommit.LayoutUpdate", 1},
      {"EventLatency.KeyPressed.SendBeginMainFrameToCommit.Prepaint", 1},
      {"EventLatency.KeyPressed.SendBeginMainFrameToCommit.Composite", 1},
      {"EventLatency.KeyPressed.SendBeginMainFrameToCommit.Paint", 1},
      {"EventLatency.KeyPressed.SendBeginMainFrameToCommit."
       "ScrollingCoordinator",
       1},
      {"EventLatency.KeyPressed.SendBeginMainFrameToCommit.CompositeCommit", 1},
      {"EventLatency.KeyPressed.SendBeginMainFrameToCommit.UpdateLayers", 1},
      {"EventLatency.KeyPressed.SendBeginMainFrameToCommit."
       "BeginMainSentToStarted",
       1},
      {"EventLatency.KeyPressed.Commit", 1},
      {"EventLatency.KeyPressed.EndCommitToActivation", 1},
      {"EventLatency.KeyPressed.Activation", 1},
      {"EventLatency.KeyPressed.EndActivateToSubmitCompositorFrame", 1},
      {"EventLatency.KeyPressed."
       "SubmitCompositorFrameToPresentationCompositorFrame",
       1},
      {"EventLatency.KeyPressed.TotalLatency", 1},
      {"EventLatency.KeyPressed."
       "SubmitCompositorFrameToPresentationCompositorFrame."
       "SubmitToReceiveCompositorFrame",
       1},
      {"EventLatency.KeyPressed."
       "SubmitCompositorFrameToPresentationCompositorFrame."
       "ReceivedCompositorFrameToStartDraw",
       1},
      {"EventLatency.KeyPressed."
       "SubmitCompositorFrameToPresentationCompositorFrame."
       "StartDrawToSwapStart",
       1},
      {"EventLatency.KeyPressed."
       "SubmitCompositorFrameToPresentationCompositorFrame.SwapStartToSwapEnd",
       1},
      {"EventLatency.KeyPressed."
       "SubmitCompositorFrameToPresentationCompositorFrame."
       "SwapEndToPresentationCompositorFrame",
       1},
      {"EventLatency.KeyReleased.BrowserToRendererCompositor", 1},
      {"EventLatency.KeyReleased.BeginImplFrameToSendBeginMainFrame", 1},
      {"EventLatency.KeyReleased.SendBeginMainFrameToCommit", 1},
      {"EventLatency.KeyReleased.SendBeginMainFrameToCommit.HandleInputEvents",
       1},
      {"EventLatency.KeyReleased.SendBeginMainFrameToCommit.Animate", 1},
      {"EventLatency.KeyReleased.SendBeginMainFrameToCommit.StyleUpdate", 1},
      {"EventLatency.KeyReleased.SendBeginMainFrameToCommit.LayoutUpdate", 1},
      {"EventLatency.KeyReleased.SendBeginMainFrameToCommit.Prepaint", 1},
      {"EventLatency.KeyReleased.SendBeginMainFrameToCommit.Composite", 1},
      {"EventLatency.KeyReleased.SendBeginMainFrameToCommit.Paint", 1},
      {"EventLatency.KeyReleased.SendBeginMainFrameToCommit."
       "ScrollingCoordinator",
       1},
      {"EventLatency.KeyReleased.SendBeginMainFrameToCommit.CompositeCommit",
       1},
      {"EventLatency.KeyReleased.SendBeginMainFrameToCommit.UpdateLayers", 1},
      {"EventLatency.KeyReleased.SendBeginMainFrameToCommit."
       "BeginMainSentToStarted",
       1},
      {"EventLatency.KeyReleased.Commit", 1},
      {"EventLatency.KeyReleased.EndCommitToActivation", 1},
      {"EventLatency.KeyReleased.Activation", 1},
      {"EventLatency.KeyReleased.EndActivateToSubmitCompositorFrame", 1},
      {"EventLatency.KeyReleased."
       "SubmitCompositorFrameToPresentationCompositorFrame",
       1},
      {"EventLatency.KeyReleased."
       "SubmitCompositorFrameToPresentationCompositorFrame."
       "SubmitToReceiveCompositorFrame",
       1},
      {"EventLatency.KeyReleased."
       "SubmitCompositorFrameToPresentationCompositorFrame."
       "ReceivedCompositorFrameToStartDraw",
       1},
      {"EventLatency.KeyReleased."
       "SubmitCompositorFrameToPresentationCompositorFrame."
       "StartDrawToSwapStart",
       1},
      {"EventLatency.KeyReleased."
       "SubmitCompositorFrameToPresentationCompositorFrame.SwapStartToSwapEnd",
       1},
      {"EventLatency.KeyReleased."
       "SubmitCompositorFrameToPresentationCompositorFrame."
       "SwapEndToPresentationCompositorFrame",
       1},
      {"EventLatency.KeyReleased.TotalLatency", 1},
      {"EventLatency.TotalLatency", 2},
  };
  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix("EventLatency."),
              testing::ContainerEq(expected_counts));
}

// Tests that entering a character in a textbox leads to appropriate event
// latency metrics being reported even though the page has an animation running.
//
// Disabled due to flakiness on several platforms. See crbug.com/1072340
IN_PROC_BROWSER_TEST_F(EventLatencyBrowserTest,
                       DISABLED_KeyPressInInputWithAnimation) {
  base::HistogramTester histogram_tester;

  ASSERT_NO_FATAL_FAILURE(LoadTestPage());
  StartAnimations();
  FocusInput();

  ui::test::EventGenerator generator(shell()
                                         ->web_contents()
                                         ->GetRenderWidgetHostView()
                                         ->GetNativeView()
                                         ->GetRootWindow());

  // Enter a character into the focused textbox. This should generate compositor
  // frames with appropriate event latency metrics.
  generator.PressKey(ui::VKEY_A, 0);
  generator.ReleaseKey(ui::VKEY_A, 0);
  RunUntilInputProcessed(GetWidgetHost());

  FetchHistogramsFromChildProcesses();

  // TODO(crbug.com/40126863): Since this is this first key-press after the
  // textbox is focused, there would be two reports, one for the RawKeyDown that
  // causes some style changes (due to :focus-visible behavior) and one for the
  // Char that inserts the actual character. These should be reported
  // separately.
  base::HistogramTester::CountsMap expected_counts = {
      {"EventLatency.KeyPressed.BrowserToRendererCompositor", 2},
      {"EventLatency.KeyPressed.BeginImplFrameToSendBeginMainFrame", 2},
      {"EventLatency.KeyPressed.SendBeginMainFrameToCommit", 2},
      {"EventLatency.KeyPressed.SendBeginMainFrameToCommit.HandleInputEvents",
       2},
      {"EventLatency.KeyPressed.SendBeginMainFrameToCommit.Animate", 2},
      {"EventLatency.KeyPressed.SendBeginMainFrameToCommit.StyleUpdate", 2},
      {"EventLatency.KeyPressed.SendBeginMainFrameToCommit.LayoutUpdate", 2},
      {"EventLatency.KeyPressed.SendBeginMainFrameToCommit.Prepaint", 2},
      {"EventLatency.KeyPressed.SendBeginMainFrameToCommit.Composite", 2},
      {"EventLatency.KeyPressed.SendBeginMainFrameToCommit.Paint", 2},
      {"EventLatency.KeyPressed.SendBeginMainFrameToCommit."
       "ScrollingCoordinator",
       2},
      {"EventLatency.KeyPressed.SendBeginMainFrameToCommit.CompositeCommit", 2},
      {"EventLatency.KeyPressed.SendBeginMainFrameToCommit.UpdateLayers", 2},
      {"EventLatency.KeyPressed.SendBeginMainFrameToCommit."
       "BeginMainSentToStarted",
       2},
      {"EventLatency.KeyPressed.Commit", 2},
      {"EventLatency.KeyPressed.EndCommitToActivation", 2},
      {"EventLatency.KeyPressed.Activation", 2},
      {"EventLatency.KeyPressed.EndActivateToSubmitCompositorFrame", 2},
      {"EventLatency.KeyPressed."
       "SubmitCompositorFrameToPresentationCompositorFrame",
       2},
      {"EventLatency.KeyPressed."
       "SubmitCompositorFrameToPresentationCompositorFrame."
       "SubmitToReceiveCompositorFrame",
       2},
      {"EventLatency.KeyPressed."
       "SubmitCompositorFrameToPresentationCompositorFrame."
       "ReceivedCompositorFrameToStartDraw",
       2},
      {"EventLatency.KeyPressed."
       "SubmitCompositorFrameToPresentationCompositorFrame."
       "StartDrawToSwapStart",
       2},
      {"EventLatency.KeyPressed."
       "SubmitCompositorFrameToPresentationCompositorFrame.SwapStartToSwapEnd",
       2},
      {"EventLatency.KeyPressed."
       "SubmitCompositorFrameToPresentationCompositorFrame."
       "SwapEndToPresentationCompositorFrame",
       2},
      {"EventLatency.KeyPressed.TotalLatency", 2},
      {"EventLatency.TotalLatency", 2},
  };
  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix("EventLatency."),
              testing::ContainerEq(expected_counts));
}

}  // namespace content
