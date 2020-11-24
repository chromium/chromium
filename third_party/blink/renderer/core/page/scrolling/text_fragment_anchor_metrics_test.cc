// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/text_fragment_anchor_metrics.h"

#include "base/test/simple_test_tick_clock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/scroll/scroll_enums.mojom-blink.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/page/scrolling/text_fragment_anchor.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/histogram_tester.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

using test::RunPendingTasks;

class TextFragmentAnchorMetricsTest : public SimTest {
 public:
  void SetUp() override {
    SimTest::SetUp();
    WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  }

  void RunAsyncMatchingTasks() {
    auto* scheduler =
        ThreadScheduler::Current()->GetWebMainThreadSchedulerForTest();
    blink::scheduler::RunIdleTasksForTesting(scheduler,
                                             base::BindOnce([]() {}));
    RunPendingTasks();
  }

  void SimulateClick(int x, int y) {
    WebMouseEvent event(WebInputEvent::Type::kMouseDown, gfx::PointF(x, y),
                        gfx::PointF(x, y), WebPointerProperties::Button::kLeft,
                        0, WebInputEvent::Modifiers::kLeftButtonDown,
                        base::TimeTicks::Now());
    event.SetFrameScale(1);
    GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(event);
  }

  void BeginEmptyFrame() {
    // If a test case doesn't find a match and therefore doesn't schedule the
    // beforematch event, we should still render a second frame as if we did
    // schedule the event to retain test coverage.
    // When the beforematch event is not scheduled, a DCHECK will fail on
    // BeginFrame() because no event was scheduled, so we schedule an empty task
    // here.
    GetDocument().EnqueueAnimationFrameTask(WTF::Bind([]() {}));
    Compositor().BeginFrame();
  }

  HistogramTester histogram_tester_;
};

// Test UMA metrics collection
TEST_F(TextFragmentAnchorMetricsTest, UMAMetricsCollected) {
  SimRequest request("https://example.com/test.html#:~:text=test&text=cat",
                     "text/html");
  LoadURL("https://example.com/test.html#:~:text=test&text=cat");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 1200px;
      }
      p {
        position: absolute;
        top: 1000px;
      }
    </style>
    <p>This is a test page</p>
    <p>With ambiguous test content</p>
  )HTML");
  RunAsyncMatchingTasks();

  // Render two frames to handle the async step added by the beforematch event.
  Compositor().BeginFrame();
  BeginEmptyFrame();

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.SelectorCount",
                                     1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.SelectorCount", 2, 1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.MatchRate", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.Unknown.MatchRate",
                                       50, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.AmbiguousMatch", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.AmbiguousMatch", 1, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.ScrollCancelled", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.ScrollCancelled", 0, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.DidScrollIntoView", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.DidScrollIntoView", 1, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.TimeToScrollIntoView", 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.DirectiveLength", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.DirectiveLength", 18, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.ExactTextLength", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.ExactTextLength", 4, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.RangeMatchLength", 0);
  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.StartTextLength", 0);
  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.EndTextLength",
                                     0);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.Parameters",
                                     1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.Parameters",
      static_cast<int>(
          TextFragmentAnchorMetrics::TextFragmentAnchorParameters::kExactText),
      1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.TimeToScrollToTop", 0);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.ListItemMatch",
                                     1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.ListItemMatch", 0, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.TableCellMatch", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.TableCellMatch", 0, 1);
  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.LinkOpenSource", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.LinkOpenSource", 0,
                                       1);
}

// Test UMA metrics collection with search engine referrer.
TEST_F(TextFragmentAnchorMetricsTest, UMAMetricsCollectedSearchEngineReferrer) {
  // Set the referrer to a known search engine URL. This should cause metrics
  // to be reported for the SearchEngine variant of histograms.
  SimRequest::Params params;
  params.referrer = "https://www.bing.com";

  SimRequest request("https://example.com/test.html#:~:text=test&text=cat",
                     "text/html", params);
  LoadURL("https://example.com/test.html#:~:text=test&text=cat");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 1200px;
      }
      p {
        position: absolute;
        top: 1000px;
      }
    </style>
    <p>This is a test page</p>
    <p>With ambiguous test content</p>
  )HTML");
  RunAsyncMatchingTasks();

  // Render two frames to handle the async step added by the beforematch event.
  Compositor().BeginFrame();
  BeginEmptyFrame();

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.SearchEngine.SelectorCount", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.SearchEngine.SelectorCount", 2, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.SearchEngine.MatchRate", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.SearchEngine.MatchRate", 50, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.SearchEngine.AmbiguousMatch", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.SearchEngine.AmbiguousMatch", 1, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.SearchEngine.ScrollCancelled", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.SearchEngine.ScrollCancelled", 0, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.SearchEngine.DidScrollIntoView", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.SearchEngine.DidScrollIntoView", 1, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.SearchEngine.TimeToScrollIntoView", 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.SearchEngine.DirectiveLength", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.SearchEngine.DirectiveLength", 18, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.SearchEngine.ExactTextLength", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.SearchEngine.ExactTextLength", 4, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.SearchEngine.RangeMatchLength", 0);
  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.SearchEngine.StartTextLength", 0);
  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.SearchEngine.EndTextLength", 0);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.SearchEngine.Parameters", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.SearchEngine.Parameters",
      static_cast<int>(
          TextFragmentAnchorMetrics::TextFragmentAnchorParameters::kExactText),
      1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.SearchEngine.TimeToScrollToTop", 0);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.SearchEngine.ListItemMatch", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.SearchEngine.ListItemMatch", 0, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.SearchEngine.TableCellMatch", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.SearchEngine.TableCellMatch", 0, 1);
  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.LinkOpenSource", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.LinkOpenSource", 1,
                                       1);
}

// Test UMA metrics collection when there is no match found
TEST_F(TextFragmentAnchorMetricsTest, NoMatchFound) {
  SimRequest request("https://example.com/test.html#:~:text=cat", "text/html");
  LoadURL("https://example.com/test.html#:~:text=cat");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 1200px;
      }
      p {
        position: absolute;
        top: 1000px;
      }
    </style>
    <p>This is a test page</p>
  )HTML");
  RunAsyncMatchingTasks();

  // Render two frames to handle the async step added by the beforematch event.
  Compositor().BeginFrame();
  BeginEmptyFrame();

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.SelectorCount",
                                     1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.SelectorCount", 1, 1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.MatchRate", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.Unknown.MatchRate",
                                       0, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.AmbiguousMatch", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.AmbiguousMatch", 0, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.ScrollCancelled", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.ScrollCancelled", 0, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.DidScrollIntoView", 0);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.TimeToScrollIntoView", 0);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.DirectiveLength", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.DirectiveLength", 8, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.ExactTextLength", 0);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.RangeMatchLength", 0);
  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.StartTextLength", 0);
  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.EndTextLength",
                                     0);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.Parameters",
                                     0);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.TimeToScrollToTop", 0);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.ListItemMatch",
                                     0);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.TableCellMatch", 0);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.LinkOpenSource", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.LinkOpenSource", 0,
                                       1);
}

// Test that we don't collect any metrics when there is no text directive
TEST_F(TextFragmentAnchorMetricsTest, NoTextFragmentAnchor) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p>This is a test page</p>
  )HTML");
  // Render two frames to handle the async step added by the beforematch event.
  Compositor().BeginFrame();
  BeginEmptyFrame();

  RunAsyncMatchingTasks();

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.SelectorCount",
                                     0);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.MatchRate", 0);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.AmbiguousMatch", 0);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.ScrollCancelled", 0);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.DidScrollIntoView", 0);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.TimeToScrollIntoView", 0);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.DirectiveLength", 0);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.ExactTextLength", 0);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.RangeMatchLength", 0);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.StartTextLength", 0);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.EndTextLength",
                                     0);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.Parameters",
                                     0);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.TimeToScrollToTop", 0);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.ListItemMatch",
                                     0);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.TableCellMatch", 0);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.LinkOpenSource", 0);
}

// Test that the correct metrics are collected when we found a match but didn't
// need to scroll.
TEST_F(TextFragmentAnchorMetricsTest, MatchFoundNoScroll) {
  SimRequest request("https://example.com/test.html#:~:text=test", "text/html");
  LoadURL("https://example.com/test.html#:~:text=test");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p>This is a test page</p>
  )HTML");
  RunAsyncMatchingTasks();

  // Render two frames to handle the async step added by the beforematch event.
  Compositor().BeginFrame();
  BeginEmptyFrame();

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.SelectorCount",
                                     1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.SelectorCount", 1, 1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.MatchRate", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.Unknown.MatchRate",
                                       100, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.AmbiguousMatch", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.AmbiguousMatch", 0, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.ScrollCancelled", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.ScrollCancelled", 0, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.DidScrollIntoView", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.DidScrollIntoView", 0, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.TimeToScrollIntoView", 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.DirectiveLength", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.DirectiveLength", 9, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.ExactTextLength", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.ExactTextLength", 4, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.RangeMatchLength", 0);
  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.StartTextLength", 0);
  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.EndTextLength",
                                     0);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.Parameters",
                                     1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.Parameters",
      static_cast<int>(
          TextFragmentAnchorMetrics::TextFragmentAnchorParameters::kExactText),
      1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.TimeToScrollToTop", 0);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.ListItemMatch",
                                     1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.ListItemMatch", 0, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.TableCellMatch", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.TableCellMatch", 0, 1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.LinkOpenSource", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.LinkOpenSource", 0,
                                       1);
}

// Test that the correct metrics are collected for all possible combinations of
// context terms on an exact text directive.
TEST_F(TextFragmentAnchorMetricsTest, ExactTextParameters) {
  SimRequest request(
      "https://example.com/"
      "test.html#:~:text=this&text=is-,a&text=test,-page&text=with-,some,-"
      "content",
      "text/html");
  LoadURL(
      "https://example.com/"
      "test.html#:~:text=this&text=is-,a&text=test,-page&text=with-,some,-"
      "content");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p>This is a test page</p>
    <p>With some content</p>
  )HTML");
  RunAsyncMatchingTasks();

  Compositor().BeginFrame();
  BeginEmptyFrame();

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.SelectorCount",
                                     1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.SelectorCount", 4, 1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.MatchRate", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.Unknown.MatchRate",
                                       100, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.AmbiguousMatch", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.AmbiguousMatch", 0, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.ScrollCancelled", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.ScrollCancelled", 0, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.DidScrollIntoView", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.DidScrollIntoView", 0, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.TimeToScrollIntoView", 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.DirectiveLength", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.DirectiveLength", 61, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.ExactTextLength", 4);
  // "this", "test", "some"
  histogram_tester_.ExpectBucketCount(
      "TextFragmentAnchor.Unknown.ExactTextLength", 4, 3);
  // "a"
  histogram_tester_.ExpectBucketCount(
      "TextFragmentAnchor.Unknown.ExactTextLength", 1, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.RangeMatchLength", 0);
  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.StartTextLength", 0);
  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.EndTextLength",
                                     0);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.Parameters",
                                     4);
  histogram_tester_.ExpectBucketCount(
      "TextFragmentAnchor.Unknown.Parameters",
      static_cast<int>(
          TextFragmentAnchorMetrics::TextFragmentAnchorParameters::kExactText),
      1);
  histogram_tester_.ExpectBucketCount(
      "TextFragmentAnchor.Unknown.Parameters",
      static_cast<int>(TextFragmentAnchorMetrics::TextFragmentAnchorParameters::
                           kExactTextWithPrefix),
      1);
  histogram_tester_.ExpectBucketCount(
      "TextFragmentAnchor.Unknown.Parameters",
      static_cast<int>(TextFragmentAnchorMetrics::TextFragmentAnchorParameters::
                           kExactTextWithSuffix),
      1);
  histogram_tester_.ExpectBucketCount(
      "TextFragmentAnchor.Unknown.Parameters",
      static_cast<int>(TextFragmentAnchorMetrics::TextFragmentAnchorParameters::
                           kExactTextWithContext),
      1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.TimeToScrollToTop", 0);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.ListItemMatch",
                                     4);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.ListItemMatch", 0, 4);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.TableCellMatch", 4);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.TableCellMatch", 0, 4);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.LinkOpenSource", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.LinkOpenSource", 0,
                                       1);
}

// Test that the correct metrics are collected for all possible combinations of
// context terms on a range text directive.
TEST_F(TextFragmentAnchorMetricsTest, TextRangeParameters) {
  SimRequest request(
      "https://example.com/"
      "test.html#:~:text=this,is&text=a-,test,page&text=with,some,-content&"
      "text=about-,nothing,at,-all",
      "text/html");
  LoadURL(
      "https://example.com/"
      "test.html#:~:text=this,is&text=a-,test,page&text=with,some,-content&"
      "text=about-,nothing,at,-all");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p>This is a test page</p>
    <p>With some content</p>
    <p>About nothing at all</p>
  )HTML");
  RunAsyncMatchingTasks();

  Compositor().BeginFrame();
  BeginEmptyFrame();

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.SelectorCount",
                                     1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.SelectorCount", 4, 1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.MatchRate", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.Unknown.MatchRate",
                                       100, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.AmbiguousMatch", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.AmbiguousMatch", 0, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.ScrollCancelled", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.ScrollCancelled", 0, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.DidScrollIntoView", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.DidScrollIntoView", 0, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.TimeToScrollIntoView", 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.DirectiveLength", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.DirectiveLength", 82, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.ExactTextLength", 0);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.RangeMatchLength", 4);
  // "This is"
  histogram_tester_.ExpectBucketCount(
      "TextFragmentAnchor.Unknown.RangeMatchLength", 7, 1);
  // "test page", "with some"
  histogram_tester_.ExpectBucketCount(
      "TextFragmentAnchor.Unknown.RangeMatchLength", 9, 2);
  // "nothing at"
  histogram_tester_.ExpectBucketCount(
      "TextFragmentAnchor.Unknown.RangeMatchLength", 10, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.StartTextLength", 4);
  // "this", "test", "with"
  histogram_tester_.ExpectBucketCount(
      "TextFragmentAnchor.Unknown.StartTextLength", 4, 3);
  // "nothing"
  histogram_tester_.ExpectBucketCount(
      "TextFragmentAnchor.Unknown.StartTextLength", 7, 1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.EndTextLength",
                                     4);
  // "is", "at"
  histogram_tester_.ExpectBucketCount(
      "TextFragmentAnchor.Unknown.EndTextLength", 2, 2);
  // "page", "some"
  histogram_tester_.ExpectBucketCount(
      "TextFragmentAnchor.Unknown.EndTextLength", 4, 2);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.Parameters",
                                     4);
  histogram_tester_.ExpectBucketCount(
      "TextFragmentAnchor.Unknown.Parameters",
      static_cast<int>(
          TextFragmentAnchorMetrics::TextFragmentAnchorParameters::kTextRange),
      1);
  histogram_tester_.ExpectBucketCount(
      "TextFragmentAnchor.Unknown.Parameters",
      static_cast<int>(TextFragmentAnchorMetrics::TextFragmentAnchorParameters::
                           kTextRangeWithPrefix),
      1);
  histogram_tester_.ExpectBucketCount(
      "TextFragmentAnchor.Unknown.Parameters",
      static_cast<int>(TextFragmentAnchorMetrics::TextFragmentAnchorParameters::
                           kTextRangeWithSuffix),
      1);
  histogram_tester_.ExpectBucketCount(
      "TextFragmentAnchor.Unknown.Parameters",
      static_cast<int>(TextFragmentAnchorMetrics::TextFragmentAnchorParameters::
                           kTextRangeWithContext),
      1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.TimeToScrollToTop", 0);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.ListItemMatch",
                                     0);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.TableCellMatch", 0);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.LinkOpenSource", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.LinkOpenSource", 0,
                                       1);
}

class TextFragmentAnchorScrollMetricsTest
    : public TextFragmentAnchorMetricsTest,
      public testing::WithParamInterface<mojom::blink::ScrollType> {
 protected:
  bool IsUserScrollType() {
    return GetParam() == mojom::blink::ScrollType::kCompositor ||
           GetParam() == mojom::blink::ScrollType::kUser;
  }
};

INSTANTIATE_TEST_SUITE_P(
    ScrollTypes,
    TextFragmentAnchorScrollMetricsTest,
    testing::Values(mojom::blink::ScrollType::kUser,
                    mojom::blink::ScrollType::kProgrammatic,
                    mojom::blink::ScrollType::kClamping,
                    mojom::blink::ScrollType::kCompositor,
                    mojom::blink::ScrollType::kAnchoring,
                    mojom::blink::ScrollType::kSequenced));

// Test that the ScrollCancelled metric gets reported when a user scroll cancels
// the scroll into view.
TEST_P(TextFragmentAnchorScrollMetricsTest, ScrollCancelled) {
  // This test isn't relevant with this flag enabled. When it's enabled,
  // there's no way to block rendering and the fragment is installed and
  // invoked as soon as parsing finishes which means the user cannot scroll
  // before this point.
  ScopedBlockHTMLParserOnStyleSheetsForTest block_parser(false);

  SimRequest request("https://example.com/test.html#:~:text=test", "text/html");
  SimSubresourceRequest css_request("https://example.com/test.css", "text/css");
  LoadURL("https://example.com/test.html#:~:text=test");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 1200px;
      }
      p {
        position: absolute;
        top: 1000px;
        visibility: hidden;
      }
    </style>
    <link rel=stylesheet href=test.css>
    <p>This is a test page</p>
  )HTML");

  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  mojom::blink::ScrollType scroll_type = GetParam();
  GetDocument().View()->LayoutViewport()->ScrollBy(ScrollOffset(0, 100),
                                                   scroll_type);

  // Set the target text to visible and change its position to cause a layout
  // and invoke the fragment anchor.
  css_request.Complete("p { visibility: visible; top: 1001px; }");
  RunAsyncMatchingTasks();

  // Render two frames to handle the async step added by the beforematch event.
  Compositor().BeginFrame();
  BeginEmptyFrame();

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.ScrollCancelled", 1);

  // A user scroll should have caused this to be canceled, other kinds of
  // scrolls should have no effect.
  if (IsUserScrollType()) {
    histogram_tester_.ExpectUniqueSample(
        "TextFragmentAnchor.Unknown.ScrollCancelled", 1, 1);

    histogram_tester_.ExpectTotalCount(
        "TextFragmentAnchor.Unknown.DidScrollIntoView", 0);
    histogram_tester_.ExpectTotalCount(
        "TextFragmentAnchor.Unknown.TimeToScrollIntoView", 0);
  } else {
    histogram_tester_.ExpectUniqueSample(
        "TextFragmentAnchor.Unknown.ScrollCancelled", 0, 1);
    histogram_tester_.ExpectTotalCount(
        "TextFragmentAnchor.Unknown.DidScrollIntoView", 1);
    histogram_tester_.ExpectTotalCount(
        "TextFragmentAnchor.Unknown.TimeToScrollIntoView", 1);
  }

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.SelectorCount",
                                     1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.SelectorCount", 1, 1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.MatchRate", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.Unknown.MatchRate",
                                       100, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.AmbiguousMatch", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.AmbiguousMatch", 0, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.DirectiveLength", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.DirectiveLength", 9, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.ExactTextLength", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.ExactTextLength", 4, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.RangeMatchLength", 0);
  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.StartTextLength", 0);
  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.EndTextLength",
                                     0);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.Parameters",
                                     1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.Parameters",
      static_cast<int>(
          TextFragmentAnchorMetrics::TextFragmentAnchorParameters::kExactText),
      1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.TimeToScrollToTop", 0);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.ListItemMatch",
                                     1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.ListItemMatch", 0, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.TableCellMatch", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.TableCellMatch", 0, 1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.LinkOpenSource", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.LinkOpenSource", 0,
                                       1);
}

// Test that the user scrolling back to the top of the page reports metrics
TEST_P(TextFragmentAnchorScrollMetricsTest, TimeToScrollToTop) {
  mojom::blink::ScrollType scroll_type = GetParam();

  // Set the page to be initially hidden to delay the text fragment so that we
  // can set the mock TickClock.
  WebView().SetVisibilityState(mojom::blink::PageVisibilityState::kHidden,
                               /*initial_state=*/true);

  SimRequest request("https://example.com/test.html#:~:text=test%20page",
                     "text/html");
  LoadURL("https://example.com/test.html#:~:text=test%20page");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 2200px;
      }
      p {
        position: absolute;
        top: 1000px;
      }
    </style>
    <p>This is a test page</p>
  )HTML");
  RunAsyncMatchingTasks();

  BeginEmptyFrame();
  BeginEmptyFrame();

  // Set the test TickClock and then render the page visible to activate the
  // text fragment.
  base::SimpleTestTickClock tick_clock;
  tick_clock.SetNowTicks(base::TimeTicks::Now());
  static_cast<TextFragmentAnchor*>(GetDocument().View()->GetFragmentAnchor())
      ->SetTickClockForTesting(&tick_clock);
  WebView().SetVisibilityState(mojom::blink::PageVisibilityState::kVisible,
                               /*initial_state=*/false);
  BeginEmptyFrame();
  BeginEmptyFrame();

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.TimeToScrollToTop", 0);

  const int64_t time_to_scroll_to_top = 500;
  tick_clock.Advance(base::TimeDelta::FromMilliseconds(time_to_scroll_to_top));

  ASSERT_GT(GetDocument().View()->LayoutViewport()->GetScrollOffset().Height(),
            100);

  // Ensure scrolling but not to the top isn't counted.
  {
    GetDocument().View()->LayoutViewport()->ScrollBy(ScrollOffset(0, -20),
                                                     scroll_type);
    histogram_tester_.ExpectTotalCount(
        "TextFragmentAnchor.Unknown.TimeToScrollToTop", 0);
  }

  // Scroll to top and ensure the metric is recorded, but only for user type
  // scrolls.
  {
    GetDocument().View()->LayoutViewport()->SetScrollOffset(ScrollOffset(),
                                                            scroll_type);

    if (IsUserScrollType()) {
      histogram_tester_.ExpectTotalCount(
          "TextFragmentAnchor.Unknown.TimeToScrollToTop", 1);
      histogram_tester_.ExpectUniqueSample(
          "TextFragmentAnchor.Unknown.TimeToScrollToTop", time_to_scroll_to_top,
          1);
    } else {
      histogram_tester_.ExpectTotalCount(
          "TextFragmentAnchor.Unknown.TimeToScrollToTop", 0);
    }
  }

  // Scroll down and then back up to the top again to ensure the metric is
  // recorded only once.
  {
    GetDocument().View()->LayoutViewport()->SetScrollOffset(
        ScrollOffset(0, 100), scroll_type);
    GetDocument().View()->LayoutViewport()->SetScrollOffset(ScrollOffset(),
                                                            scroll_type);

    if (IsUserScrollType()) {
      histogram_tester_.ExpectTotalCount(
          "TextFragmentAnchor.Unknown.TimeToScrollToTop", 1);
    } else {
      histogram_tester_.ExpectTotalCount(
          "TextFragmentAnchor.Unknown.TimeToScrollToTop", 0);
    }
  }
}

// Test that the TapToDismiss feature gets use counted when the user taps to
// dismiss the text highlight
TEST_F(TextFragmentAnchorMetricsTest, TapToDismiss) {
  SimRequest request("https://example.com/test.html#:~:text=test%20page",
                     "text/html");
  LoadURL("https://example.com/test.html#:~:text=test%20page");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 2200px;
      }
      p {
        position: absolute;
        top: 1000px;
      }
    </style>
    <p>This is a test page</p>
  )HTML");
  RunAsyncMatchingTasks();

  // Render two frames to handle the async step added by the beforematch event.
  Compositor().BeginFrame();
  BeginEmptyFrame();

  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kTextFragmentAnchor));
  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kTextFragmentAnchorMatchFound));
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kTextFragmentAnchorTapToDismiss));

  SimulateClick(100, 100);

  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kTextFragmentAnchorTapToDismiss));
}

// Test counting cases where the fragment directive fails to parse.
TEST_F(TextFragmentAnchorMetricsTest, InvalidFragmentDirective) {
  const int kUncounted = 0;
  const int kCounted = 1;

  Vector<std::pair<String, int>> test_cases = {
      {"", kUncounted},
      {"#element", kUncounted},
      {"#doesntExist", kUncounted},
      {"#:~:element", kCounted},
      {"#element:~:", kCounted},
      {"#foo:~:bar", kCounted},
      {"#:~:utext=foo", kCounted},
      {"#:~:text=foo", kUncounted},
      {"#:~:text=foo&invalid", kUncounted},
      {"#foo:~:text=foo", kUncounted}};

  for (auto test_case : test_cases) {
    String url = "https://example.com/test.html" + test_case.first;
    SimRequest request(url, "text/html");
    LoadURL(url);
    request.Complete(R"HTML(
      <!DOCTYPE html>
      <p id="element">This is a test page</p>
    )HTML");
    // Render two frames to handle the async step added by the beforematch
    // event.
    Compositor().BeginFrame();
    BeginEmptyFrame();

    RunAsyncMatchingTasks();

    bool is_use_counted =
        GetDocument().IsUseCounted(WebFeature::kInvalidFragmentDirective);
    if (test_case.second == kCounted) {
      EXPECT_TRUE(is_use_counted)
          << "Expected invalid directive in case: " << test_case.first;
    } else {
      EXPECT_FALSE(is_use_counted)
          << "Expected valid directive in case: " << test_case.first;
    }
  }
}

// Test recording of the ListItemMatch metric
TEST_F(TextFragmentAnchorMetricsTest, ListItemMatch) {
  SimRequest request("https://example.com/test.html#:~:text=list", "text/html");
  LoadURL("https://example.com/test.html#:~:text=list");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <ul>
      <li>Some test content</li>
      <li>Within a list item</li>
    </ul>
  )HTML");
  RunAsyncMatchingTasks();

  BeginEmptyFrame();
  BeginEmptyFrame();

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.ListItemMatch",
                                     1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.ListItemMatch", 1, 1);
}

// Test recording of the TableCellMatch metric
TEST_F(TextFragmentAnchorMetricsTest, TableCellMatch) {
  SimRequest request("https://example.com/test.html#:~:text=table",
                     "text/html");
  LoadURL("https://example.com/test.html#:~:text=table");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <table>
      <tr>
        <td>Some test content</td>
        <td>Within a table cell</td>
      </tr>
    </table>
  )HTML");
  RunAsyncMatchingTasks();

  BeginEmptyFrame();
  BeginEmptyFrame();

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.TableCellMatch", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.TableCellMatch", 1, 1);
}

// Test recording of ListItemMatch for a match nested in a list item
TEST_F(TextFragmentAnchorMetricsTest, NestedListItemMatch) {
  SimRequest request("https://example.com/test.html#:~:text=list", "text/html");
  LoadURL("https://example.com/test.html#:~:text=list");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <ol>
      <li>Some test content</li>
      <li>Within a <span>list</span> item</li>
    </ol>
  )HTML");
  RunAsyncMatchingTasks();

  BeginEmptyFrame();
  BeginEmptyFrame();

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.ListItemMatch",
                                     1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.ListItemMatch", 1, 1);
}

// Test recording of TableCellMatch for a match nested in a table cell
TEST_F(TextFragmentAnchorMetricsTest, NestedTableCellMatch) {
  SimRequest request("https://example.com/test.html#:~:text=table",
                     "text/html");
  LoadURL("https://example.com/test.html#:~:text=table");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <table>
      <tr>
        <td>Some test content</td>
        <td>Within a <span>table</span> cell</td>
      </tr>
    </table>
  )HTML");
  RunAsyncMatchingTasks();

  BeginEmptyFrame();
  BeginEmptyFrame();

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.TableCellMatch", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.TableCellMatch", 1, 1);
}

class TextFragmentRelatedMetricTest : public TextFragmentAnchorMetricsTest,
                                      public testing::WithParamInterface<bool> {
 public:
  TextFragmentRelatedMetricTest() : text_fragment_anchors_state_(GetParam()) {}

 private:
  ScopedTextFragmentIdentifiersForTest text_fragment_anchors_state_;
};

// These tests will run with and without the TextFragmentIdentifiers feature
// enabled to ensure we collect metrics correctly under both situations.
INSTANTIATE_TEST_SUITE_P(All,
                         TextFragmentRelatedMetricTest,
                         testing::Values(false, true));

// Test that we correctly track failed vs. successful element-id lookups. We
// only count these in cases where we don't have a text directive, when the REF
// is enabled.
TEST_P(TextFragmentRelatedMetricTest, ElementIdSuccessFailureCounts) {
  const int kUncounted = 0;
  const int kFound = 1;
  const int kNotFound = 2;

  // When the TextFragmentAnchors feature is on, we should avoid counting the
  // result of the element-id fragment if a text directive is successfully
  // parsed. If the feature is off we treat the text directive as an element-id
  // and should count the result.
  const int kUncountedOrFound = GetParam() ? kUncounted : kFound;

  // Note: We'll strip the fragment directive (i.e. anything after :~:) leaving
  // just the element anchor. The fragment directive stripping behavior is now
  // shipped unflagged so it should always be performed.

  Vector<std::pair<String, int>> test_cases = {
      {"", kUncounted},
      {"#element", kFound},
      {"#doesntExist", kNotFound},
      // `:~:foo` will be stripped so #element will be found and #doesntexist
      // ##element will be not found.
      {"#element:~:foo", kFound},
      {"#doesntexist:~:foo", kNotFound},
      {"##element", kNotFound},
      // If the feature  is on, `:~:text=` will parse so we shouldn't count.
      // Otherwise, it'll just be stripped so #element will be found.
      {"#element:~:text=doesntexist", kUncountedOrFound},
      {"#element:~:text=page", kUncountedOrFound},
      // If the feature is on, `:~:text` is parsed so we don't count. If it's
      // off the entire fragment is a directive that's stripped so no search is
      // performed either.
      {"#:~:text=doesntexist", kUncounted},
      {"#:~:text=page", kUncounted},
      {"#:~:text=name", kUncounted},
      // If the feature is enabled, `:~:text` parses and we don't count the
      // element-id. If the feature is off, we still strip the :~: directive
      // and the remaining fragment does match an element id.
      {"#element:~:text=name", kUncountedOrFound}};

  const int kNotFoundSample = 0;
  const int kFoundSample = 1;
  const std::string histogram = "TextFragmentAnchor.ElementIdFragmentFound";

  // Add counts to each histogram so that calls to GetBucketCount won't fail
  // due to not finding the histogram.
  UMA_HISTOGRAM_BOOLEAN(histogram, true);
  UMA_HISTOGRAM_BOOLEAN(histogram, false);
  int expected_found_count = 1;
  int expected_not_found_count = 1;

  for (auto test_case : test_cases) {
    String url = "https://example.com/test.html" + test_case.first;
    SimRequest request(url, "text/html");
    LoadURL(url);
    request.Complete(R"HTML(
      <!DOCTYPE html>
      <p id="element">This is a test page</p>
      <p id=":~:text=name">This is a test page</p>
      <p id="element:~:text=name">This is a test page</p>
    )HTML");
    // Render two frames to handle the async step added by the beforematch
    // event.
    Compositor().BeginFrame();
    BeginEmptyFrame();

    RunAsyncMatchingTasks();

    auto not_found_count =
        histogram_tester_.GetBucketCount(histogram, kNotFoundSample);
    auto found_count =
        histogram_tester_.GetBucketCount(histogram, kFoundSample);
    int result = test_case.second;
    if (result == kFound) {
      ++expected_found_count;
      ASSERT_EQ(expected_found_count, found_count)
          << "ElementId should have been |Found| but did not UseCount on case: "
          << test_case.first;
      ASSERT_EQ(expected_not_found_count, not_found_count)
          << "ElementId should have been |Found| but reported |NotFound| on "
             "case: "
          << test_case.first;
    } else if (result == kNotFound) {
      ++expected_not_found_count;
      ASSERT_EQ(expected_not_found_count, not_found_count)
          << "ElementId should have been |NotFound| but did not UseCount on "
             "case: "
          << test_case.first;
      ASSERT_EQ(expected_found_count, found_count)
          << "ElementId should have been |NotFound| but reported |Found| on "
             "case: "
          << test_case.first;
    } else {
      DCHECK_EQ(result, kUncounted);
      ASSERT_EQ(expected_found_count, found_count)
          << "Case should not have been counted but reported |Found| on case: "
          << test_case.first;
      ASSERT_EQ(expected_not_found_count, not_found_count)
          << "Case should not have been counted but reported |NotFound| on "
             "case: "
          << test_case.first;
    }
  }
}

// Test counting occurrences of ~&~ in the URL fragment. Used for potentially
// using ~&~ as a delimiter. Can be removed once the feature ships.
TEST_P(TextFragmentRelatedMetricTest, TildeAmpersandTildeUseCounter) {
  const int kUncounted = 0;
  const int kCounted = 1;

  Vector<std::pair<String, int>> test_cases = {{"", kUncounted},
                                               {"#element", kUncounted},
                                               {"#doesntExist", kUncounted},
                                               {"#~&~element", kCounted},
                                               {"#element~&~", kCounted},
                                               {"#foo~&~bar", kCounted},
                                               {"#foo~&~text=foo", kCounted}};

  for (auto test_case : test_cases) {
    String url = "https://example.com/test.html" + test_case.first;
    SimRequest request(url, "text/html");
    LoadURL(url);
    request.Complete(R"HTML(
      <!DOCTYPE html>
      <p id="element">This is a test page</p>
    )HTML");
    // Render two frames to handle the async step added by the beforematch
    // event.
    Compositor().BeginFrame();
    BeginEmptyFrame();

    RunAsyncMatchingTasks();

    bool is_use_counted =
        GetDocument().IsUseCounted(WebFeature::kFragmentHasTildeAmpersandTilde);
    if (test_case.second == kCounted) {
      EXPECT_TRUE(is_use_counted)
          << "Expected to count ~&~ but didn't in case: " << test_case.first;
    } else {
      EXPECT_FALSE(is_use_counted)
          << "Expected not to count ~&~ but did in case: " << test_case.first;
    }
  }
}

// Test counting occurrences of ~@~ in the URL fragment. Used for potentially
// using ~@~ as a delimiter. Can be removed once the feature ships.
TEST_P(TextFragmentRelatedMetricTest, TildeAtTildeUseCounter) {
  const int kUncounted = 0;
  const int kCounted = 1;

  Vector<std::pair<String, int>> test_cases = {{"", kUncounted},
                                               {"#element", kUncounted},
                                               {"#doesntExist", kUncounted},
                                               {"#~@~element", kCounted},
                                               {"#element~@~", kCounted},
                                               {"#foo~@~bar", kCounted},
                                               {"#foo~@~text=foo", kCounted}};

  for (auto test_case : test_cases) {
    String url = "https://example.com/test.html" + test_case.first;
    SimRequest request(url, "text/html");
    LoadURL(url);
    request.Complete(R"HTML(
      <!DOCTYPE html>
      <p id="element">This is a test page</p>
    )HTML");
    // Render two frames to handle the async step added by the beforematch
    // event.
    Compositor().BeginFrame();
    BeginEmptyFrame();

    RunAsyncMatchingTasks();

    bool is_use_counted =
        GetDocument().IsUseCounted(WebFeature::kFragmentHasTildeAtTilde);
    if (test_case.second == kCounted) {
      EXPECT_TRUE(is_use_counted)
          << "Expected to count ~@~ but didn't in case: " << test_case.first;
    } else {
      EXPECT_FALSE(is_use_counted)
          << "Expected not to count ~@~ but did in case: " << test_case.first;
    }
  }
}

// Test counting occurrences of &delimiter? in the URL fragment. Used for
// potentially using &delimiter? as a delimiter. Can be removed once the
// feature ships.
TEST_P(TextFragmentRelatedMetricTest, AmpersandDelimiterQuestionUseCounter) {
  const int kUncounted = 0;
  const int kCounted = 1;

  Vector<std::pair<String, int>> test_cases = {
      {"", kUncounted},
      {"#element", kUncounted},
      {"#doesntExist", kUncounted},
      {"#&delimiter?element", kCounted},
      {"#element&delimiter?", kCounted},
      {"#foo&delimiter?bar", kCounted},
      {"#foo&delimiter?text=foo", kCounted}};

  for (auto test_case : test_cases) {
    String url = "https://example.com/test.html" + test_case.first;
    SimRequest request(url, "text/html");
    LoadURL(url);
    request.Complete(R"HTML(
      <!DOCTYPE html>
      <p id="element">This is a test page</p>
    )HTML");
    // Render two frames to handle the async step added by the beforematch
    // event.
    Compositor().BeginFrame();
    BeginEmptyFrame();

    RunAsyncMatchingTasks();

    bool is_use_counted = GetDocument().IsUseCounted(
        WebFeature::kFragmentHasAmpersandDelimiterQuestion);
    if (test_case.second == kCounted) {
      EXPECT_TRUE(is_use_counted)
          << "Expected to count &delimiter? but didn't in case: "
          << test_case.first;
    } else {
      EXPECT_FALSE(is_use_counted)
          << "Expected not to count &delimiter? but did in case: "
          << test_case.first;
    }
  }
}

// Test counting occurrences of non-directive :~: in the URL fragment. Used to
// ensure :~: is web-compatible; can be removed once the feature ships.
TEST_P(TextFragmentRelatedMetricTest, NewDelimiterUseCounter) {
  const int kUncounted = 0;
  const int kCounted = 1;

  Vector<std::pair<String, int>> test_cases = {{"", kUncounted},
                                               {"#element", kUncounted},
                                               {"#doesntExist", kUncounted},
                                               {"#:~:element", kCounted},
                                               {"#element:~:", kCounted},
                                               {"#foo:~:bar", kCounted},
                                               {"#:~:utext=foo", kCounted},
                                               {"#:~:text=foo", kUncounted},
                                               {"#foo:~:text=foo", kUncounted}};

  for (auto test_case : test_cases) {
    String url = "https://example.com/test.html" + test_case.first;
    SimRequest request(url, "text/html");
    LoadURL(url);
    request.Complete(R"HTML(
      <!DOCTYPE html>
      <p id="element">This is a test page</p>
    )HTML");
    // Render two frames to handle the async step added by the beforematch
    // event.
    Compositor().BeginFrame();
    BeginEmptyFrame();

    RunAsyncMatchingTasks();

    bool is_use_counted =
        GetDocument().IsUseCounted(WebFeature::kFragmentHasColonTildeColon);
    if (test_case.second == kCounted) {
      EXPECT_TRUE(is_use_counted)
          << "Expected to count :~: but didn't in case: " << test_case.first;
    } else {
      EXPECT_FALSE(is_use_counted)
          << "Expected not to count :~: but did in case: " << test_case.first;
    }
  }
}

// Test use counting the document.fragmentDirective API
TEST_P(TextFragmentRelatedMetricTest, TextFragmentAPIUseCounter) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <script>
      var textFragmentsSupported = typeof(document.fragmentDirective) == "object";
    </script>
    <p>This is a test page</p>
  )HTML");
  Compositor().BeginFrame();
  RunAsyncMatchingTasks();

  bool text_fragments_enabled = GetParam();

  EXPECT_EQ(text_fragments_enabled,
            GetDocument().IsUseCounted(
                WebFeature::kV8Document_FragmentDirective_AttributeGetter));
}

// Test that simply activating a text fragment does not use count the API
TEST_P(TextFragmentRelatedMetricTest, TextFragmentActivationDoesNotCountAPI) {
  SimRequest request("https://example.com/test.html#:~:text=test", "text/html");
  LoadURL("https://example.com/test.html#:~:text=test");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p>This is a test page</p>
  )HTML");
  Compositor().BeginFrame();
  RunAsyncMatchingTasks();

  bool text_fragments_enabled = GetParam();
  EXPECT_EQ(text_fragments_enabled,
            GetDocument().IsUseCounted(WebFeature::kTextFragmentAnchor));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kV8Document_FragmentDirective_AttributeGetter));
}

// Test recording of the SpansMultipleBlocks metric. Records true because the
// range crosses an intervening block element.
TEST_F(TextFragmentAnchorMetricsTest, SpansMultipleBlocksInterveningBlock) {
  SimRequest request("https://example.com/test.html#:~:text=start,end",
                     "text/html");
  LoadURL("https://example.com/test.html#:~:text=start,end");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>
      start of text
      <div>block</div>
      text end
    </div>
  )HTML");
  RunAsyncMatchingTasks();

  BeginEmptyFrame();
  BeginEmptyFrame();

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.SpansMultipleBlocks", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.SpansMultipleBlocks", 1, 1);
}

// Test recording of the SpansMultipleBlocks metric. Records true because the
// range start and end are in different block elements.
TEST_F(TextFragmentAnchorMetricsTest, SpansMultipleBlocks) {
  SimRequest request("https://example.com/test.html#:~:text=start,end",
                     "text/html");
  LoadURL("https://example.com/test.html#:~:text=start,end");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>
      <div>start of text</div>
      text end
    </div>
  )HTML");
  RunAsyncMatchingTasks();

  BeginEmptyFrame();
  BeginEmptyFrame();

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.SpansMultipleBlocks", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.SpansMultipleBlocks", 1, 1);
}

// Test recording of the SpansMultipleBlocks metric. Records false because the
// range start and end are in the same block element with no intervening block.
TEST_F(TextFragmentAnchorMetricsTest, SpansMultipleBlocksSingleBlock) {
  SimRequest request("https://example.com/test.html#:~:text=start,end",
                     "text/html");
  LoadURL("https://example.com/test.html#:~:text=start,end");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>
      start of <i>text</i>
      text end
    </div>
  )HTML");
  RunAsyncMatchingTasks();

  BeginEmptyFrame();
  BeginEmptyFrame();

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.SpansMultipleBlocks", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.SpansMultipleBlocks", 0, 1);
}

}  // namespace

}  // namespace blink
