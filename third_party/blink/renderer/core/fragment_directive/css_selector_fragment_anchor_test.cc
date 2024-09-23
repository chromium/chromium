// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_style_declaration.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
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
    GetDocument().GetPage()->GetFocusController().SetActive(true);
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

  bool IsSelectorFragmentAnchorCreated() {
    return GetDocument().View()->GetFragmentAnchor() &&
           GetDocument()
               .View()
               ->GetFragmentAnchor()
               ->IsSelectorFragmentAnchor();
  }

  const CSSValue* GetComputedValue(const CSSPropertyID& property_id,
                                   const Element& element) {
    return CSSProperty::Get(property_id)
        .CSSValueFromComputedStyle(
            element.ComputedStyleRef(), nullptr /* layout_object */,
            false /* allow_visited_style */, CSSValuePhase::kComputedValue);
  }

  bool IsElementOutlined(const Element& element) {
    const CSSValue* value =
        GetComputedValue(CSSPropertyID::kOutlineWidth, element);
    return "0px" != value->CssText();
  }

  const String CircleSVG() {
    return R"SVG(
      <svg id="svg" width="200" height="200" xmlns="http://www.w3.org/2000/svg">
         <circle class="path" cx="100" cy="100" r="100" fill="red"/>
      </svg>
    )SVG";
  }
};

// Make sure we find the element and set it as the CSS target.
TEST_F(CssSelectorFragmentAnchorTest, BasicTest) {
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
      <img id="image" src="image.svg">
    )HTML");

  image_request.Complete(CircleSVG());

  test::RunPendingTasks();
  Compositor().BeginFrame();

  Element& img = *GetDocument().getElementById(AtomicString("image"));

  EXPECT_EQ(img, *GetDocument().CssTarget());
  EXPECT_EQ(true, IsSelectorFragmentAnchorCreated());
}

// When more than one CssSelector Fragments are present, set the first one as
// the CSS target (which will be outlined accordingly)
TEST_F(CssSelectorFragmentAnchorTest, TwoCssSelectorFragmentsOutlineFirst) {
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
      <img id="first" src="first.svg">
      <img id="second" src="second.svg">
    )HTML");

  first_img_request.Complete(CircleSVG());
  second_img_request.Complete(CircleSVG());

  test::RunPendingTasks();
  Compositor().BeginFrame();

  Element& second = *GetDocument().getElementById(AtomicString("second"));

  EXPECT_EQ(second, *GetDocument().CssTarget());
  EXPECT_EQ(true, IsSelectorFragmentAnchorCreated());
}

// If the first CssSelector Fragment is not found, look for the second one
// and set that as the CSS target
TEST_F(CssSelectorFragmentAnchorTest, TwoCssSelectorFragmentsFirstNotFound) {
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
      <img id="first" src="first.svg">
    )HTML");

  image_request.Complete(CircleSVG());

  test::RunPendingTasks();
  Compositor().BeginFrame();

  Element& first = *GetDocument().getElementById(AtomicString("first"));

  EXPECT_EQ(first, *GetDocument().CssTarget());
  EXPECT_EQ(true, IsSelectorFragmentAnchorCreated());
}

// If both CssSelectorFragment and ElementFragment present,
// prioritize CssSelectorFragment
TEST_F(CssSelectorFragmentAnchorTest,
       PrioritizeCssSelectorFragmentOverElementFragment) {
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
      <p id="element">the element!</p>
      <img id="first" src="first.svg">
    )HTML");

  image_request.Complete(CircleSVG());

  test::RunPendingTasks();
  Compositor().BeginFrame();

  Element& first = *GetDocument().getElementById(AtomicString("first"));

  EXPECT_EQ(first, *GetDocument().CssTarget());
  EXPECT_EQ(true, IsSelectorFragmentAnchorCreated());
}

// TODO(crbug/1253707): Enable after fixing!
// Don't do anything if attribute selector is not allowed according to spec
// https://github.com/WICG/scroll-to-text-fragment/blob/main/EXTENSIONS.md#proposed-solution
TEST_F(CssSelectorFragmentAnchorTest, DISABLED_CheckCssSelectorRestrictions) {
  SimRequest main_request(
      "https://example.com/test.html"
      "#:~:selector(type=CssSelector,value=div[id$=\"first\"])",
      "text/html");

  LoadURL(
      "https://example.com/test.html"
      "#:~:selector(type=CssSelector,value=div[id$=\"first\"])");

  main_request.Complete(R"HTML(
      <!DOCTYPE html>
      <div id="first">some other text</p>
    )HTML");

  test::RunPendingTasks();
  Compositor().BeginFrame();

  EXPECT_EQ(nullptr, *GetDocument().CssTarget());
  EXPECT_EQ(nullptr, GetDocument().View()->GetFragmentAnchor());
  EXPECT_EQ("https://example.com/test.html", GetDocument().Url());
}

// Make sure fragment is not dismissed after user clicks
TEST_F(CssSelectorFragmentAnchorTest, FragmentStaysAfterUserClicks) {
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
      <img id="image" src="image.svg">
    )HTML");

  image_request.Complete(CircleSVG());

  test::RunPendingTasks();
  Compositor().BeginFrame();

  KURL expected_url = GetDocument()
                          .GetFrame()
                          ->Loader()
                          .GetDocumentLoader()
                          ->GetHistoryItem()
                          ->Url();

  Element& img = *GetDocument().getElementById(AtomicString("image"));
  EXPECT_EQ(img, *GetDocument().CssTarget());
  EXPECT_EQ(true, IsSelectorFragmentAnchorCreated());

  SimulateClick(100, 100);

  EXPECT_TRUE(GetDocument().View()->GetFragmentAnchor());

  KURL url = GetDocument()
                 .GetFrame()
                 ->Loader()
                 .GetDocumentLoader()
                 ->GetHistoryItem()
                 ->Url();

  EXPECT_EQ(expected_url, url);
}

// Although parsed correctly, the element is not found, hence no CSS target
// should be set
TEST_F(CssSelectorFragmentAnchorTest, ParsedCorrectlyButElementNotFound) {
  SimRequest main_request(
      "https://example.com/test.html"
      "#:~:selector(type=CssSelector,value=img[src$=\"lorem.svg\"])",
      "text/html");

  LoadURL(
      "https://example.com/test.html"
      "#:~:selector(type=CssSelector,value=img[src$=\"lorem.svg\"])");

  main_request.Complete(R"HTML(
      <!DOCTYPE html>
      <p>some text</p>
    )HTML");

  test::RunPendingTasks();
  Compositor().BeginFrame();

  EXPECT_EQ(nullptr, GetDocument().CssTarget());
  EXPECT_EQ(nullptr, GetDocument().View()->GetFragmentAnchor());
}

// value= part should be encoded/decoded
TEST_F(CssSelectorFragmentAnchorTest, ValuePartHasCommaAndIsEncoded) {
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
      <img id="first" src="cat,dog">
    )HTML");

  img_request.Complete(CircleSVG());

  test::RunPendingTasks();
  Compositor().BeginFrame();

  Element& first = *GetDocument().getElementById(AtomicString("first"));

  EXPECT_EQ(first, *GetDocument().CssTarget());
  EXPECT_EQ(true, IsSelectorFragmentAnchorCreated());
}

// What if value= part is not encoded, and it contains a comma,
// Should not crash and no CSS target should be set
TEST_F(CssSelectorFragmentAnchorTest, ValuePartHasCommaButIsNotEncoded) {
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
      <img id="first" src="cat,dog">
    )HTML");

  img_request.Complete(CircleSVG());

  test::RunPendingTasks();
  Compositor().BeginFrame();

  EXPECT_EQ(nullptr, GetDocument().CssTarget());
  EXPECT_EQ(nullptr, GetDocument().View()->GetFragmentAnchor());
}

TEST_F(CssSelectorFragmentAnchorTest,
       TargetElementIsNotHighlightedWithElementFragment) {
  SimRequest main_request("https://example.com/test.html#image", "text/html");
  SimRequest image_request("https://example.com/image.svg", "image/svg+xml");

  LoadURL("https://example.com/test.html#image");

  // main frame widget size is 800x600
  main_request.Complete(R"HTML(
      <!DOCTYPE html>
      <img id="image" src="image.svg">
    )HTML");

  image_request.Complete(CircleSVG());

  test::RunPendingTasks();
  Compositor().BeginFrame();

  Element& img = *GetDocument().getElementById(AtomicString("image"));

  EXPECT_FALSE(IsElementOutlined(img));
  EXPECT_EQ(img, *GetDocument().CssTarget());
}

TEST_F(CssSelectorFragmentAnchorTest,
       TargetElementIsNotHighlightedWithTextFragment) {
  SimRequest main_request("https://example.com/test.html#:~:text=some other",
                          "text/html");

  LoadURL("https://example.com/test.html#:~:text=some other");

  // main frame widget size is 800x600
  main_request.Complete(R"HTML(
      <!DOCTYPE html>
      <div id="element">some other text</div>
    )HTML");

  test::RunPendingTasks();

  Compositor().BeginFrame();

  Element& element = *GetDocument().getElementById(AtomicString("element"));

  EXPECT_FALSE(IsElementOutlined(element));
  EXPECT_EQ(element, *GetDocument().CssTarget());
}

// Simulate an anchor link navigation and check that the style is removed.
TEST_F(CssSelectorFragmentAnchorTest, SelectorFragmentTargetOutline) {
  SimRequest main_request(
      "https://example.com/test.html"
      "#:~:selector(type=CssSelector,value=img[src=\"image.svg\"])",
      "text/html");
  SimRequest image_request("https://example.com/image.svg", "image/svg+xml");

  LoadURL(
      "https://example.com/test.html"
      "#:~:selector(type=CssSelector,value=img[src=\"image.svg\"])");

  // main frame widget size is 800x600
  main_request.Complete(R"HTML(
      <!DOCTYPE html>
      <a id="element" href="#paragraph">Go to paragraph</a>
      <img id="image" src="image.svg">
      <p id="paragraph"></p>
    )HTML");

  image_request.Complete(CircleSVG());

  test::RunPendingTasks();
  Compositor().BeginFrame();

  Element& paragraph = *GetDocument().getElementById(AtomicString("paragraph"));
  Element& img = *GetDocument().getElementById(AtomicString("image"));

  EXPECT_TRUE(IsElementOutlined(img));
  EXPECT_EQ(img, *GetDocument().CssTarget());
  EXPECT_EQ(true, IsSelectorFragmentAnchorCreated());

  auto* anchor = To<HTMLAnchorElement>(
      GetDocument().getElementById(AtomicString("element")));
  anchor->click();

  EXPECT_FALSE(IsElementOutlined(img));
  EXPECT_EQ(paragraph, *GetDocument().CssTarget());
  EXPECT_EQ("https://example.com/test.html#paragraph", GetDocument().Url());
}

}  // namespace blink
