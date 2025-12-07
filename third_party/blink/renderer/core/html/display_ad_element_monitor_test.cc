// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/display_ad_element_monitor.h"

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
  ad_element->SetIsAdRelated();
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
  ad_element->SetIsAdRelated();
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
  ad_element->SetIsAdRelated();
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
  ad_element->SetIsAdRelated();
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
  ad_element->SetIsAdRelated();
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
  ad_element->SetIsAdRelated();
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
  DynamicTo<LocalFrame>(ad_element->ContentFrame())->SetAdEvidence(ad_evidence);
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

TEST_F(DisplayAdElementMonitorTest, ReportingForAdIframe_UntagAsAd) {
  frame_test_helpers::LoadHTMLString(helper_.LocalMainFrame(), R"(
    <iframe id="ad" style="position:absolute; left:100px; top:50px; width:300px; height:250px; border:none;"></iframe>
  )",
                                     WebURL(KURL("https://example.com")));
  MarkFirstContentfulPaint();
  UpdateLifecycle();

  auto* ad_element =
      To<HTMLIFrameElement>(GetDocument().getElementById(AtomicString("ad")));
  ASSERT_NE(ad_element->ContentFrame(), nullptr);

  WebRemoteFrameImpl* remote_frame = frame_test_helpers::CreateRemote();
  frame_test_helpers::SwapRemoteFrame(helper_.LocalMainFrame()->FirstChild(),
                                      remote_frame);

  // Expect the initial position report when the iframe is marked as an ad.
  EXPECT_CALL(MockClient(),
              OnMainFrameAdRectangleChanged(ad_element->GetDomNodeId(),
                                            gfx::Rect(100, 50, 300, 250)));
  DynamicTo<RemoteFrame>(ad_element->ContentFrame())
      ->SetReplicatedIsAdFrame(true);
  UpdateLifecycle();
  testing::Mock::VerifyAndClearExpectations(&MockClient());

  // Untag the element as ad, and expect an empty rect report.
  const int dom_node_id = ad_element->GetDomNodeId();
  EXPECT_CALL(MockClient(),
              OnMainFrameAdRectangleChanged(dom_node_id, gfx::Rect()));
  DynamicTo<RemoteFrame>(ad_element->ContentFrame())
      ->SetReplicatedIsAdFrame(false);
  UpdateLifecycle();
  testing::Mock::VerifyAndClearExpectations(&MockClient());
}

}  // namespace blink
