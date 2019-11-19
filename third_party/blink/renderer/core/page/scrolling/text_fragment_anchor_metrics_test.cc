// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
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
    WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  }

  void RunAsyncMatchingTasks() {
    auto* scheduler =
        ThreadScheduler::Current()->GetWebMainThreadSchedulerForTest();
    blink::scheduler::RunIdleTasksForTesting(scheduler,
                                             base::BindOnce([]() {}));
    RunPendingTasks();
  }

  void SimulateClick(int x, int y) {
    WebMouseEvent event(
        WebInputEvent::kMouseDown, WebFloatPoint(x, y), WebFloatPoint(x, y),
        WebPointerProperties::Button::kLeft, 0,
        WebInputEvent::Modifiers::kLeftButtonDown, base::TimeTicks::Now());
    event.SetFrameScale(1);
    GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(event);
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
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.SelectorCount", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.SelectorCount", 2,
                                       1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.MatchRate", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.MatchRate", 50, 1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.AmbiguousMatch", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.AmbiguousMatch", 1,
                                       1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.ScrollCancelled", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.ScrollCancelled", 0,
                                       1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.DidScrollIntoView", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.DidScrollIntoView",
                                       1, 1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.TimeToScrollIntoView",
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
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.SelectorCount", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.SelectorCount", 1,
                                       1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.MatchRate", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.MatchRate", 0, 1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.AmbiguousMatch", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.AmbiguousMatch", 0,
                                       1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.ScrollCancelled", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.ScrollCancelled", 0,
                                       1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.DidScrollIntoView", 0);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.TimeToScrollIntoView",
                                     0);
}

// Test that we don't collect any metrics when there is no text directive
TEST_F(TextFragmentAnchorMetricsTest, NoTextFragmentAnchor) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p>This is a test page</p>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.SelectorCount", 0);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.MatchRate", 0);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.AmbiguousMatch", 0);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.ScrollCancelled", 0);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.DidScrollIntoView", 0);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.TimeToScrollIntoView",
                                     0);
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
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.SelectorCount", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.SelectorCount", 1,
                                       1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.MatchRate", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.MatchRate", 100, 1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.AmbiguousMatch", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.AmbiguousMatch", 0,
                                       1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.ScrollCancelled", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.ScrollCancelled", 0,
                                       1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.DidScrollIntoView", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.DidScrollIntoView",
                                       0, 1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.TimeToScrollIntoView",
                                     1);
}

// Test that the ScrollCancelled metric gets reported when a user scroll cancels
// the scroll into view.
TEST_F(TextFragmentAnchorMetricsTest, ScrollCancelled) {
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

  Compositor().PaintFrame();
  GetDocument().View()->LayoutViewport()->ScrollBy(ScrollOffset(0, 100),
                                                   kUserScroll);

  // Set the target text to visible and change its position to cause a layout
  // and invoke the fragment anchor.
  css_request.Complete("p { visibility: visible; top: 1001px; }");

  Compositor().BeginFrame();
  RunAsyncMatchingTasks();

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.SelectorCount", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.SelectorCount", 1,
                                       1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.MatchRate", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.MatchRate", 100, 1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.AmbiguousMatch", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.AmbiguousMatch", 0,
                                       1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.ScrollCancelled", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.ScrollCancelled", 1,
                                       1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.DidScrollIntoView", 0);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.TimeToScrollIntoView",
                                     0);
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
  Compositor().BeginFrame();
  RunAsyncMatchingTasks();

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
      {"#:~:text=foo&invalid", kCounted},
      {"#foo:~:text=foo", kUncounted}};

  for (auto test_case : test_cases) {
    String url = "https://example.com/test.html" + test_case.first;
    SimRequest request(url, "text/html");
    LoadURL(url);
    request.Complete(R"HTML(
      <!DOCTYPE html>
      <p id="element">This is a test page</p>
    )HTML");
    Compositor().BeginFrame();

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

class TextFragmentRelatedMetricTest : public TextFragmentAnchorMetricsTest,
                                      public testing::WithParamInterface<bool> {
 public:
  TextFragmentRelatedMetricTest() : text_fragment_anchors_state_(GetParam()) {}

 private:
  ScopedTextFragmentIdentifiersForTest text_fragment_anchors_state_;
};

// These tests will run with and without the TextFragmentIdentifiers feature
// enabled to ensure we collect metrics correctly under both situations.
INSTANTIATE_TEST_SUITE_P(,
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
  const int kUncountedOrNotFound = GetParam() ? kUncounted : kNotFound;
  const int kUncountedOrFound = GetParam() ? kUncounted : kFound;

  // When the TextFragmentAnchors feature is on, we'll strip the fragment
  // directive (i.e. anything after :~:) leaving just the element anchor.
  const int kFoundIfDirectiveStripped = GetParam() ? kFound : kNotFound;

  Vector<std::pair<String, int>> test_cases = {
      {"", kUncounted},
      {"#element", kFound},
      {"#doesntExist", kNotFound},
      {"#element:~:foo", kFoundIfDirectiveStripped},
      {"#doesntexist:~:foo", kNotFound},
      {"##element", kNotFound},
      {"#element:~:text=doesntexist", kUncountedOrNotFound},
      {"#element:~:text=page", kUncountedOrNotFound},
      {"#:~:text=doesntexist", kUncountedOrNotFound},
      {"#:~:text=page", kUncountedOrNotFound},
      {"#:~:text=name", kUncountedOrFound},
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
    Compositor().BeginFrame();

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
    Compositor().BeginFrame();

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
    Compositor().BeginFrame();

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
    Compositor().BeginFrame();

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
    Compositor().BeginFrame();

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

}  // namespace

}  // namespace blink
