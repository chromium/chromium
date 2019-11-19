// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/snap_coordinator.h"

#include <gtest/gtest.h>
#include <memory>

#include "cc/input/scroll_snap_data.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/scoped_mock_overlay_scrollbars.h"

namespace blink {

using html_names::kStyleAttr;

class SnapCoordinatorTest : public testing::Test,
                            public ScopedMockOverlayScrollbars {
 protected:
  void SetUp() override {
    page_holder_ = std::make_unique<DummyPageHolder>(
        IntSize(), nullptr, nullptr, base::BindOnce([](Settings& settings) {
          settings.SetAcceleratedCompositingEnabled(true);
        }));

    GetDocument().View()->SetParentVisible(true);
    GetDocument().View()->SetSelfVisible(true);

    SetHTML(R"HTML(
      <style>
          #snap-container {
              height: 1000px;
              width: 1000px;
              overflow: scroll;
              scroll-snap-type: both mandatory;
          }
          #snap-element-fixed-position {
               position: fixed;
          }
      </style>
      <body>
        <div id='snap-container'>
          <div id='snap-element'></div>
          <div id='intermediate'>
             <div id='nested-snap-element'></div>
          </div>
          <div id='snap-element-fixed-position'></div>
          <div style='width:2000px; height:2000px;'></div>
        </div>
      </body>
    )HTML");
    UpdateAllLifecyclePhasesForTest();
  }

  void TearDown() override { page_holder_ = nullptr; }

  Document& GetDocument() { return page_holder_->GetDocument(); }

  void SetHTML(const char* html_content) {
    GetDocument().documentElement()->SetInnerHTMLFromString(html_content);
  }

  Element& SnapContainer() {
    return *GetDocument().getElementById("snap-container");
  }

  unsigned SizeOfSnapAreas(const ContainerNode& node) {
    if (node.GetLayoutBox()->SnapAreas())
      return node.GetLayoutBox()->SnapAreas()->size();
    return 0U;
  }

  void SetUpSingleSnapArea() {
    SetHTML(R"HTML(
      <style>
      #scroller {
        width: 140px;
        height: 160px;
        padding: 0px;
        scroll-snap-type: both mandatory;
        scroll-padding: 10px;
        overflow: scroll;
      }
      #container {
        margin: 0px;
        padding: 0px;
        width: 500px;
        height: 500px;
      }
      #area {
        position: relative;
        top: 200px;
        left: 200px;
        width: 100px;
        height: 100px;
        scroll-margin: 8px;
      }
      </style>
      <div id='scroller'>
        <div id='container'>
          <div id="area"></div>
        </div>
      </div>
      )HTML");
    UpdateAllLifecyclePhasesForTest();
  }

  void UpdateAllLifecyclePhasesForTest() {
    GetDocument().View()->UpdateAllLifecyclePhases(
        DocumentLifecycle::LifecycleUpdateReason::kTest);
    GetDocument().View()->RunPostLifecycleSteps();
  }

  const cc::SnapContainerData* GetSnapContainerData(LayoutBox& layout_box) {
    if (layout_box.GetScrollableArea()) {
      return layout_box.GetScrollableArea()->GetSnapContainerData();
    }
    return nullptr;
  }

  std::unique_ptr<DummyPageHolder> page_holder_;
};

TEST_F(SnapCoordinatorTest, SimpleSnapElement) {
  Element& snap_element = *GetDocument().getElementById("snap-element");
  snap_element.setAttribute(kStyleAttr, "scroll-snap-align: start;");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(1U, SizeOfSnapAreas(SnapContainer()));
}

TEST_F(SnapCoordinatorTest, NestedSnapElement) {
  Element& snap_element = *GetDocument().getElementById("nested-snap-element");
  snap_element.setAttribute(kStyleAttr, "scroll-snap-align: start;");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(1U, SizeOfSnapAreas(SnapContainer()));
}

TEST_F(SnapCoordinatorTest, ModifySnapElement) {
  Element& snap_element = *GetDocument().getElementById("snap-element");
  snap_element.setAttribute(kStyleAttr, "scroll-snap-align: start;");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(1U, SizeOfSnapAreas(SnapContainer()));

  snap_element.setAttribute(kStyleAttr, "scroll-snap-align: end;");

  // Set scrollable area will set paint invalidation while scroll, will crash
  // if snap-element not set needs update.
  SnapContainer().GetLayoutBox()->SetShouldDoFullPaintInvalidation();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(1U, SizeOfSnapAreas(SnapContainer()));
}

TEST_F(SnapCoordinatorTest, NestedSnapElementCaptured) {
  Element& snap_element = *GetDocument().getElementById("nested-snap-element");
  snap_element.setAttribute(kStyleAttr, "scroll-snap-align: start;");

  Element* intermediate = GetDocument().getElementById("intermediate");
  intermediate->setAttribute(kStyleAttr, "overflow: scroll;");

  UpdateAllLifecyclePhasesForTest();

  // Intermediate scroller captures nested snap elements first so ancestor
  // does not get them.
  EXPECT_EQ(0U, SizeOfSnapAreas(SnapContainer()));
  EXPECT_EQ(1U, SizeOfSnapAreas(*intermediate));
}

TEST_F(SnapCoordinatorTest, PositionFixedSnapElement) {
  Element& snap_element =
      *GetDocument().getElementById("snap-element-fixed-position");
  snap_element.setAttribute(kStyleAttr, "scroll-snap-align: start;");
  UpdateAllLifecyclePhasesForTest();

  // Position fixed elements are contained in document and not its immediate
  // ancestor scroller. They cannot be a valid snap destination so they should
  // not contribute snap points to their immediate snap container or document
  // See: https://lists.w3.org/Archives/Public/www-style/2015Jun/0376.html
  EXPECT_EQ(0U, SizeOfSnapAreas(SnapContainer()));

  Element* body = GetDocument().ViewportDefiningElement();
  EXPECT_EQ(0U, SizeOfSnapAreas(*body));
}

TEST_F(SnapCoordinatorTest, UpdateStyleForSnapElement) {
  Element& snap_element = *GetDocument().getElementById("snap-element");
  snap_element.setAttribute(kStyleAttr, "scroll-snap-align: start;");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(1U, SizeOfSnapAreas(SnapContainer()));

  snap_element.remove();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(0U, SizeOfSnapAreas(SnapContainer()));

  // Add a new snap element
  Element& container = *GetDocument().getElementById("snap-container");
  container.SetInnerHTMLFromString(R"HTML(
    <div style='scroll-snap-align: start;'>
        <div style='width:2000px; height:2000px;'></div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(1U, SizeOfSnapAreas(SnapContainer()));
}

TEST_F(SnapCoordinatorTest, ViewpoertScrollSnapStyleComesFromDocumentElement) {
  SetHTML(R"HTML(
    <style>
    :root {
      scroll-snap-type: both mandatory;
    }
    body {
     scroll-snap-type: none;
    }
    </style>
    <body>
    </body>
    )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* body = GetDocument().body();
  EXPECT_EQ(body, GetDocument().ViewportDefiningElement());

  const cc::SnapContainerData* viewport_data =
      GetSnapContainerData(*GetDocument().GetLayoutView());
  EXPECT_TRUE(viewport_data);
  EXPECT_EQ(viewport_data->scroll_snap_type(),
            cc::ScrollSnapType(false, cc::SnapAxis::kBoth,
                               cc::SnapStrictness::kMandatory));

  const cc::SnapContainerData* body_data =
      GetSnapContainerData(*body->GetLayoutBox());

  EXPECT_FALSE(body_data);
}

TEST_F(SnapCoordinatorTest, LayoutViewCapturesWhenBodyElementViewportDefining) {
  SetHTML(R"HTML(
    <style>
    body {
        overflow: scroll;
        scroll-snap-type: both mandatory;
        height: 1000px;
        width: 1000px;
        margin: 5px;
    }
    </style>
    <body>
        <div id='snap-element' style='scroll-snap-align: start;></div>
        <div id='intermediate'>
            <div id='nested-snap-element'
                style='scroll-snap-align: start;'></div>
        </div>
        <div style='width:2000px; height:2000px;'></div>
    </body>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  // Sanity check that body is the viewport defining element
  EXPECT_EQ(GetDocument().body(), GetDocument().ViewportDefiningElement());

  // When body is viewport defining and overflows then any snap points on the
  // body element will be captured by layout view as the snap container.
  EXPECT_EQ(2U, SizeOfSnapAreas(GetDocument()));
  EXPECT_EQ(0U, SizeOfSnapAreas(*(GetDocument().body())));
  EXPECT_EQ(0U, SizeOfSnapAreas(*(GetDocument().documentElement())));
}

TEST_F(SnapCoordinatorTest,
       LayoutViewCapturesWhenDocumentElementViewportDefining) {
  SetHTML(R"HTML(
    <style>
    :root {
        overflow: scroll;
        scroll-snap-type: both mandatory;
        height: 500px;
        width: 500px;
    }
    body {
        margin: 5px;
    }
    </style>
    <html>
       <body>
           <div id='snap-element' style='scroll-snap-align: start;></div>
           <div id='intermediate'>
             <div id='nested-snap-element'
                 style='scroll-snap-align: start;'></div>
          </div>
          <div style='width:2000px; height:2000px;'></div>
       </body>
    </html>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  // Sanity check that document element is the viewport defining element
  EXPECT_EQ(GetDocument().documentElement(),
            GetDocument().ViewportDefiningElement());

  // When document is viewport defining and overflows then any snap points on
  // the document element will be captured by layout view as the snap
  // container.
  EXPECT_EQ(2U, SizeOfSnapAreas(GetDocument()));
  EXPECT_EQ(0U, SizeOfSnapAreas(*(GetDocument().body())));
  EXPECT_EQ(0U, SizeOfSnapAreas(*(GetDocument().documentElement())));
}

TEST_F(SnapCoordinatorTest,
       BodyCapturesWhenBodyOverflowAndDocumentElementViewportDefining) {
  SetHTML(R"HTML(
    <style>
    :root {
        overflow: scroll;
        scroll-snap-type: both mandatory;
        height: 500px;
        width: 500px;
    }
    body {
        overflow: scroll;
        scroll-snap-type: both mandatory;
        height: 1000px;
        width: 1000px;
        margin: 5px;
    }
    </style>
    <html>
       <body style='overflow: scroll; scroll-snap-type: both mandatory;
    height:1000px; width:1000px;'>
           <div id='snap-element' style='scroll-snap-align: start;></div>
           <div id='intermediate'>
             <div id='nested-snap-element'
                 style='scroll-snap-align: start;'></div>
          </div>
          <div style='width:2000px; height:2000px;'></div>
       </body>
    </html>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  // Sanity check that document element is the viewport defining element
  EXPECT_EQ(GetDocument().documentElement(),
            GetDocument().ViewportDefiningElement());

  // When body and document elements are both scrollable then body element
  // should capture snap points defined on it as opposed to layout view.
  Element& body = *GetDocument().body();
  EXPECT_EQ(2U, SizeOfSnapAreas(body));
  EXPECT_EQ(0U, SizeOfSnapAreas(*GetDocument().documentElement()));
  EXPECT_EQ(0U, SizeOfSnapAreas(GetDocument()));
}

#define EXPECT_EQ_CONTAINER(expected, actual)                          \
  {                                                                    \
    EXPECT_EQ(expected.max_position(), actual.max_position());         \
    EXPECT_EQ(expected.scroll_snap_type(), actual.scroll_snap_type()); \
    EXPECT_EQ(expected.proximity_range(), actual.proximity_range());   \
    EXPECT_EQ(expected.size(), actual.size());                         \
    EXPECT_EQ(expected.rect(), actual.rect());                         \
  }

#define EXPECT_EQ_AREA(expected, actual)                             \
  {                                                                  \
    EXPECT_EQ(expected.scroll_snap_align, actual.scroll_snap_align); \
    EXPECT_EQ(expected.rect, actual.rect);                           \
    EXPECT_EQ(expected.must_snap, actual.must_snap);                 \
  }

// The following tests check the SnapContainerData and SnapAreaData are
// correctly calculated.
TEST_F(SnapCoordinatorTest, SnapDataCalculation) {
  SetUpSingleSnapArea();
  Element* scroller_element = GetDocument().getElementById("scroller");
  ScrollableArea* scrollable_area =
      scroller_element->GetLayoutBox()->GetScrollableArea();
  Element* area_element = GetDocument().getElementById("area");
  area_element->setAttribute(kStyleAttr, "scroll-snap-align: start;");
  UpdateAllLifecyclePhasesForTest();

  const cc::SnapContainerData* data =
      GetSnapContainerData(*scroller_element->GetLayoutBox());
  EXPECT_TRUE(data);
  cc::SnapContainerData actual_container = *data;
  FloatPoint max_position = scrollable_area->ScrollOffsetToPosition(
      scrollable_area->MaximumScrollOffset());

  double width = scroller_element->clientWidth();
  double height = scroller_element->clientHeight();
  cc::SnapContainerData expected_container(
      cc::ScrollSnapType(false, cc::SnapAxis::kBoth,
                         cc::SnapStrictness::kMandatory),
      gfx::RectF(10, 10, width - 20, height - 20),
      gfx::ScrollOffset(max_position.X(), max_position.Y()));
  cc::SnapAreaData expected_area(cc::ScrollSnapAlign(cc::SnapAlignment::kStart),
                                 gfx::RectF(192, 192, 116, 116), false,
                                 cc::ElementId(10));
  expected_container.AddSnapAreaData(expected_area);

  EXPECT_EQ_CONTAINER(expected_container, actual_container);
  EXPECT_EQ_AREA(expected_area, actual_container.at(0));
}

TEST_F(SnapCoordinatorTest, ScrolledSnapDataCalculation) {
  SetUpSingleSnapArea();
  Element* scroller_element = GetDocument().getElementById("scroller");
  ScrollableArea* scrollable_area =
      scroller_element->GetLayoutBox()->GetScrollableArea();
  scroller_element->scrollBy(20, 20);
  EXPECT_EQ(FloatPoint(20, 20), scrollable_area->ScrollPosition());
  Element* area_element = GetDocument().getElementById("area");
  area_element->setAttribute(kStyleAttr, "scroll-snap-align: start;");
  UpdateAllLifecyclePhasesForTest();

  const cc::SnapContainerData* data =
      GetSnapContainerData(*scroller_element->GetLayoutBox());
  EXPECT_TRUE(data);
  cc::SnapContainerData actual_container = *data;
  FloatPoint max_position = scrollable_area->ScrollOffsetToPosition(
      scrollable_area->MaximumScrollOffset());

  double width = scroller_element->clientWidth();
  double height = scroller_element->clientHeight();
  cc::SnapContainerData expected_container(
      cc::ScrollSnapType(false, cc::SnapAxis::kBoth,
                         cc::SnapStrictness::kMandatory),
      gfx::RectF(10, 10, width - 20, height - 20),
      gfx::ScrollOffset(max_position.X(), max_position.Y()));
  cc::SnapAreaData expected_area(cc::ScrollSnapAlign(cc::SnapAlignment::kStart),
                                 gfx::RectF(192, 192, 116, 116), false,
                                 cc::ElementId(10));
  expected_container.AddSnapAreaData(expected_area);

  EXPECT_EQ_CONTAINER(expected_container, actual_container);
  EXPECT_EQ_AREA(expected_area, actual_container.at(0));
}

TEST_F(SnapCoordinatorTest, ScrolledSnapDataCalculationOnViewport) {
  SetHTML(R"HTML(
    <style>
    :root {
      scroll-snap-type: both mandatory;
    }
    body {
      margin: 0px;
      overflow: scroll;
    }
    #container {
      width: 1000px;
      height: 1000px;
    }
    #area {
      position: relative;
      top: 200px;
      left: 200px;
      width: 100px;
      height: 100px;
    }
    </style>
    <div id='container'>
    <div id="area"></div>
    </div>
    )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* body = GetDocument().body();
  EXPECT_EQ(body, GetDocument().ViewportDefiningElement());
  ScrollableArea* scrollable_area = GetDocument().View()->LayoutViewport();
  body->scrollBy(20, 20);
  EXPECT_EQ(FloatPoint(20, 20), scrollable_area->ScrollPosition());
  Element* area_element = GetDocument().getElementById("area");
  area_element->setAttribute(kStyleAttr, "scroll-snap-align: start;");
  UpdateAllLifecyclePhasesForTest();
  const cc::SnapContainerData* data =
      GetSnapContainerData(*GetDocument().GetLayoutView());
  EXPECT_TRUE(data);
  cc::SnapContainerData actual_container = *data;

  FloatPoint max_position = scrollable_area->ScrollOffsetToPosition(
      scrollable_area->MaximumScrollOffset());

  double width = body->clientWidth();
  double height = body->clientHeight();
  cc::SnapContainerData expected_container(
      cc::ScrollSnapType(false, cc::SnapAxis::kBoth,
                         cc::SnapStrictness::kMandatory),
      gfx::RectF(0, 0, width, height),
      gfx::ScrollOffset(max_position.X(), max_position.Y()));

  cc::SnapAreaData expected_area(cc::ScrollSnapAlign(cc::SnapAlignment::kStart),
                                 gfx::RectF(200, 200, 100, 100), false,
                                 cc::ElementId(10));
  expected_container.AddSnapAreaData(expected_area);

  EXPECT_EQ_CONTAINER(expected_container, actual_container);
  EXPECT_EQ_AREA(expected_area, actual_container.at(0));
}

TEST_F(SnapCoordinatorTest, SnapDataCalculationWithBoxModel) {
  SetUpSingleSnapArea();
  Element* area_element = GetDocument().getElementById("area");
  area_element->setAttribute(kStyleAttr,
                             "scroll-snap-align: start; margin: 2px; border: "
                             "9px solid; padding: 5px;");
  Element* scroller_element = GetDocument().getElementById("scroller");
  scroller_element->setAttribute(
      kStyleAttr, "margin: 3px; border: 10px solid; padding: 4px;");
  UpdateAllLifecyclePhasesForTest();
  const cc::SnapContainerData* data =
      GetSnapContainerData(*scroller_element->GetLayoutBox());
  EXPECT_TRUE(data);
  cc::SnapContainerData actual_container = *data;

  ScrollableArea* scrollable_area =
      scroller_element->GetLayoutBox()->GetScrollableArea();
  FloatPoint max_position = scrollable_area->ScrollOffsetToPosition(
      scrollable_area->MaximumScrollOffset());

  double width = scroller_element->clientWidth();
  double height = scroller_element->clientHeight();

  // rect.x = rect.y = scroller.border + scroller.scroll-padding
  cc::SnapContainerData expected_container(
      cc::ScrollSnapType(false, cc::SnapAxis::kBoth,
                         cc::SnapStrictness::kMandatory),
      gfx::RectF(20, 20, width - 20, height - 20),
      gfx::ScrollOffset(max_position.X(), max_position.Y()));
  // rect.x = scroller.border + scroller.padding + area.left + area.margin
  //          - area.scroll-margin
  // rect.y = scroller.border + scroller.padding + area.top + area.margin
  //          - area.scroll-margin
  // rect.width = area.width +
  //              2 * (area.padding + area.border + area.scroll-margin)
  // rect.height = area.height +
  //               2 * (area.padding + area.border + area.scroll-margin)
  cc::SnapAreaData expected_area(cc::ScrollSnapAlign(cc::SnapAlignment::kStart),
                                 gfx::RectF(208, 208, 144, 144), false,
                                 cc::ElementId(10));
  expected_container.AddSnapAreaData(expected_area);

  EXPECT_EQ_CONTAINER(expected_container, actual_container);
  EXPECT_EQ_AREA(expected_area, actual_container.at(0));
}

TEST_F(SnapCoordinatorTest, NegativeMarginSnapDataCalculation) {
  SetUpSingleSnapArea();
  Element* area_element = GetDocument().getElementById("area");
  area_element->setAttribute(kStyleAttr,
                             "scroll-snap-align: start; scroll-margin: -8px;");
  UpdateAllLifecyclePhasesForTest();
  Element* scroller_element = GetDocument().getElementById("scroller");
  const cc::SnapContainerData* data =
      GetSnapContainerData(*scroller_element->GetLayoutBox());
  EXPECT_TRUE(data);
  cc::SnapContainerData actual_container = *data;

  ScrollableArea* scrollable_area =
      scroller_element->GetLayoutBox()->GetScrollableArea();
  FloatPoint max_position = scrollable_area->ScrollOffsetToPosition(
      scrollable_area->MaximumScrollOffset());

  double width = scroller_element->clientWidth();
  double height = scroller_element->clientHeight();

  cc::SnapContainerData expected_container(
      cc::ScrollSnapType(false, cc::SnapAxis::kBoth,
                         cc::SnapStrictness::kMandatory),
      gfx::RectF(10, 10, width - 20, height - 20),
      gfx::ScrollOffset(max_position.X(), max_position.Y()));
  cc::SnapAreaData expected_area(cc::ScrollSnapAlign(cc::SnapAlignment::kStart),
                                 gfx::RectF(208, 208, 84, 84), false,
                                 cc::ElementId(10));
  expected_container.AddSnapAreaData(expected_area);

  EXPECT_EQ_CONTAINER(expected_container, actual_container);
  EXPECT_EQ_AREA(expected_area, actual_container.at(0));
}

TEST_F(SnapCoordinatorTest, AsymmetricalSnapDataCalculation) {
  SetUpSingleSnapArea();
  Element* area_element = GetDocument().getElementById("area");
  area_element->setAttribute(kStyleAttr,
                             R"HTML(
        scroll-snap-align: center;
        scroll-margin-top: 2px;
        scroll-margin-right: 4px;
        scroll-margin-bottom: 6px;
        scroll-margin-left: 8px;
      )HTML");
  Element* scroller_element = GetDocument().getElementById("scroller");
  scroller_element->setAttribute(kStyleAttr,
                                 R"HTML(
        scroll-padding-top: 10px;
        scroll-padding-right: 12px;
        scroll-padding-bottom: 14px;
        scroll-padding-left: 16px;
      )HTML");
  UpdateAllLifecyclePhasesForTest();
  const cc::SnapContainerData* data =
      GetSnapContainerData(*scroller_element->GetLayoutBox());
  EXPECT_TRUE(data);
  cc::SnapContainerData actual_container = *data;

  ScrollableArea* scrollable_area =
      scroller_element->GetLayoutBox()->GetScrollableArea();
  FloatPoint max_position = scrollable_area->ScrollOffsetToPosition(
      scrollable_area->MaximumScrollOffset());

  double width = scroller_element->clientWidth();
  double height = scroller_element->clientHeight();

  cc::SnapContainerData expected_container(
      cc::ScrollSnapType(false, cc::SnapAxis::kBoth,
                         cc::SnapStrictness::kMandatory),
      gfx::RectF(16, 10, width - 28, height - 24),
      gfx::ScrollOffset(max_position.X(), max_position.Y()));
  cc::SnapAreaData expected_area(
      cc::ScrollSnapAlign(cc::SnapAlignment::kCenter),
      gfx::RectF(192, 198, 112, 108), false, cc::ElementId(10));
  expected_container.AddSnapAreaData(expected_area);

  EXPECT_EQ_CONTAINER(expected_container, actual_container);
  EXPECT_EQ_AREA(expected_area, actual_container.at(0));
}

TEST_F(SnapCoordinatorTest, ScaledSnapDataCalculation) {
  SetUpSingleSnapArea();
  Element* area_element = GetDocument().getElementById("area");
  area_element->setAttribute(kStyleAttr,
                             "scroll-snap-align: end; transform: scale(4, 4);");
  UpdateAllLifecyclePhasesForTest();
  Element* scroller_element = GetDocument().getElementById("scroller");
  const cc::SnapContainerData* data =
      GetSnapContainerData(*scroller_element->GetLayoutBox());
  EXPECT_TRUE(data);
  cc::SnapContainerData actual_container = *data;

  ScrollableArea* scrollable_area =
      scroller_element->GetLayoutBox()->GetScrollableArea();
  FloatPoint max_position = scrollable_area->ScrollOffsetToPosition(
      scrollable_area->MaximumScrollOffset());

  double width = scroller_element->clientWidth();
  double height = scroller_element->clientHeight();
  cc::SnapContainerData expected_container(
      cc::ScrollSnapType(false, cc::SnapAxis::kBoth,
                         cc::SnapStrictness::kMandatory),
      gfx::RectF(10, 10, width - 20, height - 20),
      gfx::ScrollOffset(max_position.X(), max_position.Y()));

  // The area is scaled from center, so it pushes the area's top-left corner to
  // (50, 50).
  cc::SnapAreaData expected_area(cc::ScrollSnapAlign(cc::SnapAlignment::kEnd),
                                 gfx::RectF(42, 42, 416, 416), false,
                                 cc::ElementId(10));
  expected_container.AddSnapAreaData(expected_area);

  EXPECT_EQ_CONTAINER(expected_container, actual_container);
  EXPECT_EQ_AREA(expected_area, actual_container.at(0));
}

TEST_F(SnapCoordinatorTest, VerticalRlSnapDataCalculation) {
  SetUpSingleSnapArea();
  Element* area_element = GetDocument().getElementById("area");
  area_element->setAttribute(kStyleAttr,
                             "scroll-snap-align: start; left: -200px;");
  Element* scroller_element = GetDocument().getElementById("scroller");
  scroller_element->setAttribute(kStyleAttr, "writing-mode: vertical-rl;");
  UpdateAllLifecyclePhasesForTest();
  const cc::SnapContainerData* data =
      GetSnapContainerData(*scroller_element->GetLayoutBox());
  EXPECT_TRUE(data);
  cc::SnapContainerData actual_container = *data;

  ScrollableArea* scrollable_area =
      scroller_element->GetLayoutBox()->GetScrollableArea();
  FloatPoint max_position = scrollable_area->ScrollOffsetToPosition(
      scrollable_area->MaximumScrollOffset());

  double width = scroller_element->clientWidth();
  double height = scroller_element->clientHeight();

  cc::SnapContainerData expected_container(
      cc::ScrollSnapType(false, cc::SnapAxis::kBoth,
                         cc::SnapStrictness::kMandatory),
      gfx::RectF(10, 10, width - 20, height - 20),
      gfx::ScrollOffset(max_position.X(), max_position.Y()));
  // Under vertical-rl writing mode, 'start' should align to the right
  // and 'end' should align to the left.
  cc::SnapAreaData expected_area(
      cc::ScrollSnapAlign(cc::SnapAlignment::kStart, cc::SnapAlignment::kEnd),
      gfx::RectF(192, 192, 116, 116), false, cc::ElementId(10));
  expected_container.AddSnapAreaData(expected_area);

  EXPECT_EQ_CONTAINER(expected_container, actual_container);
  EXPECT_EQ_AREA(expected_area, actual_container.at(0));
}

TEST_F(SnapCoordinatorTest, ChangeOverflowToVisible) {
  SetUpSingleSnapArea();
  Element* scroller_element = GetDocument().getElementById("scroller");
  const cc::SnapContainerData* data =
      GetSnapContainerData(*scroller_element->GetLayoutBox());
  EXPECT_TRUE(data);

  // Scroller should no longer be considered a snap container
  scroller_element->setAttribute(kStyleAttr, "overflow : visible");
  UpdateAllLifecyclePhasesForTest();
  data = GetSnapContainerData(*scroller_element->GetLayoutBox());
  EXPECT_FALSE(data);

  // Scroller should be considered a snap container again
  scroller_element->setAttribute(kStyleAttr, "overflow : scroll");
  UpdateAllLifecyclePhasesForTest();
  data = GetSnapContainerData(*scroller_element->GetLayoutBox());
  EXPECT_TRUE(data);
}

TEST_F(SnapCoordinatorTest, CurrentSnappedAreaRemoved) {
  SetUpSingleSnapArea();
  Element* scroller_element = GetDocument().getElementById("scroller");
  Element* area_element = GetDocument().getElementById("area");
  area_element->setAttribute(kStyleAttr, "scroll-snap-align: start;");
  UpdateAllLifecyclePhasesForTest();
  scroller_element->scrollTo(250, 250);
  UpdateAllLifecyclePhasesForTest();

  const cc::SnapContainerData* data_ptr =
      GetSnapContainerData(*scroller_element->GetLayoutBox());
  EXPECT_TRUE(data_ptr);
  cc::SnapContainerData data = *data_ptr;
  cc::TargetSnapAreaElementIds expected_snap_targets(data.at(0).element_id,
                                                     data.at(0).element_id);
  EXPECT_TRUE(expected_snap_targets.x);
  EXPECT_EQ(expected_snap_targets, data.GetTargetSnapAreaElementIds());

  area_element->setAttribute(kStyleAttr, "scroll-snap-align: none;");
  UpdateAllLifecyclePhasesForTest();

  // Removing a snap area should also remove it as the target snap area.
  data_ptr = GetSnapContainerData(*scroller_element->GetLayoutBox());
  EXPECT_TRUE(data_ptr);
  data = *data_ptr;
  EXPECT_EQ(cc::TargetSnapAreaElementIds(), data.GetTargetSnapAreaElementIds());
}

TEST_F(SnapCoordinatorTest, AddingSnapAreaDoesNotRemoveCurrentSnapTarget) {
  // Set up 2 areas. Mark the other as a snap area later.
  SetHTML(R"HTML(
      <style>
      #scroller {
        width: 140px;
        height: 160px;
        padding: 0px;
        scroll-snap-type: both mandatory;
        scroll-padding: 10px;
        overflow: scroll;
      }
      #container {
        margin: 0px;
        padding: 0px;
        width: 500px;
        height: 500px;
      }
      #area {
        position: relative;
        top: 200px;
        left: 200px;
        width: 100px;
        height: 100px;
        scroll-margin: 8px;
      }
      #area2 {
        position: relative;
        top: 400px;
        left: 400px;
        width: 100px;
        height: 100px;
        scroll-margin: 8px;
      }
      </style>
      <div id='scroller'>
        <div id='container'>
          <div id="area"></div>
          <div id="area2"></div>
        </div>
      </div>
      )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* scroller_element = GetDocument().getElementById("scroller");
  Element* area_element = GetDocument().getElementById("area");
  area_element->setAttribute(kStyleAttr, "scroll-snap-align: start;");
  UpdateAllLifecyclePhasesForTest();
  scroller_element->scrollTo(250, 250);
  UpdateAllLifecyclePhasesForTest();

  const cc::SnapContainerData* data_ptr =
      GetSnapContainerData(*scroller_element->GetLayoutBox());
  EXPECT_TRUE(data_ptr);
  cc::SnapContainerData data = *data_ptr;
  cc::TargetSnapAreaElementIds expected_snap_targets(data.at(0).element_id,
                                                     data.at(0).element_id);
  EXPECT_TRUE(expected_snap_targets.x);
  EXPECT_EQ(expected_snap_targets, data.GetTargetSnapAreaElementIds());

  Element* area2_element = GetDocument().getElementById("area2");
  area2_element->setAttribute(kStyleAttr, "scroll-snap-align: start;");
  UpdateAllLifecyclePhasesForTest();

  // Adding another snap area should not affect the current snapped target.
  data_ptr = GetSnapContainerData(*scroller_element->GetLayoutBox());
  EXPECT_TRUE(data_ptr);
  data = *data_ptr;
  EXPECT_EQ(expected_snap_targets, data.GetTargetSnapAreaElementIds());
}
}  // namespace blink
