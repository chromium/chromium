// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/css/css_style_declaration.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/scrolling/element_fragment_anchor.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

using test::RunPendingTasks;

class CssSelectorFragmentAnchorTest : public SimTest {
 public:
  void SetUp() override {
    SimTest::SetUp();

    // Focus handlers aren't run unless the page is focused.
    GetDocument().GetPage()->GetFocusController().SetFocused(true);

    WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  }

  ScrollableArea* LayoutViewport() {
    return GetDocument().View()->LayoutViewport();
  }

  gfx::Rect ViewportRect() {
    return gfx::Rect(LayoutViewport()->VisibleContentRect().size());
  }

  gfx::Rect BoundingRectInFrame(Node& node) {
    return node.GetLayoutObject()->AbsoluteBoundingBoxRect();
  }

  void SimulateClick(int x, int y) {
    WebMouseEvent event(WebInputEvent::Type::kMouseDown, gfx::PointF(x, y),
                        gfx::PointF(x, y), WebPointerProperties::Button::kLeft,
                        0, WebInputEvent::Modifiers::kLeftButtonDown,
                        base::TimeTicks::Now());
    event.SetFrameScale(1);
    WebView().MainFrameWidget()->ProcessInputEventSynchronouslyForTesting(
        WebCoalescedInputEvent(event, ui::LatencyInfo()));
  }

  bool IsVisibleInViewport(Element& element) {
    return ViewportRect().Contains(BoundingRectInFrame(element));
  }
};

// Make sure we find the element and scroll it to the middle of viewport.
TEST_F(CssSelectorFragmentAnchorTest, BasicTest) {
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitAndEnableFeature(
      blink::features::kCssSelectorFragmentAnchor);

  SimRequest main_request(
      "https://example.com/"
      "test.html#:~:selector(type=CssSelector,value=img[src$=\"image.svg\"])",
      "text/html");
  SimRequest image_request("https://example.com/image.svg", "image/svg+xml");

  LoadURL(
      "https://example.com/"
      "test.html#:~:selector(type=CssSelector,value=img[src$=\"image.svg\"])");

  // main frame widget size is 800x600
  main_request.Complete(R"HTML(
      <!DOCTYPE html>
      <body style="margin:0px;">
      <div style="height:600px;">some text</div>
      <img id="image" src="image.svg" style="vertical-align:top;">
      <div style="height:600px;">some other text</div>
      </body>
    )HTML");

  image_request.Complete(R"SVG(
      <svg id="svg" width="200" height="200" xmlns="http://www.w3.org/2000/svg">
         <circle class="path" cx="100" cy="100" r="100" fill="red"/>
      </svg>
    )SVG");

  test::RunPendingTasks();
  Compositor().BeginFrame();

  // +-------------------------------+   <-- 0px
  // | some text (height:600px)
  // |
  // |
  // |
  // |                                   <-- 400px (scroll offset)  | visible
  // |                                                              | height
  // | circle (height:200px)                                        |
  // |                                                              |
  // | some other text (height:600px)                               |
  // |                                                              |
  // |
  // |
  // |
  // |
  // +-------------------------------+
  EXPECT_EQ(ScrollOffset(0, 400), LayoutViewport()->GetScrollOffset())
      << "<img> was not EXACTLY scrolled into the MIDDLE of the viewport "
      << "vertically, viewport's scroll offset: "
      << LayoutViewport()->GetScrollOffset().ToString();

  Element& img = *GetDocument().getElementById("image");
  EXPECT_TRUE(IsVisibleInViewport(img))
      << "<img> Element wasn't scrolled into view, viewport's scroll offset: "
      << LayoutViewport()->GetScrollOffset().ToString();

  EXPECT_EQ(img, *GetDocument().CssTarget());
}

// When more than one CssSelector Fragments are present, scroll the first one
// into view
TEST_F(CssSelectorFragmentAnchorTest, TwoCssSelectorFragmentsScrollToFirst) {
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitAndEnableFeature(
      blink::features::kCssSelectorFragmentAnchor);

  SimRequest main_request(
      "https://example.com/test.html"
      "#:~:selector(type=CssSelector,value=img[src$=\"second.svg\"])"
      "&selector(type=CssSelector,value=img[src$=\"first.svg\"])",
      "text/html");
  SimRequest first_img_request("https://example.com/first.svg",
                               "image/svg+xml");
  SimRequest second_img_request("https://example.com/second.svg",
                                "image/svg+xml");

  LoadURL(
      "https://example.com/test.html"
      "#:~:selector(type=CssSelector,value=img[src$=\"second.svg\"])"
      "&selector(type=CssSelector,value=img[src$=\"first.svg\"])");

  main_request.Complete(R"HTML(
      <!DOCTYPE html>
      <p style="height:1000px;">some text</p>
      <img id="first" src="first.svg">
      <p style="height:1000px;">some other text</p>
      <img id="second" src="second.svg">
      <p style="height:1000px;">yet some more text</p>
    )HTML");

  first_img_request.Complete(R"SVG(
      <svg id="svg" width="50" height="50" xmlns="http://www.w3.org/2000/svg">
         <circle class="path" cx="25" cy="25" r="25" fill="red"/>
      </svg>
    )SVG");

  second_img_request.Complete(R"SVG(
      <svg id="svg" width="50" height="50" xmlns="http://www.w3.org/2000/svg">
         <circle class="path" cx="25" cy="25" r="25" fill="red"/>
      </svg>
    )SVG");

  test::RunPendingTasks();
  Compositor().BeginFrame();

  Element& second = *GetDocument().getElementById("second");

  EXPECT_EQ(second, *GetDocument().CssTarget());
  EXPECT_TRUE(IsVisibleInViewport(second))
      << "second <img> Element wasn't scrolled into view, viewport's scroll "
         "offset: "
      << LayoutViewport()->GetScrollOffset().ToString();
}

// If the first CssSelector Fragment is not found, look for the second one
// and scroll it into view
TEST_F(CssSelectorFragmentAnchorTest, TwoCssSelectorFragmentsFirstNotFound) {
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitAndEnableFeature(
      blink::features::kCssSelectorFragmentAnchor);

  SimRequest main_request(
      "https://example.com/test.html"
      "#:~:selector(type=CssSelector,value=img[src$=\"penguin.svg\"])"
      "&selector(type=CssSelector,value=img[src$=\"first.svg\"])",
      "text/html");
  SimRequest image_request("https://example.com/first.svg", "image/svg+xml");

  LoadURL(
      "https://example.com/test.html"
      "#:~:selector(type=CssSelector,value=img[src$=\"penguin.svg\"])"
      "&selector(type=CssSelector,value=img[src$=\"first.svg\"])");

  main_request.Complete(R"HTML(
      <!DOCTYPE html>
      <p style="height:1000px;">some text</p>
      <img id="first" src="first.svg">
      <p style="height:1000px;">some other text</p>
    )HTML");

  image_request.Complete(R"SVG(
      <svg id="svg" width="50" height="50" xmlns="http://www.w3.org/2000/svg">
         <circle class="path" cx="25" cy="25" r="25" fill="red"/>
      </svg>
    )SVG");

  test::RunPendingTasks();
  Compositor().BeginFrame();

  Element& first = *GetDocument().getElementById("first");

  EXPECT_EQ(first, *GetDocument().CssTarget());
  EXPECT_TRUE(IsVisibleInViewport(first))
      << "<img> Element wasn't scrolled into view, viewport's scroll offset: "
      << LayoutViewport()->GetScrollOffset().ToString();
}

// If both CssSelectorFragment and ElementFragment present,
// prioritize CssSelectorFragment
TEST_F(CssSelectorFragmentAnchorTest,
       PrioritizeCssSelectorFragmentOverElementFragment) {
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitAndEnableFeature(
      blink::features::kCssSelectorFragmentAnchor);

  SimRequest main_request(
      "https://example.com/test.html#element"
      ":~:selector(type=CssSelector,value=img[src$=\"first.svg\"])",
      "text/html");
  SimRequest image_request("https://example.com/first.svg", "image/svg+xml");

  LoadURL(
      "https://example.com/test.html#element"
      ":~:selector(type=CssSelector,value=img[src$=\"first.svg\"])");

  main_request.Complete(R"HTML(
      <!DOCTYPE html>
      <p style="height:1000px;">some text</p>
      <img id="first" src="first.svg">
      <p style="height:1000px;">some other text</p>
      <p id="element" style="height:1000px;">the element!</p>
    )HTML");

  image_request.Complete(R"SVG(
      <svg id="svg" width="50" height="50" xmlns="http://www.w3.org/2000/svg">
         <circle class="path" cx="25" cy="25" r="25" fill="red"/>
      </svg>
    )SVG");

  test::RunPendingTasks();
  Compositor().BeginFrame();

  Element& first = *GetDocument().getElementById("first");

  EXPECT_EQ(first, *GetDocument().CssTarget());
  EXPECT_TRUE(IsVisibleInViewport(first))
      << "<img> Element wasn't scrolled into view, viewport's scroll offset: "
      << LayoutViewport()->GetScrollOffset().ToString();
}

// TODO(crbug/1253707): Enable after fixing!
// Don't scroll into view if attribute selector is not allowed according to spec
// https://github.com/WICG/scroll-to-text-fragment/blob/main/EXTENSIONS.md#proposed-solution
TEST_F(CssSelectorFragmentAnchorTest, DISABLED_CheckCssSelectorRestrictions) {
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitAndEnableFeature(
      blink::features::kCssSelectorFragmentAnchor);

  SimRequest main_request(
      "https://example.com/test.html"
      "#:~:selector(type=CssSelector,value=div[id$=\"first\"])",
      "text/html");

  LoadURL(
      "https://example.com/test.html"
      "#:~:selector(type=CssSelector,value=div[id$=\"first\"])");

  main_request.Complete(R"HTML(
      <!DOCTYPE html>
      <p style="height:1000px;">some text</p>
      <div id="first" style="height:50px;">some other text</p>
      <p style="height:1000px;">another paragraph</p>
    )HTML");

  test::RunPendingTasks();
  Compositor().BeginFrame();

  EXPECT_EQ(ScrollOffset(0, 0), LayoutViewport()->GetScrollOffset())
      << "No scroll should happen, viewport's scroll offset: "
      << LayoutViewport()->GetScrollOffset().ToString();
  EXPECT_EQ(nullptr, *GetDocument().CssTarget());

  EXPECT_FALSE(GetDocument().View()->GetFragmentAnchor());

  EXPECT_EQ(GetDocument().Url(), "https://example.com/test.html");
}

// Make sure fragment is dismissed after user clicks
TEST_F(CssSelectorFragmentAnchorTest, DismissFragmentAfterUserClicks) {
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitAndEnableFeature(
      blink::features::kCssSelectorFragmentAnchor);

  SimRequest main_request(
      "https://example.com/"
      "test.html#:~:selector(type=CssSelector,value=img[src$=\"image.svg\"])",
      "text/html");
  SimRequest image_request("https://example.com/image.svg", "image/svg+xml");

  LoadURL(
      "https://example.com/"
      "test.html#:~:selector(type=CssSelector,value=img[src$=\"image.svg\"])");

  // main frame widget size is 800x600
  main_request.Complete(R"HTML(
      <!DOCTYPE html>
      <body style="margin:0px;">
      <div style="height:600px;">some text</div>
      <img id="image" src="image.svg" style="vertical-align:top;">
      <div style="height:600px;">some other text</div>
      </body>
    )HTML");

  image_request.Complete(R"SVG(
      <svg id="svg" width="200" height="200" xmlns="http://www.w3.org/2000/svg">
         <circle class="path" cx="100" cy="100" r="100" fill="red"/>
      </svg>
    )SVG");

  test::RunPendingTasks();
  Compositor().BeginFrame();

  Element& img = *GetDocument().getElementById("image");
  EXPECT_TRUE(IsVisibleInViewport(img))
      << "<img> Element wasn't scrolled into view, viewport's scroll offset: "
      << LayoutViewport()->GetScrollOffset().ToString();
  EXPECT_EQ(img, *GetDocument().CssTarget());

  SimulateClick(100, 100);

  EXPECT_FALSE(GetDocument().View()->GetFragmentAnchor());

  KURL url = GetDocument()
                 .GetFrame()
                 ->Loader()
                 .GetDocumentLoader()
                 ->GetHistoryItem()
                 ->Url();
  EXPECT_EQ("https://example.com/test.html", url);
}

// Although parsed correctly, the element is not found, hence no scroll happens
// and scroll offset should be zero
TEST_F(CssSelectorFragmentAnchorTest, ParsedCorrectlyButElementNotFound) {
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitAndEnableFeature(
      blink::features::kCssSelectorFragmentAnchor);

  SimRequest main_request(
      "https://example.com/test.html"
      "#:~:selector(type=CssSelector,value=img[src$=\"lorem.svg\"])",
      "text/html");

  LoadURL(
      "https://example.com/test.html"
      "#:~:selector(type=CssSelector,value=img[src$=\"lorem.svg\"])");

  main_request.Complete(R"HTML(
      <!DOCTYPE html>
      <p style="height:1000px;">some text</p>
      <p style="height:1000px;">another paragraph</p>
    )HTML");

  test::RunPendingTasks();
  Compositor().BeginFrame();

  EXPECT_EQ(ScrollOffset(0, 0), LayoutViewport()->GetScrollOffset())
      << "No scroll should happen, viewport's scroll offset: "
      << LayoutViewport()->GetScrollOffset().ToString();
  EXPECT_EQ(nullptr, *GetDocument().CssTarget());

  EXPECT_FALSE(GetDocument().View()->GetFragmentAnchor());
}

// value= part should be encoded/decoded
TEST_F(CssSelectorFragmentAnchorTest, ValuePartHasCommaAndIsEncoded) {
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitAndEnableFeature(
      blink::features::kCssSelectorFragmentAnchor);

  SimRequest main_request(
      "https://example.com/test.html"
      //      "#:~:selector(value=img[src$="cat,dog"],type=CssSelector)",
      "#:~:selector(value=img%5Bsrc%24%3D%22cat%2Cdog%22%5D,type=CssSelector)",
      "text/html");
  SimRequest img_request("https://example.com/cat,dog", "image/svg+xml");

  LoadURL(
      "https://example.com/test.html"
      "#:~:selector(value=img%5Bsrc%24%3D%22cat%2Cdog%22%5D,type=CssSelector)");

  main_request.Complete(R"HTML(
      <!DOCTYPE html>
      <p style="height:1000px;">some text</p>
      <img id="first" src="cat,dog">
      <p style="height:1000px;">some other text</p>
    )HTML");

  img_request.Complete(R"SVG(
      <svg id="svg" width="50" height="50" xmlns="http://www.w3.org/2000/svg">
         <circle class="path" cx="25" cy="25" r="25" fill="red"/>
      </svg>
    )SVG");

  test::RunPendingTasks();
  Compositor().BeginFrame();

  Element& first = *GetDocument().getElementById("first");

  EXPECT_EQ(first, *GetDocument().CssTarget());
  EXPECT_TRUE(IsVisibleInViewport(first))
      << "second <img> Element wasn't scrolled into view, viewport's scroll "
         "offset: "
      << LayoutViewport()->GetScrollOffset().ToString();
}

// What if value= part is not encoded, and it contains a comma,
// Should not crash, and should not scroll to anywhere and no fragment anchor
TEST_F(CssSelectorFragmentAnchorTest, ValuePartHasCommaButIsNotEncoded) {
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitAndEnableFeature(
      blink::features::kCssSelectorFragmentAnchor);

  SimRequest main_request(
      "https://example.com/test.html"
      "#:~:selector(value=img[src$=\"cat,dog\"],type=CssSelector)",
      "text/html");
  SimRequest img_request("https://example.com/cat,dog", "image/svg+xml");

  LoadURL(
      "https://example.com/test.html"
      "#:~:selector(value=img[src$=\"cat,dog\"],type=CssSelector)");

  main_request.Complete(R"HTML(
      <!DOCTYPE html>
      <p style="height:1000px;">some text</p>
      <img id="first" src="cat,dog">
      <p style="height:1000px;">some other text</p>
    )HTML");

  img_request.Complete(R"SVG(
      <svg id="svg" width="50" height="50" xmlns="http://www.w3.org/2000/svg">
         <circle class="path" cx="25" cy="25" r="25" fill="red"/>
      </svg>
    )SVG");

  test::RunPendingTasks();
  Compositor().BeginFrame();

  EXPECT_FALSE(GetDocument().View()->GetFragmentAnchor());
  EXPECT_EQ(nullptr, *GetDocument().CssTarget());
  EXPECT_EQ(ScrollOffset(0, 0), LayoutViewport()->GetScrollOffset())
      << "No scroll should happen, viewport's scroll offset: "
      << LayoutViewport()->GetScrollOffset().ToString();
}

}  // namespace blink
