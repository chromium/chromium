// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/anchor_element_metrics.h"

#include <optional>

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/loader/navigation_predictor.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class AnchorElementMetricsTest : public SimTest {
 public:
  static constexpr int kViewportWidth = 400;
  static constexpr int kViewportHeight = 600;

 protected:
  AnchorElementMetricsTest() = default;

  void SetUp() override {
    SimTest::SetUp();
    WebView().MainFrameWidget()->Resize(
        gfx::Size(kViewportWidth, kViewportHeight));
    feature_list_.InitAndEnableFeature(features::kNavigationPredictor);
  }

  mojom::blink::AnchorElementMetricsPtr CreateAnchorMetrics(
      const String& source,
      const String& target) {
    SimRequest main_resource(source, "text/html");
    LoadURL(source);
    main_resource.Complete("<a id='anchor' href=''>example</a>");

    auto* anchor_element = To<HTMLAnchorElement>(
        GetDocument().getElementById(AtomicString("anchor")));
    anchor_element->SetHref(AtomicString(target));
    // We need layout to have happened before calling
    // CreateAnchorElementMetrics.
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();
    return CreateAnchorElementMetrics(*anchor_element);
  }

  base::test::ScopedFeatureList feature_list_;
};

constexpr int AnchorElementMetricsTest::kViewportWidth;
constexpr int AnchorElementMetricsTest::kViewportHeight;

TEST_F(AnchorElementMetricsTest, ViewportSize) {
  auto metrics =
      CreateAnchorMetrics("http://example.com/p1", "http://example.com/p2");
  EXPECT_EQ(metrics->viewport_size.width(),
            AnchorElementMetricsTest::kViewportWidth);
  EXPECT_EQ(metrics->viewport_size.height(),
            AnchorElementMetricsTest::kViewportHeight);
}

// Test for is_url_incremented_by_one.
TEST_F(AnchorElementMetricsTest, IsUrlIncrementedByOne) {
  EXPECT_TRUE(
      CreateAnchorMetrics("http://example.com/p1", "http://example.com/p2")
          ->is_url_incremented_by_one);
  EXPECT_TRUE(
      CreateAnchorMetrics("http://example.com/?p=9", "http://example.com/?p=10")
          ->is_url_incremented_by_one);
  EXPECT_TRUE(CreateAnchorMetrics("http://example.com/?p=12",
                                  "http://example.com/?p=13")
                  ->is_url_incremented_by_one);
  EXPECT_TRUE(CreateAnchorMetrics("http://example.com/p9/cat1",
                                  "http://example.com/p10/cat1")
                  ->is_url_incremented_by_one);
  EXPECT_FALSE(
      CreateAnchorMetrics("http://example.com/1", "https://example.com/2")
          ->is_url_incremented_by_one);
  EXPECT_FALSE(
      CreateAnchorMetrics("http://example.com/1", "http://google.com/2")
          ->is_url_incremented_by_one);
  EXPECT_FALSE(
      CreateAnchorMetrics("http://example.com/p1", "http://example.com/p1")
          ->is_url_incremented_by_one);
  EXPECT_FALSE(
      CreateAnchorMetrics("http://example.com/p2", "http://example.com/p1")
          ->is_url_incremented_by_one);
  EXPECT_FALSE(CreateAnchorMetrics("http://example.com/p9/cat1",
                                   "http://example.com/p10/cat2")
                   ->is_url_incremented_by_one);
}

// The main frame contains an anchor element, which contains an image element.
TEST_F(AnchorElementMetricsTest, AnchorFeatureImageLink) {
  SimRequest main_resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
    <body style='margin: 0px'>
    <div style='height: %dpx;'></div>
    <a id='anchor' href="https://example.com/page2">
      <img height="300" width="200">
    </a>
    <div style='height: %d;'></div>
    </body>)HTML",
      kViewportHeight / 2, 10 * kViewportHeight));

  Element* anchor = GetDocument().getElementById(AtomicString("anchor"));
  auto* anchor_element = To<HTMLAnchorElement>(anchor);

  auto metrics = CreateAnchorElementMetrics(*anchor_element);
  EXPECT_FALSE(metrics->is_in_iframe);
  EXPECT_TRUE(metrics->contains_image);
  EXPECT_TRUE(metrics->is_same_host);
  EXPECT_FALSE(metrics->is_url_incremented_by_one);
}

// The main frame contains one anchor element without a text sibling.
TEST_F(AnchorElementMetricsTest, AnchorWithoutTextSibling) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(
      R"HTML(<body><a id='anchor' href="https://example.com/page2">foo</a></body>)HTML");

  Element* anchor = GetDocument().getElementById(AtomicString("anchor"));
  auto* anchor_element = To<HTMLAnchorElement>(anchor);

  auto metrics = CreateAnchorElementMetrics(*anchor_element);
  EXPECT_FALSE(metrics->has_text_sibling);
}

// The main frame contains one anchor element with empty text siblings.
TEST_F(AnchorElementMetricsTest, AnchorWithEmptyTextSibling) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(
      R"HTML(<body> <a id='anchor' href="https://example.com/page2">foo</a> </body>)HTML");

  Element* anchor = GetDocument().getElementById(AtomicString("anchor"));
  auto* anchor_element = To<HTMLAnchorElement>(anchor);

  auto metrics = CreateAnchorElementMetrics(*anchor_element);
  EXPECT_FALSE(metrics->has_text_sibling);
}

// The main frame contains one anchor element with a previous text sibling.
TEST_F(AnchorElementMetricsTest, AnchorWithPreviousTextSibling) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(
      R"HTML(<body>bar<a id='anchor' href="https://example.com/page2">foo</a></body>)HTML");

  Element* anchor = GetDocument().getElementById(AtomicString("anchor"));
  auto* anchor_element = To<HTMLAnchorElement>(anchor);

  auto metrics = CreateAnchorElementMetrics(*anchor_element);
  EXPECT_TRUE(metrics->has_text_sibling);
}

// The main frame contains one anchor element with a next text sibling.
TEST_F(AnchorElementMetricsTest, AnchorWithNextTextSibling) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(
      R"HTML(<body><a id='anchor' href="https://example.com/page2">foo</a>bar</body>)HTML");

  Element* anchor = GetDocument().getElementById(AtomicString("anchor"));
  auto* anchor_element = To<HTMLAnchorElement>(anchor);

  auto metrics = CreateAnchorElementMetrics(*anchor_element);
  EXPECT_TRUE(metrics->has_text_sibling);
}

// The main frame contains one anchor element with a font size of 23px.
TEST_F(AnchorElementMetricsTest, AnchorFontSize) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(
      R"HTML(<body><a id='anchor' style="font-size: 23px" href="https://example.com/page2">foo</a>bar</body>)HTML");

  Element* anchor = GetDocument().getElementById(AtomicString("anchor"));
  auto* anchor_element = To<HTMLAnchorElement>(anchor);

  auto metrics = CreateAnchorElementMetrics(*anchor_element);
  EXPECT_EQ(metrics->font_size_px, 23u);
}

// The main frame contains one anchor element with a font weight of 438.
TEST_F(AnchorElementMetricsTest, AnchorFontWeight) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(
      R"HTML(<body><a id='anchor' style='font-weight: 438' href="https://example.com/page2">foo</a>bar</body>)HTML");

  Element* anchor = GetDocument().getElementById(AtomicString("anchor"));
  auto* anchor_element = To<HTMLAnchorElement>(anchor);

  auto metrics = CreateAnchorElementMetrics(*anchor_element);
  EXPECT_EQ(metrics->font_weight, 438u);
}

// The main frame contains an anchor element.
// Features of the element are extracted.
// Then the test scrolls down to check features again.
TEST_F(AnchorElementMetricsTest, AnchorFeatureExtract) {
  SimRequest main_resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
    <body style='margin: 0px'>
    <div style='height: %dpx;'></div>
    <a id='anchor' href="https://b.example.com">example</a>
    <div style='height: %d;'></div>
    </body>)HTML",
      2 * kViewportHeight, 10 * kViewportHeight));

  Element* anchor = GetDocument().getElementById(AtomicString("anchor"));
  auto* anchor_element = To<HTMLAnchorElement>(anchor);

  auto metrics = CreateAnchorElementMetrics(*anchor_element);

  // Element not in the viewport.
  EXPECT_FALSE(metrics->is_in_iframe);
  EXPECT_FALSE(metrics->contains_image);
  EXPECT_FALSE(metrics->is_same_host);
  EXPECT_FALSE(metrics->is_url_incremented_by_one);

  // Scroll down to the anchor element.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, kViewportHeight * 1.5),
      mojom::blink::ScrollType::kProgrammatic);

  auto metrics2 = CreateAnchorElementMetrics(*anchor_element);
  EXPECT_FALSE(metrics2->is_in_iframe);
  EXPECT_FALSE(metrics2->contains_image);
  EXPECT_FALSE(metrics2->is_same_host);
  EXPECT_FALSE(metrics2->is_url_incremented_by_one);
}

// The main frame contains an iframe. The iframe contains an anchor element.
// Features of the element are extracted.
// Then the test scrolls down in the main frame to check features again.
// Then the test scrolls down in the iframe to check features again.
TEST_F(AnchorElementMetricsTest, AnchorFeatureInIframe) {
  SimRequest main_resource("https://example.com/page1", "text/html");
  SimRequest iframe_resource("https://example.com/iframe.html", "text/html");
  SimSubresourceRequest image_resource("https://example.com/cat.png",
                                       "image/png");

  LoadURL("https://example.com/page1");

  main_resource.Complete(String::Format(
      R"HTML(
        <body style='margin: 0px'>
        <div style='height: %dpx;'></div>
        <iframe id='iframe' src='https://example.com/iframe.html'
            style='width: 300px; height: %dpx;
            border-style: none; padding: 0px; margin: 0px;'></iframe>
        <div style='height: %dpx;'></div>
        </body>)HTML",
      2 * kViewportHeight, kViewportHeight / 2, 10 * kViewportHeight));

  iframe_resource.Complete(String::Format(
      R"HTML(
    <body style='margin: 0px'>
    <div style='height: %dpx;'></div>
    <a id='anchor' href="https://example.com/page2">example</a>
    <div style='height: %dpx;'></div>
    </body>)HTML",
      kViewportHeight / 2, 5 * kViewportHeight));

  Element* iframe = GetDocument().getElementById(AtomicString("iframe"));
  auto* iframe_element = To<HTMLIFrameElement>(iframe);
  Frame* sub = iframe_element->ContentFrame();
  auto* subframe = To<LocalFrame>(sub);

  Element* anchor =
      subframe->GetDocument()->getElementById(AtomicString("anchor"));
  auto* anchor_element = To<HTMLAnchorElement>(anchor);

  // We need layout to have happened before calling
  // CreateAnchorElementMetrics.
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  auto metrics = CreateAnchorElementMetrics(*anchor_element);
  EXPECT_TRUE(metrics->is_in_iframe);
  EXPECT_FALSE(metrics->contains_image);
  EXPECT_TRUE(metrics->is_same_host);
  EXPECT_TRUE(metrics->is_url_incremented_by_one);

  // Scroll down the main frame.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, kViewportHeight * 1.8),
      mojom::blink::ScrollType::kProgrammatic);
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  auto metrics2 = CreateAnchorElementMetrics(*anchor_element);
  EXPECT_TRUE(metrics2->is_in_iframe);
  EXPECT_FALSE(metrics2->contains_image);
  EXPECT_TRUE(metrics2->is_same_host);
  EXPECT_TRUE(metrics2->is_url_incremented_by_one);

  // Scroll down inside iframe. Now the anchor element is visible.
  subframe->View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, kViewportHeight * 0.2),
      mojom::blink::ScrollType::kProgrammatic);
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  auto metrics3 = CreateAnchorElementMetrics(*anchor_element);
  EXPECT_TRUE(metrics3->is_in_iframe);
  EXPECT_FALSE(metrics3->contains_image);
  EXPECT_TRUE(metrics3->is_same_host);
  EXPECT_TRUE(metrics3->is_url_incremented_by_one);
}

}  // namespace blink
