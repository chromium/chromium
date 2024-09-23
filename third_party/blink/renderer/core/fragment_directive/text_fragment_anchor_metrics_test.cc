// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fragment_directive/text_fragment_anchor_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/scroll/scroll_enums.mojom-blink.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/core/fragment_directive/text_fragment_anchor.h"
#include "third_party/blink/renderer/core/fragment_directive/text_fragment_test_util.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/scoped_fake_ukm_recorder.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

using test::RunPendingTasks;

const char kSuccessUkmMetric[] = "Success";
const char kSourceUkmMetric[] = "Source";

class TextFragmentAnchorMetricsTest : public TextFragmentAnchorTestBase {
 public:
  TextFragmentAnchorMetricsTest()
      : TextFragmentAnchorTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  void SimulateClick(int x, int y) {
    WebMouseEvent event(WebInputEvent::Type::kMouseDown, gfx::PointF(x, y),
                        gfx::PointF(x, y), WebPointerProperties::Button::kLeft,
                        0, WebInputEvent::Modifiers::kLeftButtonDown,
                        base::TimeTicks::Now());
    event.SetFrameScale(1);
    GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(event);
  }

 protected:
  ukm::TestUkmRecorder* ukm_recorder() {
    return scoped_fake_ukm_recorder_.recorder();
  }

  base::HistogramTester histogram_tester_;
  ScopedFakeUkmRecorder scoped_fake_ukm_recorder_;
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
  RunUntilTextFragmentFinalization();

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.MatchRate", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.Unknown.MatchRate",
                                       50, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.AmbiguousMatch", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.AmbiguousMatch", 1, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.TimeToScrollIntoView", 1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.LinkOpenSource", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.LinkOpenSource", 0,
                                       1);
}

// Test UMA metrics collection with search engine referrer.
TEST_F(TextFragmentAnchorMetricsTest, UMAMetricsCollectedSearchEngineReferrer) {
  // Set the referrer to a known search engine URL. This should cause metrics
  // to be reported for the SearchEngine variant of histograms.
  SimRequest::Params params;
  params.requestor_origin = WebSecurityOrigin::CreateFromString(
      WebString::FromUTF8("https://www.bing.com"));
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
  RunUntilTextFragmentFinalization();

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.SearchEngine.MatchRate", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.SearchEngine.MatchRate", 50, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.SearchEngine.AmbiguousMatch", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.SearchEngine.AmbiguousMatch", 1, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.SearchEngine.TimeToScrollIntoView", 1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.LinkOpenSource", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.LinkOpenSource", 1,
                                       1);
}

// Test UMA metrics collection when there is no match found with an unknown
// referrer.
TEST_F(TextFragmentAnchorMetricsTest, NoMatchFoundWithUnknownSource) {
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
  RunUntilTextFragmentFinalization();

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.MatchRate", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.Unknown.MatchRate",
                                       0, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.AmbiguousMatch", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.AmbiguousMatch", 0, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.TimeToScrollIntoView", 0);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.LinkOpenSource", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.LinkOpenSource", 0,
                                       1);
}

// Test UMA metrics collection when there is no match found with a Search Engine
// referrer.
TEST_F(TextFragmentAnchorMetricsTest, NoMatchFoundWithSearchEngineSource) {
  // Set the referrer to a known search engine URL. This should cause metrics
  // to be reported for the SearchEngine variant of histograms.
  SimRequest::Params params;
  params.requestor_origin = WebSecurityOrigin::CreateFromString(
      WebString::FromUTF8("https://www.bing.com"));
  SimRequest request("https://example.com/test.html#:~:text=cat", "text/html",
                     params);
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
  RunUntilTextFragmentFinalization();

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.SearchEngine.MatchRate", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.SearchEngine.MatchRate", 0, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.SearchEngine.AmbiguousMatch", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.SearchEngine.AmbiguousMatch", 0, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.SearchEngine.TimeToScrollIntoView", 0);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.LinkOpenSource", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.LinkOpenSource", 1,
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
  Compositor().BeginFrame();

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.MatchRate", 0);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.AmbiguousMatch", 0);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.TimeToScrollIntoView", 0);

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
  Compositor().BeginFrame();

  // The anchor should have been found and finalized.
  EXPECT_FALSE(GetDocument().GetFrame()->View()->GetFragmentAnchor());

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.MatchRate", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.Unknown.MatchRate",
                                       100, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.AmbiguousMatch", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.AmbiguousMatch", 0, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.TimeToScrollIntoView", 1);

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
  RunUntilTextFragmentFinalization();

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.MatchRate", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.Unknown.MatchRate",
                                       100, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.AmbiguousMatch", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.AmbiguousMatch", 0, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.TimeToScrollIntoView", 1);

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
  RunUntilTextFragmentFinalization();

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.Unknown.MatchRate", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.Unknown.MatchRate",
                                       100, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.AmbiguousMatch", 1);
  histogram_tester_.ExpectUniqueSample(
      "TextFragmentAnchor.Unknown.AmbiguousMatch", 0, 1);

  histogram_tester_.ExpectTotalCount(
      "TextFragmentAnchor.Unknown.TimeToScrollIntoView", 1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.LinkOpenSource", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.LinkOpenSource", 0,
                                       1);
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
    if (GetDocument().GetFrame()->View()->GetFragmentAnchor()) {
      RunUntilTextFragmentFinalization();
    }

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
INSTANTIATE_TEST_SUITE_P(All,
                         TextFragmentRelatedMetricTest,
                         testing::Values(false, true));

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
  RunPendingTasks();
  Compositor().BeginFrame();

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
  bool text_fragments_enabled = GetParam();
  if (text_fragments_enabled) {
    RunUntilTextFragmentFinalization();
  }

  EXPECT_EQ(text_fragments_enabled,
            GetDocument().IsUseCounted(WebFeature::kTextFragmentAnchor));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kV8Document_FragmentDirective_AttributeGetter));
}

// Tests that a LinkOpened UKM Event is recorded upon a successful fragment
// highlight.
TEST_F(TextFragmentAnchorMetricsTest, LinkOpenedSuccessUKM) {
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
  RunUntilTextFragmentFinalization();

  // Flush UKM logging mojo request.
  RunPendingTasks();

  auto entries = ukm_recorder()->GetEntriesByName(
      ukm::builders::SharedHighlights_LinkOpened::kEntryName);
  ASSERT_EQ(1u, entries.size());
  const ukm::mojom::UkmEntry* entry = entries[0];
  EXPECT_EQ(GetDocument().UkmSourceID(), entry->source_id);
  ukm_recorder()->ExpectEntryMetric(entry, kSuccessUkmMetric,
                                    /*expected_value=*/true);
  EXPECT_TRUE(ukm_recorder()->GetEntryMetric(entry, kSourceUkmMetric));
}

// Tests that a LinkOpened UKM Event is recorded upon a failed fragment
// highlight.
TEST_F(TextFragmentAnchorMetricsTest, LinkOpenedFailedUKM) {
  SimRequest request(
      "https://example.com/test.html#:~:text=not%20on%20the%20page",
      "text/html");
  LoadURL("https://example.com/test.html#:~:text=not%20on%20the%20page");
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
  RunUntilTextFragmentFinalization();

  // Flush UKM logging mojo request.
  RunPendingTasks();

  auto entries = ukm_recorder()->GetEntriesByName(
      ukm::builders::SharedHighlights_LinkOpened::kEntryName);
  ASSERT_EQ(1u, entries.size());
  const ukm::mojom::UkmEntry* entry = entries[0];
  EXPECT_EQ(GetDocument().UkmSourceID(), entry->source_id);
  ukm_recorder()->ExpectEntryMetric(entry, kSuccessUkmMetric,
                                    /*expected_value=*/false);
  EXPECT_TRUE(ukm_recorder()->GetEntryMetric(entry, kSourceUkmMetric));
}

// Tests that loading a page that has a ForceLoadAtTop DocumentPolicy invokes
// the UseCounter.
TEST_F(TextFragmentAnchorMetricsTest, ForceLoadAtTopUseCounter) {
  SimRequest::Params params;
  params.response_http_headers.insert("Document-Policy", "force-load-at-top");
  SimRequest request("https://example.com/test.html", "text/html", params);
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p>This is a test page</p>
  )HTML");
  RunPendingTasks();
  Compositor().BeginFrame();

  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kForceLoadAtTop));
}

// Tests that loading a page that explicitly disables ForceLoadAtTop
// DocumentPolicy or has no DocumentPolicy doesn't invoke the UseCounter for
// ForceLoadAtTop.
TEST_F(TextFragmentAnchorMetricsTest, NoForceLoadAtTopUseCounter) {
  SimRequest::Params params;
  params.response_http_headers.insert("Document-Policy",
                                      "no-force-load-at-top");
  SimRequest request("https://example.com/test.html", "text/html", params);
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p>This is a test page</p>
  )HTML");
  RunPendingTasks();
  Compositor().BeginFrame();

  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kForceLoadAtTop));

  // Try without any DocumentPolicy headers.
  SimRequest request2("https://example.com/test2.html", "text/html");
  LoadURL("https://example.com/test2.html");
  request2.Complete(R"HTML(
    <!DOCTYPE html>
    <p>This is a different test page</p>
  )HTML");
  RunPendingTasks();
  Compositor().BeginFrame();

  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kForceLoadAtTop));
}

// Tests that we correctly record the "TextFragmentBlockedByForceLoadAtTop" use
// counter, that is, only when a text fragment appears and would otherwise have
// been invoked but was blocked by DocumentPolicy.
TEST_F(TextFragmentAnchorMetricsTest,
       TextFragmentBlockedByForceLoadAtTopUseCounter) {
  // ForceLoadAtTop is effective but TextFragmentBlocked isn't recorded because
  // there is no text fragment.
  {
    SimRequest::Params params;
    params.response_http_headers.insert("Document-Policy", "force-load-at-top");
    SimRequest request("https://example.com/test.html", "text/html", params);
    LoadURL("https://example.com/test.html");
    request.Complete(R"HTML(
      <!DOCTYPE html>
      <p>This is a test page</p>
    )HTML");
    RunPendingTasks();
    Compositor().BeginFrame();

    ASSERT_TRUE(GetDocument().IsUseCounted(WebFeature::kForceLoadAtTop));
    EXPECT_FALSE(GetDocument().IsUseCounted(
        WebFeature::kTextFragmentBlockedByForceLoadAtTop));
  }

  // This time there was a text fragment along with the DocumentPolicy so we
  // record TextFragmentBlocked.
  {
    SimRequest::Params params;
    params.response_http_headers.insert("Document-Policy", "force-load-at-top");
    SimRequest request("https://example.com/test2.html#:~:text=foo",
                       "text/html", params);
    LoadURL("https://example.com/test2.html#:~:text=foo");
    request.Complete(R"HTML(
      <!DOCTYPE html>
      <p>This is a test page</p>
    )HTML");
    RunUntilTextFragmentFinalization();

    ASSERT_TRUE(GetDocument().IsUseCounted(WebFeature::kForceLoadAtTop));
    EXPECT_TRUE(GetDocument().IsUseCounted(
        WebFeature::kTextFragmentBlockedByForceLoadAtTop));
  }

  // Ensure that an unblocked text fragment doesn't cause recording the
  // TextFragmentBlocked counter.
  {
    SimRequest request("https://example.com/test3.html#:~:text=foo",
                       "text/html");
    LoadURL("https://example.com/test3.html#:~:text=foo");
    request.Complete(R"HTML(
      <!DOCTYPE html>
      <p>This is a test page</p>
    )HTML");
    RunUntilTextFragmentFinalization();

    ASSERT_FALSE(GetDocument().IsUseCounted(WebFeature::kForceLoadAtTop));
    EXPECT_FALSE(GetDocument().IsUseCounted(
        WebFeature::kTextFragmentBlockedByForceLoadAtTop));
  }
}

TEST_F(TextFragmentAnchorMetricsTest, TextFragmentLinkOpenSource_GoogleDomain) {
  // Set the referrer to a google domain page.
  SimRequest::Params params;
  params.requestor_origin = WebSecurityOrigin::CreateFromString(
      WebString::FromUTF8("https://www.mail.google.com"));
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
  RunUntilTextFragmentFinalization();

  // This should be recorded as coming from an unknown source (not search
  // engine).
  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.LinkOpenSource", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.LinkOpenSource", 0,
                                       1);
}

TEST_F(TextFragmentAnchorMetricsTest, ShadowDOMUseCounter) {
  {
    SimRequest request("https://example.com/test.html#:~:text=RegularDOM",
                       "text/html");
    LoadURL("https://example.com/test.html#:~:text=RegularDOM");
    request.Complete(R"HTML(
      <!DOCTYPE html>
      <p>This is RegularDOM</p>
    )HTML");
    RunUntilTextFragmentFinalization();

    EXPECT_FALSE(
        GetDocument().IsUseCounted(WebFeature::kTextDirectiveInShadowDOM));
  }

  {
    SimRequest request("https://example.com/shadowtest.html#:~:text=ShadowDOM",
                       "text/html");
    LoadURL("https://example.com/shadowtest.html#:~:text=ShadowDOM");
    request.Complete(R"HTML(
      <!DOCTYPE html>
      <p>This is RegularDOM</p>
      <p id="shadow-parent"></p>
      <script>
        let shadow = document.getElementById("shadow-parent").attachShadow({mode: 'open'});
        shadow.innerHTML = '<p id="shadow">This is ShadowDOM</p>';
      </script>
    )HTML");
    RunUntilTextFragmentFinalization();

    EXPECT_TRUE(
        GetDocument().IsUseCounted(WebFeature::kTextDirectiveInShadowDOM));
  }
}

}  // namespace blink
