// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_area_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class HTMLAreaElementTest : public PageTestBase {};

// The HTML rendering spec's expected UA defaults list <area> among the hidden
// elements.
// https://html.spec.whatwg.org/multipage/rendering.html#hidden-elements
TEST_F(HTMLAreaElementTest, AreaIsDisplayNone) {
  SetBodyInnerHTML(R"HTML(
    <img usemap="#m" width="500" height="500">
    <map name="m"><area id="area" shape="rect" coords="0,0,500,500"></map>
  )HTML");

  auto* area = GetDocument().getElementById(AtomicString("area"));
  ASSERT_TRUE(area);
  EXPECT_EQ(nullptr, area->GetLayoutObject());
  const ComputedStyle* style = area->GetComputedStyle();
  ASSERT_TRUE(style);
  EXPECT_FALSE(style->IsEnsuredInDisplayNone());
  EXPECT_EQ(EDisplay::kNone, style->Display());
}

TEST_F(HTMLAreaElementTest, FocusStyleUpdatesWithoutLayoutObject) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #area { outline: none }
      #area:focus { outline: 2px solid green }
    </style>
    <img usemap="#m" width="500" height="500">
    <map name="m">
      <area id="area" href="#target" shape="rect" coords="0,0,500,500">
    </map>
  )HTML");

  auto* area = GetDocument().getElementById(AtomicString("area"));
  ASSERT_TRUE(area);
  ASSERT_TRUE(area->GetComputedStyle());
  EXPECT_FALSE(area->GetComputedStyle()->HasOutline());

  GetFocusController().SetActive(true);
  GetFocusController().SetFocused(true);
  GetDocument().SetFocusedElement(
      area, FocusParams(SelectionBehaviorOnFocus::kNone,
                        mojom::blink::FocusType::kScript, nullptr));
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(area, GetDocument().FocusedElement());
  ASSERT_TRUE(area->GetComputedStyle());
  EXPECT_TRUE(area->GetComputedStyle()->HasOutline());

  GetDocument().ClearFocusedElement();
  UpdateAllLifecyclePhasesForTest();
  ASSERT_TRUE(area->GetComputedStyle());
  EXPECT_FALSE(area->GetComputedStyle()->HasOutline());
}

// A focused <area> keeps its ComputedStyle for as long as style recalc reaches
// it, but that stops once it is inside a display:none subtree. Page focus
// changes must not assume that a focused <area> has a ComputedStyle.
TEST_F(HTMLAreaElementTest, PageFocusChangeWithDisplayNoneAncestor) {
  SetBodyInnerHTML(R"HTML(
    <img usemap="#m" width="500" height="500">
    <div id="wrapper">
      <map name="m">
        <area id="area" href="#target" shape="rect" coords="0,0,500,500">
      </map>
    </div>
  )HTML");

  auto* area = GetDocument().getElementById(AtomicString("area"));
  ASSERT_TRUE(area);

  GetFocusController().SetActive(true);
  GetFocusController().SetFocused(true);
  GetDocument().SetFocusedElement(
      area, FocusParams(SelectionBehaviorOnFocus::kNone,
                        mojom::blink::FocusType::kScript, nullptr));
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(area, GetDocument().FocusedElement());

  GetDocument()
      .getElementById(AtomicString("wrapper"))
      ->setAttribute(html_names::kStyleAttr, AtomicString("display:none"));
  UpdateAllLifecyclePhasesForTest();

  // Should not crash.
  GetFocusController().SetFocused(false);
  GetFocusController().SetFocused(true);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(area, GetDocument().FocusedElement());
}

// crbug.com/41140071: an <area> in a <map> that is not a sibling of the image
// used to report the position of the empty inline box generated for the
// <area>, which is unrelated to the image map geometry. Since <area> has no
// box, all offset* values must be 0 and offsetParent must be null.
TEST_F(HTMLAreaElementTest, OffsetsAreZeroWhenMapIsNotNextToImage) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0 }</style>
    <div style="height: 500px; padding: 20px 0 0 30px">
      <img id="img" usemap="#m" width="500" height="500" style="display:block">
    </div>
    <map name="m"><area id="area" shape="rect" coords="0,0,500,500"></map>
  )HTML");

  auto* area = GetDocument().getElementById(AtomicString("area"));
  auto* img = GetDocument().getElementById(AtomicString("img"));
  ASSERT_TRUE(area);
  ASSERT_TRUE(img);

  // The <area>'s zero offsets must not expose the image map's geometry.
  EXPECT_EQ(30, img->OffsetLeft());
  EXPECT_EQ(20, img->OffsetTop());

  EXPECT_EQ(0, area->OffsetLeft());
  EXPECT_EQ(0, area->OffsetTop());
  EXPECT_EQ(0, area->OffsetWidth());
  EXPECT_EQ(0, area->OffsetHeight());
  EXPECT_EQ(nullptr, area->OffsetParent());

  DOMRect* rect = area->GetBoundingClientRect();
  EXPECT_EQ(0, rect->width());
  EXPECT_EQ(0, rect->height());
}

}  // namespace blink
