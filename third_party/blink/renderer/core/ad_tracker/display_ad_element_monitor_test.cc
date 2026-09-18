// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/ad_tracker/display_ad_element_monitor.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

class MockFrameClient : public frame_test_helpers::TestWebFrameClient {
 public:
  MOCK_METHOD(void,
              OnMainFrameAdRectangleChanged,
              (int element_dom_node_id, const gfx::Rect& content_rect),
              (override));
  MOCK_METHOD(void, OnLargeStickyAdDetected, (), (override));
};

}  // namespace

class DisplayAdElementMonitorTest : public testing::Test {
 protected:
  void SetUp() final {
    mock_frame_client_ = std::make_unique<MockFrameClient>();
    helper_.Initialize(mock_frame_client_.get());
    helper_.GetWebView()->Resize(gfx::Size(800, 600));
  }

  Document& GetDocument() {
    return *static_cast<Document*>(helper_.LocalMainFrame()->GetDocument());
  }

  void UpdateLifecycle() {
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  }

  void MarkFirstContentfulPaint() {
    viz::FrameTimingDetails presentation_details;
    presentation_details.presentation_feedback.timestamp =
        task_environment_.NowTicks();

    PaintTiming::From(GetDocument())
        .ReportPresentationTime(PaintEvent::kFirstContentfulPaint,
                                base::TimeTicks(), presentation_details);
  }

  MockFrameClient& MockClient() { return *mock_frame_client_; }

 protected:
  test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<MockFrameClient> mock_frame_client_;
  frame_test_helpers::WebViewHelper helper_;
};

TEST_F(DisplayAdElementMonitorTest, BasicReporting_InsertUpdateRemove) {
  frame_test_helpers::LoadHTMLString(helper_.LocalMainFrame(), R"(
    <img id="ad" style="position:absolute; left:100px; top:50px; width:300px; height:250px;">
  )",
                                     WebURL(KURL("https://example.com")));
  MarkFirstContentfulPaint();
  UpdateLifecycle();

  auto* ad_element =
      To<HTMLImageElement>(GetDocument().getElementById(AtomicString("ad")));

  // Expect the initial position report.
  EXPECT_CALL(MockClient(),
              OnMainFrameAdRectangleChanged(ad_element->GetDomNodeId(),
                                            gfx::Rect(100, 50, 300, 250)));
  ad_element->SetIsAdRelated(NoProvenance{});
  UpdateLifecycle();
  testing::Mock::VerifyAndClearExpectations(&MockClient());

  // Change the element's position and expect a new report with the updated
  // position.
  EXPECT_CALL(MockClient(),
              OnMainFrameAdRectangleChanged(ad_element->GetDomNodeId(),
                                            gfx::Rect(200, 150, 300, 250)));
  ad_element->setAttribute(
      html_names::kStyleAttr,
      AtomicString("position:absolute; left:200px; top:150px; width:300px; "
                   "height:250px;"));
  UpdateLifecycle();
  testing::Mock::VerifyAndClearExpectations(&MockClient());

  // Remove the element and expect an empty rect report.
  const int dom_node_id = ad_element->GetDomNodeId();
  EXPECT_CALL(MockClient(),
              OnMainFrameAdRectangleChanged(dom_node_id, gfx::Rect()));
  ad_element->remove();
  testing::Mock::VerifyAndClearExpectations(&MockClient());
}

TEST_F(DisplayAdElementMonitorTest, ScrollingDoesNotSendNewReport) {
  // Add a large div to make the body taller than the viewport (600px), making
  // it scrollable. The image is placed after this div.
  frame_test_helpers::LoadHTMLString(helper_.LocalMainFrame(), R"(
    <div style="height: 2000px"></div>
    <img id="ad" style="position:absolute; left:100px; top:2050px; width:300px; height:250px;">
  )",
                                     WebURL(KURL("https://example.com")));
  MarkFirstContentfulPaint();
  UpdateLifecycle();

  auto* ad_element =
      To<HTMLImageElement>(GetDocument().getElementById(AtomicString("ad")));

  // The first report should contain the ad's position in document coordinates.
  EXPECT_CALL(MockClient(),
              OnMainFrameAdRectangleChanged(ad_element->GetDomNodeId(),
                                            gfx::Rect(100, 2050, 300, 250)));
  ad_element->SetIsAdRelated(NoProvenance{});
  UpdateLifecycle();
  testing::Mock::VerifyAndClearExpectations(&MockClient());

  // Scroll the page down. This changes the ad's position relative to the
  // viewport but not relative to the document. Expect that no new message is
  // sent because the reported rect is unchanged.
  EXPECT_CALL(MockClient(),
              OnMainFrameAdRectangleChanged(testing::_, testing::_))
      .Times(0);
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 500), mojom::blink::ScrollType::kProgrammatic,
      cc::ScrollSourceType::kNone);
  UpdateLifecycle();
  testing::Mock::VerifyAndClearExpectations(&MockClient());
}

TEST_F(DisplayAdElementMonitorTest, NestedAdElement) {
  // Set up an iframe positioned absolutely in the main document.
  frame_test_helpers::LoadHTMLString(helper_.LocalMainFrame(), R"(
    <iframe id="frame" style="position:absolute; left:100px; top:50px; border:none; width:400px; height:400px;"></iframe>
  )",
                                     WebURL(KURL("https://example.com")));
  MarkFirstContentfulPaint();
  UpdateLifecycle();

  auto* iframe_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  ASSERT_NE(iframe_element->ContentFrame(), nullptr);

  // The iframe content is very tall, making it scrollable. The ad is positioned
  // absolutely within this tall content.
  iframe_element->setAttribute(html_names::kSrcdocAttr, AtomicString(R"HTML(
    <body style="margin:0;">
      <div style="height: 2000px;"></div>
      <img id="ad" style="position:absolute; left:10px; top:1500px; width:300px; height:250px;">
    </body>
  )HTML"));

  UpdateLifecycle();

  // Run pending tasks to allow the iframe's srcdoc to load.
  test::RunPendingTasks();

  Document* iframe_doc = iframe_element->contentDocument();
  ASSERT_NE(iframe_doc, nullptr);

  auto* ad_element =
      To<HTMLImageElement>(iframe_doc->getElementById(AtomicString("ad")));

  // The initial reported position should be relative to the main document.
  // Ad's absolute X = iframe's left (100) + ad's left (10) = 110.
  // Ad's absolute Y = iframe's top (50) + ad's top (1500) = 1550.
  EXPECT_CALL(MockClient(),
              OnMainFrameAdRectangleChanged(ad_element->GetDomNodeId(),
                                            gfx::Rect(110, 1550, 300, 250)));
  ad_element->SetIsAdRelated(NoProvenance{});
  UpdateLifecycle();
  testing::Mock::VerifyAndClearExpectations(&MockClient());

  // Scroll the iframe content down by 200px. This moves the ad "up" by 200px
  // relative to the main document's coordinates. Expect a new report with the
  // updated position.
  // New absolute Y = initial Y (1550) - iframe scroll (200) = 1350.
  EXPECT_CALL(MockClient(),
              OnMainFrameAdRectangleChanged(ad_element->GetDomNodeId(),
                                            gfx::Rect(110, 1350, 300, 250)));
  iframe_doc->View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 200), mojom::blink::ScrollType::kProgrammatic,
      cc::ScrollSourceType::kNone);
  UpdateLifecycle();
  testing::Mock::VerifyAndClearExpectations(&MockClient());
}

TEST_F(DisplayAdElementMonitorTest, AdInitiallyOverlaidAndThenExposed) {
  frame_test_helpers::LoadHTMLString(helper_.LocalMainFrame(), R"(
    <img id="ad" style="position:absolute; left:100px; top:50px; width:300px; height:250px; z-index:1;">
    <div id="overlay" style="position:absolute; left:100px; top:50px; width:300px; height:250px; z-index:2; background-color:red;"></div>
  )",
                                     WebURL(KURL("https://example.com")));

  MarkFirstContentfulPaint();
  UpdateLifecycle();

  auto* ad_element =
      To<HTMLImageElement>(GetDocument().getElementById(AtomicString("ad")));
  auto* overlay_element = GetDocument().getElementById(AtomicString("overlay"));

  // The ad is covered by the overlay div. Expect that no geometry report is
  // sent.
  EXPECT_CALL(MockClient(),
              OnMainFrameAdRectangleChanged(testing::_, testing::_))
      .Times(0);
  ad_element->SetIsAdRelated(NoProvenance{});
  UpdateLifecycle();
  testing::Mock::VerifyAndClearExpectations(&MockClient());

  // Expose the ad by removing the overlay. A report is not sent immediately
  // because the visibility check is throttled.
  EXPECT_CALL(MockClient(),
              OnMainFrameAdRectangleChanged(testing::_, testing::_))
      .Times(0);
  overlay_element->remove();
  UpdateLifecycle();
  testing::Mock::VerifyAndClearExpectations(&MockClient());

  // Fast-forward time past the throttle delay. The monitor should now detect
  // the exposed ad and send a report with its correct geometry.
  EXPECT_CALL(MockClient(),
              OnMainFrameAdRectangleChanged(ad_element->GetDomNodeId(),
                                            gfx::Rect(100, 50, 300, 250)));
  task_environment_.FastForwardBy(base::Seconds(1));
  UpdateLifecycle();
  testing::Mock::VerifyAndClearExpectations(&MockClient());
}

TEST_F(DisplayAdElementMonitorTest,
       AdOutOfViewport_InitiallyOverlaidAndThenScrollsIntoView) {
  // Position the ad and its overlay below the initial viewport.
  frame_test_helpers::LoadHTMLString(helper_.LocalMainFrame(), R"(
    <div style="height: 650px;"></div>
    <img id="ad" style="position:absolute; left:100px; top:700px; width:300px; height:250px; z-index:1;">
    <div id="overlay" style="position:absolute; left:100px; top:700px; width:300px; height:250px; z-index:2; background-color:red;"></div>
  )",
                                     WebURL(KURL("https://example.com")));

  MarkFirstContentfulPaint();
  UpdateLifecycle();

  auto* ad_element =
      To<HTMLImageElement>(GetDocument().getElementById(AtomicString("ad")));

  // Since the ad is outside the viewport, the overlay check is skipped, and
  // it's assumed to be visible by default. A report with its geometry is
  // expected.
  EXPECT_CALL(MockClient(),
              OnMainFrameAdRectangleChanged(ad_element->GetDomNodeId(),
                                            gfx::Rect(100, 700, 300, 250)));
  ad_element->SetIsAdRelated(NoProvenance{});
  UpdateLifecycle();
  testing::Mock::VerifyAndClearExpectations(&MockClient());

  // Scroll the page down so the ad and its overlay are now inside the viewport.
  // The overlay check will now perform a hit-test. The ad is now detected as
  // invisible, so an empty rect should be reported to signal its removal from
  // visibility.
  EXPECT_CALL(MockClient(), OnMainFrameAdRectangleChanged(
                                ad_element->GetDomNodeId(), gfx::Rect()));
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 200), mojom::blink::ScrollType::kProgrammatic,
      cc::ScrollSourceType::kNone);
  task_environment_.FastForwardBy(base::Seconds(1));
  UpdateLifecycle();
  testing::Mock::VerifyAndClearExpectations(&MockClient());
}

TEST_F(DisplayAdElementMonitorTest,
       IframeContainingAd_InitiallyOverlaidAndThenExposed) {
  // Set up the main document with an iframe and an overlay div that has a
  // higher z-index, positioned to cover the iframe.
  frame_test_helpers::LoadHTMLString(helper_.LocalMainFrame(), R"(
    <iframe id="frame" style="position:absolute; left:100px; top:50px; border:none; width:400px; height:400px; z-index:1;"></iframe>
    <div id="overlay" style="position:absolute; left:100px; top:50px; width:400px; height:400px; z-index:2; background-color:red;"></div>
  )",
                                     WebURL(KURL("https://example.com")));

  MarkFirstContentfulPaint();
  UpdateLifecycle();

  auto* iframe_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  ASSERT_NE(iframe_element->ContentFrame(), nullptr);

  auto* overlay_element = GetDocument().getElementById(AtomicString("overlay"));

  // The iframe contains the ad element.
  iframe_element->setAttribute(html_names::kSrcdocAttr, AtomicString(R"HTML(
    <body style="margin:0;">
      <img id="ad" style="position:absolute; left:10px; top:20px; width:300px; height:250px;">
    </body>
  )HTML"));

  // Run pending tasks to allow the iframe's srcdoc to load.
  test::RunPendingTasks();
  UpdateLifecycle();

  Document* iframe_doc = iframe_element->contentDocument();
  ASSERT_NE(iframe_doc, nullptr);

  auto* ad_element =
      To<HTMLImageElement>(iframe_doc->getElementById(AtomicString("ad")));

  // Initially, the iframe (and the ad within it) is covered by the overlay.
  // The hit-test should detect this, and no geometry report should be sent.
  EXPECT_CALL(MockClient(),
              OnMainFrameAdRectangleChanged(testing::_, testing::_))
      .Times(0);
  ad_element->SetIsAdRelated(NoProvenance{});
  UpdateLifecycle();
  testing::Mock::VerifyAndClearExpectations(&MockClient());

  // Now, remove the overlay from the main document, and fast-forward time past
  // the throttle delay. The hit-test should now pass and find the ad element
  // inside the iframe. A report with the ad's geometry, relative to the main
  // frame, should be sent.
  // Ad's absolute X = iframe's left (100) + ad's left (10) = 110.
  // Ad's absolute Y = iframe's top (50) + ad's top (20) = 70.
  EXPECT_CALL(MockClient(),
              OnMainFrameAdRectangleChanged(ad_element->GetDomNodeId(),
                                            gfx::Rect(110, 70, 300, 250)));
  overlay_element->remove();
  task_environment_.FastForwardBy(base::Seconds(1));
  UpdateLifecycle();
  testing::Mock::VerifyAndClearExpectations(&MockClient());
}

TEST_F(DisplayAdElementMonitorTest, RemoveAdInSubframe) {
  // Set up an iframe in the main document.
  frame_test_helpers::LoadHTMLString(helper_.LocalMainFrame(), R"(
    <iframe id="frame" style="position:absolute; left:100px; top:50px; border:none; width:400px; height:400px;"></iframe>
  )",
                                     WebURL(KURL("https://example.com")));
  MarkFirstContentfulPaint();
  UpdateLifecycle();

  auto* iframe_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  iframe_element->setAttribute(html_names::kSrcdocAttr, AtomicString(R"HTML(
    <body style="margin:0;">
      <img id="ad" style="position:absolute; left:10px; top:20px; width:300px; height:250px;">
    </body>
  )HTML"));

  test::RunPendingTasks();
  UpdateLifecycle();

  Document* iframe_doc = iframe_element->contentDocument();
  auto* ad_element =
      To<HTMLImageElement>(iframe_doc->getElementById(AtomicString("ad")));

  // Expect the initial position report relative to the main document.
  EXPECT_CALL(MockClient(),
              OnMainFrameAdRectangleChanged(ad_element->GetDomNodeId(),
                                            gfx::Rect(110, 70, 300, 250)));
  ad_element->SetIsAdRelated(NoProvenance{});
  UpdateLifecycle();
  testing::Mock::VerifyAndClearExpectations(&MockClient());

  // Remove the element from the subframe and expect an empty rect report on the
  // main frame's client.
  const int dom_node_id = ad_element->GetDomNodeId();
  EXPECT_CALL(MockClient(),
              OnMainFrameAdRectangleChanged(dom_node_id, gfx::Rect()));
  ad_element->remove();
  testing::Mock::VerifyAndClearExpectations(&MockClient());
}

TEST_F(DisplayAdElementMonitorTest, ReportingForAdIframe_InsertUpdateHide) {
  frame_test_helpers::LoadHTMLString(helper_.LocalMainFrame(), R"(
    <iframe id="ad" style="position:absolute; left:100px; top:50px; width:300px; height:250px; border:none;"></iframe>
  )",
                                     WebURL(KURL("https://example.com")));
  MarkFirstContentfulPaint();
  UpdateLifecycle();

  auto* ad_element =
      To<HTMLIFrameElement>(GetDocument().getElementById(AtomicString("ad")));
  ASSERT_NE(ad_element->ContentFrame(), nullptr);

  // Expect the initial position report when the iframe is marked as an ad.
  EXPECT_CALL(MockClient(),
              OnMainFrameAdRectangleChanged(ad_element->GetDomNodeId(),
                                            gfx::Rect(100, 50, 300, 250)));
  FrameAdEvidence ad_evidence;
  ad_evidence.set_created_by_ad_script(
      mojom::FrameCreationStackEvidence::kCreatedByAdScript);
  ad_evidence.set_is_complete();
  To<LocalFrame>(*ad_element->ContentFrame()).SetAdEvidence(ad_evidence);
  UpdateLifecycle();
  testing::Mock::VerifyAndClearExpectations(&MockClient());

  // Change the element's position and expect a new report with the updated
  // position.
  EXPECT_CALL(MockClient(),
              OnMainFrameAdRectangleChanged(ad_element->GetDomNodeId(),
                                            gfx::Rect(200, 150, 300, 250)));
  ad_element->setAttribute(
      html_names::kStyleAttr,
      AtomicString("position:absolute; left:200px; top:150px; width:300px; "
                   "height:250px; border:none;"));
  UpdateLifecycle();
  testing::Mock::VerifyAndClearExpectations(&MockClient());

  // Hide the element by setting "display: none" and expect an empty rect
  // report.
  const int dom_node_id = ad_element->GetDomNodeId();
  EXPECT_CALL(MockClient(),
              OnMainFrameAdRectangleChanged(dom_node_id, gfx::Rect()));
  ad_element->setAttribute(html_names::kStyleAttr,
                           AtomicString("display:none;"));
  UpdateLifecycle();
  testing::Mock::VerifyAndClearExpectations(&MockClient());
}

TEST_F(DisplayAdElementMonitorTest, LargeStickyAdDetected) {
  // Add a large div to make the body scrollable.
  // Viewport is 800x600. Threshold is 0.3 * 480000 = 144000.
  // Ad is 800x200 = 160000.
  // Placed at the bottom: 600 * 0.9 = 540. The ad bottom is at 400 + 200 = 600.
  frame_test_helpers::LoadHTMLString(helper_.LocalMainFrame(), R"(
    <div style="height: 2000px"></div>
    <img id="ad" style="position:fixed; left:0px; top:400px; width:800px; height:200px;">
  )",
                                     WebURL(KURL("https://example.com")));
  MarkFirstContentfulPaint();
  UpdateLifecycle();

  auto* ad_element =
      To<HTMLImageElement>(GetDocument().getElementById(AtomicString("ad")));

  EXPECT_CALL(MockClient(),
              OnMainFrameAdRectangleChanged(testing::_, testing::_))
      .Times(testing::AnyNumber());
  ad_element->SetIsAdRelated(NoProvenance{});
  UpdateLifecycle();

  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kLargeStickyAd));
  EXPECT_CALL(MockClient(), OnLargeStickyAdDetected()).Times(0);

  // Scroll down. The distance should be > ad height (200px).
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 250), mojom::blink::ScrollType::kProgrammatic,
      cc::ScrollSourceType::kNone);

  EXPECT_CALL(MockClient(), OnLargeStickyAdDetected()).Times(1);
  UpdateLifecycle();

  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kLargeStickyAd));
}

TEST_F(DisplayAdElementMonitorTest, StickyAdNotLargeDoesNotTriggerUseCounter) {
  // Ad is smaller than 30% of viewport.
  frame_test_helpers::LoadHTMLString(helper_.LocalMainFrame(), R"(
    <div style="height: 2000px"></div>
    <img id="ad" style="position:fixed; left:0px; top:500px; width:100px; height:100px;">
  )",
                                     WebURL(KURL("https://example.com")));
  MarkFirstContentfulPaint();
  UpdateLifecycle();

  auto* ad_element =
      To<HTMLImageElement>(GetDocument().getElementById(AtomicString("ad")));

  EXPECT_CALL(MockClient(),
              OnMainFrameAdRectangleChanged(testing::_, testing::_))
      .Times(testing::AnyNumber());
  ad_element->SetIsAdRelated(NoProvenance{});
  UpdateLifecycle();

  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kLargeStickyAd));

  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 150), mojom::blink::ScrollType::kProgrammatic,
      cc::ScrollSourceType::kNone);

  EXPECT_CALL(MockClient(), OnLargeStickyAdDetected()).Times(0);
  UpdateLifecycle();

  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kLargeStickyAd));
}

TEST_F(DisplayAdElementMonitorTest,
       StickyAdNotAtBottomDoesNotTriggerUseCounter) {
  // Viewport is 800x600. Threshold is 0.3 * 480000 = 144000.
  // Ad is large (800x200 = 160000 > 144000) but placed at the center (top:
  // 200px). The bottom of the ad is at 400px, which is not >= 540px (90% of
  // 600px).
  frame_test_helpers::LoadHTMLString(helper_.LocalMainFrame(), R"(
    <div style="height: 2000px"></div>
    <img id="ad" style="position:fixed; left:0px; top:200px; width:800px; height:200px;">
  )",
                                     WebURL(KURL("https://example.com")));
  MarkFirstContentfulPaint();
  UpdateLifecycle();

  auto* ad_element =
      To<HTMLImageElement>(GetDocument().getElementById(AtomicString("ad")));

  EXPECT_CALL(MockClient(),
              OnMainFrameAdRectangleChanged(testing::_, testing::_))
      .Times(testing::AnyNumber());
  ad_element->SetIsAdRelated(NoProvenance{});
  UpdateLifecycle();

  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kLargeStickyAd));

  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 250), mojom::blink::ScrollType::kProgrammatic,
      cc::ScrollSourceType::kNone);

  EXPECT_CALL(MockClient(), OnLargeStickyAdDetected()).Times(0);
  UpdateLifecycle();

  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kLargeStickyAd));
}

TEST_F(DisplayAdElementMonitorTest,
       LargeAdAtBottomNotStickyDoesNotTriggerUseCounter) {
  frame_test_helpers::LoadHTMLString(helper_.LocalMainFrame(), R"(
    <div style="height: 2000px"></div>
    <img id="ad" style="position:absolute; left:0px; top:400px; width:800px; height:200px;">
  )",
                                     WebURL(KURL("https://example.com")));
  MarkFirstContentfulPaint();
  UpdateLifecycle();

  auto* ad_element =
      To<HTMLImageElement>(GetDocument().getElementById(AtomicString("ad")));

  EXPECT_CALL(MockClient(),
              OnMainFrameAdRectangleChanged(testing::_, testing::_))
      .Times(testing::AnyNumber());
  ad_element->SetIsAdRelated(NoProvenance{});
  UpdateLifecycle();

  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kLargeStickyAd));

  // Scroll down. The ad is absolutely positioned, so it moves up with the page.
  // The monitor will discard the anchor because the ad's relative position
  // to the viewport changes significantly.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 250), mojom::blink::ScrollType::kProgrammatic,
      cc::ScrollSourceType::kNone);

  EXPECT_CALL(MockClient(), OnLargeStickyAdDetected()).Times(0);
  UpdateLifecycle();

  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kLargeStickyAd));
}

TEST_F(DisplayAdElementMonitorTest,
       StaticAdWithScrollAnchoringDoesNotTriggerUseCounter) {
  // Set up a static in-flow ad at the bottom of the viewport (400px down in a
  // 600px tall viewport).
  frame_test_helpers::LoadHTMLString(helper_.LocalMainFrame(), R"(
    <div id="spacer" style="height: 400px"></div>
    <img id="ad" style="display:block; width:800px; height:200px;">
    <div style="height: 2000px"></div>
  )",
                                     WebURL(KURL("https://example.com")));
  MarkFirstContentfulPaint();
  UpdateLifecycle();

  auto* ad_element =
      To<HTMLImageElement>(GetDocument().getElementById(AtomicString("ad")));

  EXPECT_CALL(MockClient(),
              OnMainFrameAdRectangleChanged(testing::_, testing::_))
      .Times(testing::AnyNumber());
  ad_element->SetIsAdRelated(NoProvenance{});
  UpdateLifecycle();

  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kLargeStickyAd));

  // Simulate scroll anchoring: content above grows by 250px (spacer height
  // expands to 650px), and the scroll offset adjusts by 250px to keep the
  // content in place within the viewport.
  auto* spacer_element = GetDocument().getElementById(AtomicString("spacer"));
  spacer_element->setAttribute(html_names::kStyleAttr,
                               AtomicString("height: 650px;"));
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 250), mojom::blink::ScrollType::kProgrammatic,
      cc::ScrollSourceType::kNone);

  // The ad remains at the same viewport-relative position (400px), and the
  // scroll offset changed by > ad_height (250px > 200px). Because the ad is
  // in-flow (static), it should not be considered sticky.
  EXPECT_CALL(MockClient(), OnLargeStickyAdDetected()).Times(0);
  UpdateLifecycle();

  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kLargeStickyAd));
}

TEST_F(DisplayAdElementMonitorTest, ParallaxAdDoesNotTriggerUseCounter) {
  // Set up an ad that is fixed and large, but initially covered by an overlay.
  frame_test_helpers::LoadHTMLString(helper_.LocalMainFrame(), R"(
    <div style="position:absolute; left:0px; top:0px; width:800px; height:600px; z-index:3; background-color:white;"></div>
    <div style="position:absolute; left:0px; top:600px; width:800px; height:600px; z-index:1;"></div>
    <div style="position:absolute; left:0px; top:1200px; width:800px; height:1000px; z-index:3; background-color:white;"></div>
    <img id="ad" style="position:fixed; left:0px; top:0px; width:800px; height:600px; z-index:2; background-color:blue;">
  )",
                                     WebURL(KURL("https://example.com")));

  auto* ad_element =
      To<HTMLImageElement>(GetDocument().getElementById(AtomicString("ad")));

  EXPECT_CALL(MockClient(),
              OnMainFrameAdRectangleChanged(testing::_, testing::_))
      .Times(testing::AnyNumber());
  ad_element->SetIsAdRelated(NoProvenance{});

  // Scroll down to the position where the parallax-ad is no longer
  // covered by the first overlay and becomes visible.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 600), mojom::blink::ScrollType::kProgrammatic,
      cc::ScrollSourceType::kNone);

  MarkFirstContentfulPaint();
  UpdateLifecycle();

  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kLargeStickyAd));
  EXPECT_CALL(MockClient(), OnLargeStickyAdDetected()).Times(0);

  testing::Mock::VerifyAndClearExpectations(&MockClient());

  // Scroll further down to the position where the parallax-ad is covered by
  // the second overlay and becomes invisible again. Note we must scroll past
  // the ad's height (600px) to trigger the sticky check.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 1201), mojom::blink::ScrollType::kProgrammatic,
      cc::ScrollSourceType::kNone);

  // When CalculateStickyAdState determines that the ad is invisible, an empty
  // rect should be reported rather than an updated rect_to_report.
  EXPECT_CALL(MockClient(), OnMainFrameAdRectangleChanged(
                                ad_element->GetDomNodeId(), gfx::Rect()))
      .Times(1);
  UpdateLifecycle();
  testing::Mock::VerifyAndClearExpectations(&MockClient());

  // The ad is now covered again. Even though it is large and fixed, it
  // shouldn't trigger the use counter.
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kLargeStickyAd));
}

TEST_F(DisplayAdElementMonitorTest, LargeStickyAdDetectedWithJitter) {
  // Viewport is 800x600. Ad is 800x200.
  // 20% tolerance of 200px is 40px.
  // We simulate jitter of 30px, which is within the tolerance.
  frame_test_helpers::LoadHTMLString(helper_.LocalMainFrame(), R"(
    <div style="height: 2000px"></div>
    <img id="ad" style="position:absolute; left:0px; top:400px; width:800px; height:200px;">
  )",
                                     WebURL(KURL("https://example.com")));
  MarkFirstContentfulPaint();
  UpdateLifecycle();

  auto* ad_element =
      To<HTMLImageElement>(GetDocument().getElementById(AtomicString("ad")));

  EXPECT_CALL(MockClient(),
              OnMainFrameAdRectangleChanged(testing::_, testing::_))
      .Times(testing::AnyNumber());
  ad_element->SetIsAdRelated(NoProvenance{});
  UpdateLifecycle();

  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kLargeStickyAd));
  EXPECT_CALL(MockClient(), OnLargeStickyAdDetected()).Times(1);

  // Scroll down incrementally, simulating jitter at each step.
  // Jitter is 30px, which is <= 40px tolerance.
  int current_scroll = 0;
  while (current_scroll < 250) {
    current_scroll += 30;

    // Simulate scroll
    GetDocument().View()->LayoutViewport()->SetScrollOffset(
        ScrollOffset(0, current_scroll),
        mojom::blink::ScrollType::kProgrammatic, cc::ScrollSourceType::kNone);

    // In the first lifecycle after scroll, the ad is temporarily out of place
    // (moved up by 30px in the viewport).
    UpdateLifecycle();

    // JS repositioning brings it back to the bottom.
    ad_element->setAttribute(html_names::kStyleAttr,
                             AtomicString("position:absolute; left:0px; top:" +
                                          String::Number(400 + current_scroll) +
                                          "px; width:800px; height:200px;"));
    UpdateLifecycle();
  }

  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kLargeStickyAd));
}

TEST_F(DisplayAdElementMonitorTest, LargeStickyAdNotDetectedWithLargeJitter) {
  // Viewport is 800x600. Ad is 800x200.
  // 20% tolerance of 200px is 40px.
  // We simulate jitter of 50px, which is strictly > the 40px tolerance.
  // This causes the anchor to be discarded, so it should never trigger.
  frame_test_helpers::LoadHTMLString(helper_.LocalMainFrame(), R"(
    <div style="height: 2000px"></div>
    <img id="ad" style="position:absolute; left:0px; top:400px; width:800px; height:200px;">
  )",
                                     WebURL(KURL("https://example.com")));
  MarkFirstContentfulPaint();
  UpdateLifecycle();

  auto* ad_element =
      To<HTMLImageElement>(GetDocument().getElementById(AtomicString("ad")));

  EXPECT_CALL(MockClient(),
              OnMainFrameAdRectangleChanged(testing::_, testing::_))
      .Times(testing::AnyNumber());
  ad_element->SetIsAdRelated(NoProvenance{});
  UpdateLifecycle();

  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kLargeStickyAd));
  EXPECT_CALL(MockClient(), OnLargeStickyAdDetected()).Times(0);

  // Scroll down incrementally, simulating jitter at each step.
  // Jitter is 50px, which is > 40px tolerance.
  int current_scroll = 0;
  while (current_scroll < 250) {
    current_scroll += 50;

    // Simulate scroll
    GetDocument().View()->LayoutViewport()->SetScrollOffset(
        ScrollOffset(0, current_scroll),
        mojom::blink::ScrollType::kProgrammatic, cc::ScrollSourceType::kNone);

    // In the first lifecycle after scroll, the ad is temporarily out of place.
    // The anchor should be discarded.
    UpdateLifecycle();

    // JS repositioning brings it back to the bottom.
    // A new anchor is established, but the scroll distance from the new anchor
    // is 0, so the sticky condition is never met.
    ad_element->setAttribute(html_names::kStyleAttr,
                             AtomicString("position:absolute; left:0px; top:" +
                                          String::Number(400 + current_scroll) +
                                          "px; width:800px; height:200px;"));
    UpdateLifecycle();
  }

  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kLargeStickyAd));
}

TEST_F(DisplayAdElementMonitorTest, StickyVideoAdDetected) {
  frame_test_helpers::LoadHTMLString(helper_.LocalMainFrame(), R"(
    <div style="height: 2000px"></div>
    <img id="ad" style="position:fixed; left:0px; top:400px; width:800px; height:200px;">
  )",
                                     WebURL(KURL("https://example.com")));
  MarkFirstContentfulPaint();
  UpdateLifecycle();

  auto* ad_element =
      To<HTMLImageElement>(GetDocument().getElementById(AtomicString("ad")));

  EXPECT_CALL(MockClient(),
              OnMainFrameAdRectangleChanged(testing::_, testing::_))
      .Times(testing::AnyNumber());
  EXPECT_CALL(MockClient(), OnLargeStickyAdDetected())
      .Times(testing::AnyNumber());
  ad_element->SetIsAdRelated(NoProvenance{});
  ad_element->UpdateToVideoAd();
  UpdateLifecycle();

  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kVideoAdDetected));
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kStickyVideoAdDetected));

  // Scroll down. The distance should be > ad height (200px).
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 250), mojom::blink::ScrollType::kProgrammatic,
      cc::ScrollSourceType::kNone);

  UpdateLifecycle();

  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kStickyVideoAdDetected));
}

TEST_F(DisplayAdElementMonitorTest, StickyNonVideoAd) {
  frame_test_helpers::LoadHTMLString(helper_.LocalMainFrame(), R"(
    <div style="height: 2000px"></div>
    <img id="ad" style="position:fixed; left:0px; top:400px; width:800px; height:200px;">
  )",
                                     WebURL(KURL("https://example.com")));
  MarkFirstContentfulPaint();
  UpdateLifecycle();

  auto* ad_element =
      To<HTMLImageElement>(GetDocument().getElementById(AtomicString("ad")));

  EXPECT_CALL(MockClient(),
              OnMainFrameAdRectangleChanged(testing::_, testing::_))
      .Times(testing::AnyNumber());
  EXPECT_CALL(MockClient(), OnLargeStickyAdDetected()).Times(0);
  ad_element->SetIsAdRelated(NoProvenance{});
  UpdateLifecycle();

  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kStickyVideoAdDetected));

  // Scroll down. The distance should be > ad height (200px).
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 250), mojom::blink::ScrollType::kProgrammatic,
      cc::ScrollSourceType::kNone);

  EXPECT_CALL(MockClient(), OnLargeStickyAdDetected()).Times(1);
  UpdateLifecycle();

  // Stickiness is detected for the ad, but it is not a video ad, so
  // kStickyVideoAdDetected must not be counted.
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kLargeStickyAd));
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kStickyVideoAdDetected));
}

TEST_F(DisplayAdElementMonitorTest, NonStickyVideoAd) {
  frame_test_helpers::LoadHTMLString(helper_.LocalMainFrame(), R"(
    <div style="height: 2000px"></div>
    <img id="ad" style="position:absolute; left:0px; top:400px; width:800px; height:200px;">
  )",
                                     WebURL(KURL("https://example.com")));
  MarkFirstContentfulPaint();
  UpdateLifecycle();

  auto* ad_element =
      To<HTMLImageElement>(GetDocument().getElementById(AtomicString("ad")));

  EXPECT_CALL(MockClient(),
              OnMainFrameAdRectangleChanged(testing::_, testing::_))
      .Times(testing::AnyNumber());
  EXPECT_CALL(MockClient(), OnLargeStickyAdDetected()).Times(0);
  ad_element->SetIsAdRelated(NoProvenance{});
  ad_element->UpdateToVideoAd();
  UpdateLifecycle();

  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kStickyVideoAdDetected));

  // Scroll down. The ad is absolutely positioned, so it moves with the page
  // and is not sticky.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 250), mojom::blink::ScrollType::kProgrammatic,
      cc::ScrollSourceType::kNone);

  UpdateLifecycle();

  // Stickiness is not detected for the video ad, so
  // kStickyVideoAdDetected must not be counted.
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kStickyVideoAdDetected));
}

TEST_F(DisplayAdElementMonitorTest,
       StickyAdUpdatedToVideoAdAfterStickinessDetected) {
  frame_test_helpers::LoadHTMLString(helper_.LocalMainFrame(), R"(
    <div style="height: 2000px"></div>
    <img id="ad" style="position:fixed; left:0px; top:400px; width:800px; height:200px;">
  )",
                                     WebURL(KURL("https://example.com")));
  MarkFirstContentfulPaint();
  UpdateLifecycle();

  auto* ad_element =
      To<HTMLImageElement>(GetDocument().getElementById(AtomicString("ad")));

  EXPECT_CALL(MockClient(),
              OnMainFrameAdRectangleChanged(testing::_, testing::_))
      .Times(testing::AnyNumber());
  EXPECT_CALL(MockClient(), OnLargeStickyAdDetected()).Times(0);
  ad_element->SetIsAdRelated(NoProvenance{});
  UpdateLifecycle();

  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kStickyVideoAdDetected));

  // Scroll down. The distance should be > ad height (200px).
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 250), mojom::blink::ScrollType::kProgrammatic,
      cc::ScrollSourceType::kNone);

  EXPECT_CALL(MockClient(), OnLargeStickyAdDetected()).Times(1);
  UpdateLifecycle();

  // Stickiness is confirmed detected, but not yet tagged as video ad.
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kLargeStickyAd));
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kVideoAdDetected));
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kStickyVideoAdDetected));

  // Tag the ad as video ad after stickiness detection.
  ad_element->UpdateToVideoAd();
  UpdateLifecycle();

  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kVideoAdDetected));
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kStickyVideoAdDetected));
}

}  // namespace blink
