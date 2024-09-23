// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"

#include "build/build_config.h"
#include "cc/layers/picture_layer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/page/scrolling/snap_coordinator.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/core/testing/color_scheme_helper.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"

using testing::_;

namespace blink {
namespace {

class ScrollableAreaMockChromeClient : public RenderingTestChromeClient {
 public:
  MOCK_METHOD3(MockUpdateTooltipUnderCursor,
               void(LocalFrame*, const String&, TextDirection));
  void UpdateTooltipUnderCursor(LocalFrame& frame,
                                const String& tooltip_text,
                                TextDirection dir) override {
    MockUpdateTooltipUnderCursor(&frame, tooltip_text, dir);
  }
};

}  // namespace

class PaintLayerScrollableAreaTest : public PaintControllerPaintTest {
 public:
  PaintLayerScrollableAreaTest()
      : PaintControllerPaintTest(MakeGarbageCollected<EmptyLocalFrameClient>()),
        chrome_client_(MakeGarbageCollected<ScrollableAreaMockChromeClient>()) {
  }

  ~PaintLayerScrollableAreaTest() override {
    testing::Mock::VerifyAndClearExpectations(&GetChromeClient());
  }

  ScrollableAreaMockChromeClient& GetChromeClient() const override {
    return *chrome_client_;
  }

  bool HasDirectCompositingReasons(const LayoutObject* scroller) {
    const auto* paint_properties = scroller->FirstFragment().PaintProperties();
    return paint_properties && paint_properties->Transform() &&
           paint_properties->Transform()->HasDirectCompositingReasons();
  }

  bool UsesCompositedScrolling(const LayoutBox* scroller) {
    // TODO(crbug.com/1414885): The tests no longer test
    // PaintLayerScrollableArea. We should probably move them into
    // scrolling_test.cc.
    if (auto* scrollable_area = scroller->GetScrollableArea()) {
      const auto* property_trees =
          GetFrame().View()->RootCcLayer()->layer_tree_host()->property_trees();
      if (const auto* scroll_node =
              property_trees->scroll_tree().FindNodeFromElementId(
                  scrollable_area->GetScrollElementId())) {
        return scroll_node->is_composited;
      }
    }
    return false;
  }

  // Default browser preferred color scheme is light. The method sets both
  // browser-based and the OS-based preferred color schemes to dark.
  void SetPreferredColorSchemesToDark(ColorSchemeHelper& color_scheme_helper) {
    color_scheme_helper.SetPreferredRootScrollbarColorScheme(
        mojom::blink::PreferredColorScheme::kDark);
    color_scheme_helper.SetPreferredColorScheme(
        mojom::blink::PreferredColorScheme::kDark);
  }

  void AssertDefaultPreferredColorSchemes() const {
    ASSERT_EQ(GetDocument().GetPreferredColorScheme(),
              mojom::blink::PreferredColorScheme::kLight);
    ASSERT_EQ(
        GetDocument().GetSettings()->GetPreferredRootScrollbarColorScheme(),
        mojom::blink::PreferredColorScheme::kLight);
  }

  void ExpectEqAllScrollControlsNeedPaintInvalidation(
      const PaintLayerScrollableArea* area,
      bool expectation) const {
    EXPECT_EQ(area->VerticalScrollbarNeedsPaintInvalidation(), expectation);
    EXPECT_EQ(area->HorizontalScrollbarNeedsPaintInvalidation(), expectation);
    EXPECT_EQ(area->ScrollCornerNeedsPaintInvalidation(), expectation);
  }

 private:
  void SetUp() override {
    EnableCompositing();
    RenderingTest::SetUp();
  }

  Persistent<ScrollableAreaMockChromeClient> chrome_client_;
};

INSTANTIATE_PAINT_TEST_SUITE_P(PaintLayerScrollableAreaTest);

TEST_P(PaintLayerScrollableAreaTest, OpaqueContainedLayersPromoted) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #scroller { overflow: scroll; height: 200px; width: 200px;
    contain: paint; background: white local content-box;
    border: 10px solid rgba(0, 255, 0, 0.5); }
    #scrolled { height: 300px; }
    </style>
    <div id="scroller"><div id="scrolled"></div></div>
  )HTML");

  EXPECT_TRUE(UsesCompositedScrolling(GetLayoutBoxByElementId("scroller")));
}

TEST_P(PaintLayerScrollableAreaTest, NonStackingContextScrollerPromoted) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #scroller { overflow: scroll; height: 200px; width: 200px;
    background: white local content-box;
    border: 10px solid rgba(0, 255, 0, 0.5); }
    #scrolled { height: 300px; }
    #positioned { position: relative; }
    </style>
    <div id="scroller">
      <div id="positioned">Not contained by scroller.</div>
      <div id="scrolled"></div>
    </div>
  )HTML");

  EXPECT_TRUE(UsesCompositedScrolling(GetLayoutBoxByElementId("scroller")));
}

TEST_P(PaintLayerScrollableAreaTest, TransparentLayersNotPromoted) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #scroller { overflow: scroll; height: 200px; width: 200px; background:
    rgba(0, 255, 0, 0.5) local content-box; border: 10px solid rgba(0, 255,
    0, 0.5); contain: paint; }
    #scrolled { height: 300px; }
    </style>
    <div id="scroller"><div id="scrolled"></div></div>
  )HTML");

  EXPECT_FALSE(UsesCompositedScrolling(GetLayoutBoxByElementId("scroller")));
}

TEST_P(PaintLayerScrollableAreaTest, OpaqueLayersDepromotedOnStyleChange) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #scroller { overflow: scroll; height: 200px; width: 200px; background:
    white local content-box; contain: paint; }
    #scrolled { height: 300px; }
    </style>
    <div id="scroller"><div id="scrolled"></div></div>
  )HTML");

  Element* scroller = GetDocument().getElementById(AtomicString("scroller"));
  EXPECT_TRUE(UsesCompositedScrolling(scroller->GetLayoutBox()));

  // Change the background to transparent
  scroller->setAttribute(
      html_names::kStyleAttr,
      AtomicString("background: rgba(255,255,255,0.5) local content-box;"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(UsesCompositedScrolling(scroller->GetLayoutBox()));
}

TEST_P(PaintLayerScrollableAreaTest, OpaqueLayersPromotedOnStyleChange) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #scroller { overflow: scroll; height: 200px; width: 200px; background:
    rgba(255,255,255,0.5) local content-box; contain: paint; }
    #scrolled { height: 300px; }
    </style>
    <div id="scroller"><div id="scrolled"></div></div>
  )HTML");

  Element* scroller = GetDocument().getElementById(AtomicString("scroller"));
  EXPECT_FALSE(UsesCompositedScrolling(scroller->GetLayoutBox()));

  // Change the background to opaque
  scroller->setAttribute(html_names::kStyleAttr,
                         AtomicString("background: white local content-box;"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(UsesCompositedScrolling(scroller->GetLayoutBox()));
}

// Tests that a transform on the scroller or an ancestor doesn't prevent
// promotion.
TEST_P(PaintLayerScrollableAreaTest,
       TransformDoesNotPreventCompositedScrolling) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #scroller { overflow: scroll; height: 200px; width: 200px; background:
    white local content-box; contain: paint; }
    #scrolled { height: 300px; }
    </style>
    <div id="parent">
      <div id="scroller"><div id="scrolled"></div></div>
    </div>
  )HTML");

  Element* parent = GetDocument().getElementById(AtomicString("parent"));
  Element* scroller = GetDocument().getElementById(AtomicString("scroller"));
  EXPECT_TRUE(UsesCompositedScrolling(scroller->GetLayoutBox()));

  // Change the parent to have a transform.
  parent->setAttribute(html_names::kStyleAttr,
                       AtomicString("transform: translate(1px, 0);"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(UsesCompositedScrolling(scroller->GetLayoutBox()));

  // Change the parent to have no transform again.
  parent->removeAttribute(html_names::kStyleAttr);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(UsesCompositedScrolling(scroller->GetLayoutBox()));

  // Apply a transform to the scroller directly.
  scroller->setAttribute(html_names::kStyleAttr,
                         AtomicString("transform: translate(1px, 0);"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(UsesCompositedScrolling(scroller->GetLayoutBox()));
}

TEST_P(PaintLayerScrollableAreaTest,
       PromoteLayerRegardlessOfSelfAndAncestorOpacity) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #scroller { overflow: scroll; height: 200px; width: 200px; background:
    white local content-box; contain: paint; }
    #scrolled { height: 300px; }
    </style>
    <div id="parent">
      <div id="scroller"><div id="scrolled"></div></div>
    </div>
  )HTML");

  Element* parent = GetDocument().getElementById(AtomicString("parent"));
  Element* scroller = GetDocument().getElementById(AtomicString("scroller"));
  EXPECT_TRUE(UsesCompositedScrolling(scroller->GetLayoutBox()));

  // Change the parent to be partially translucent.
  parent->setAttribute(html_names::kStyleAttr, AtomicString("opacity: 0.5;"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(UsesCompositedScrolling(scroller->GetLayoutBox()));

  // Change the parent to be opaque again.
  parent->setAttribute(html_names::kStyleAttr, AtomicString("opacity: 1;"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(UsesCompositedScrolling(scroller->GetLayoutBox()));

  // Make the scroller translucent.
  scroller->setAttribute(html_names::kStyleAttr, AtomicString("opacity: 0.5"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(UsesCompositedScrolling(scroller->GetLayoutBox()));
}

// Test that will-change: transform applied to the scroller will cause the
// scrolling contents layer to be promoted.
TEST_P(PaintLayerScrollableAreaTest, CompositedScrollOnWillChangeTransform) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller { overflow: scroll; height: 100px; width: 100px; }
      #scrolled { height: 300px; }
    </style>
    <div id="scroller"><div id="scrolled"></div></div>
  )HTML");

  Element* scroller = GetDocument().getElementById(AtomicString("scroller"));
  EXPECT_FALSE(UsesCompositedScrolling(scroller->GetLayoutBox()));

  scroller->setAttribute(html_names::kStyleAttr,
                         AtomicString("will-change: transform"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(UsesCompositedScrolling(scroller->GetLayoutBox()));

  scroller->setAttribute(html_names::kStyleAttr, g_empty_atom);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(UsesCompositedScrolling(scroller->GetLayoutBox()));
}

// Test that will-change: transform applied to the scroller will cause the
// scrolling contents layer to be promoted.
TEST_P(PaintLayerScrollableAreaTest, ScrollLayerOnPointerEvents) {
  SetPreferCompositingToLCDText(true);
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller { overflow: scroll; height: 100px; width: 100px; }
      #scrolled { height: 300px; }
    </style>
    <div id="scroller"><div id="scrolled"></div></div>
  )HTML");

  Element* scroller = GetDocument().getElementById(AtomicString("scroller"));
  EXPECT_TRUE(UsesCompositedScrolling(scroller->GetLayoutBox()));

  // pointer-events: none does not affect whether composited scrolling is
  // present.
  scroller->setAttribute(html_names::kStyleAttr,
                         AtomicString("pointer-events: none"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(UsesCompositedScrolling(scroller->GetLayoutBox()));

  // visibility: hidden causes the scroller to be invisible for hit testing,
  // so ScrollsOverflow becomes false on the PaintLayerScrollableArea, and hence
  // composited scrolling is not present.
  scroller->setAttribute(html_names::kStyleAttr,
                         AtomicString("visibility: hidden"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(UsesCompositedScrolling(scroller->GetLayoutBox()));

  scroller->setAttribute(html_names::kStyleAttr, g_empty_atom);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(UsesCompositedScrolling(scroller->GetLayoutBox()));
}

// Test that <input> elements don't use composited scrolling even with
// "will-change:transform".
TEST_P(PaintLayerScrollableAreaTest, InputElementPromotionTest) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
     .composited { will-change: transform; }
    </style>
    <input id='input' width=10 style='font-size:40pt;'/>
  )HTML");

  Element* element = GetDocument().getElementById(AtomicString("input"));
  EXPECT_FALSE(HasDirectCompositingReasons(element->GetLayoutObject()));
  EXPECT_FALSE(UsesCompositedScrolling(element->GetLayoutBox()));

  element->setAttribute(html_names::kClassAttr, AtomicString("composited"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(HasDirectCompositingReasons(element->GetLayoutObject()));
  EXPECT_FALSE(UsesCompositedScrolling(element->GetLayoutBox()));
}

// Test that <select> elements use composited scrolling with
// "will-change:transform".
TEST_P(PaintLayerScrollableAreaTest, SelectElementPromotionTest) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
     .composited { will-change: transform; }
    </style>
    <select id='select' size='2'>
      <option> value 1</option>
      <option> value 2</option>
      <option> value 3</option>
      <option> value 4</option>
    </select>
  )HTML");

  Element* element = GetDocument().getElementById(AtomicString("select"));
  EXPECT_FALSE(HasDirectCompositingReasons(element->GetLayoutObject()));
  EXPECT_FALSE(UsesCompositedScrolling(element->GetLayoutBox()));

  element->setAttribute(html_names::kClassAttr, AtomicString("composited"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(HasDirectCompositingReasons(element->GetLayoutBox()));
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // <select> implementation is different and not scrollable on Android and iOS.
  EXPECT_FALSE(UsesCompositedScrolling(element->GetLayoutBox()));
#else
  EXPECT_TRUE(UsesCompositedScrolling(element->GetLayoutBox()));
#endif
}

// Ensure OverlayScrollbarColorTheme get updated when page load
TEST_P(PaintLayerScrollableAreaTest, OverlayScrollbarColorThemeUpdated) {
  SetBodyInnerHTML(R"HTML(
    <style>
    div { overflow: scroll; }
    #white { background-color: white; }
    #black { background-color: black; }
    </style>
    <div id="none">a</div>
    <div id="white">b</div>
    <div id="black">c</div>
  )HTML");

  PaintLayer* none_layer = GetPaintLayerByElementId("none");
  PaintLayer* white_layer = GetPaintLayerByElementId("white");
  PaintLayer* black_layer = GetPaintLayerByElementId("black");

  ASSERT_TRUE(none_layer);
  ASSERT_TRUE(white_layer);
  ASSERT_TRUE(black_layer);

  ASSERT_EQ(mojom::blink::ColorScheme::kLight,
            none_layer->GetScrollableArea()->GetOverlayScrollbarColorScheme());
  ASSERT_EQ(mojom::blink::ColorScheme::kLight,
            white_layer->GetScrollableArea()->GetOverlayScrollbarColorScheme());
  ASSERT_EQ(mojom::blink::ColorScheme::kDark,
            black_layer->GetScrollableArea()->GetOverlayScrollbarColorScheme());
}

TEST_P(PaintLayerScrollableAreaTest,
       RecalculatesScrollbarOverlayIfBackgroundChanges) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller {
        width: 10px;
        height: 10px;
        overflow: scroll;
      }
      .forcescroll { height: 1000px; }
    </style>
    <div id="scroller">
      <div class="forcescroll"></div>
    </div>
  )HTML");
  PaintLayer* scroll_paint_layer = GetPaintLayerByElementId("scroller");
  EXPECT_EQ(mojom::blink::ColorScheme::kLight,
            scroll_paint_layer->GetScrollableArea()
                ->GetOverlayScrollbarColorScheme());

  GetElementById("scroller")
      ->setAttribute(html_names::kStyleAttr,
                     AtomicString("background: rgb(34, 85, 51);"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(mojom::blink::ColorScheme::kDark,
            scroll_paint_layer->GetScrollableArea()
                ->GetOverlayScrollbarColorScheme());

  GetElementById("scroller")
      ->setAttribute(html_names::kStyleAttr,
                     AtomicString("background: rgb(236, 143, 185);"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(mojom::blink::ColorScheme::kLight,
            scroll_paint_layer->GetScrollableArea()
                ->GetOverlayScrollbarColorScheme());
}

// The scrollbar overlay color theme should follow the used color scheme when a
// background color is not available on the scroller itself.
TEST_P(PaintLayerScrollableAreaTest, PreferredOverlayScrollbarColorTheme) {
  ColorSchemeHelper color_scheme_helper(GetDocument());
  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kDark);
  SetBodyInnerHTML(R"HTML(
    <meta name="color-scheme" content="light dark">
    <style>
      .scroller {
        width: 10px;
        height: 10px;
        overflow: scroll;
      }
      #white { background-color: white; }
      #black { background-color: black; }
      .forcescroll { height: 1000px; }
    </style>
    <div class="scroller" id="none">
      <div class="forcescroll"></div>
    </div>
    <div class="scroller" id="white">
      <div class="forcescroll"></div>
    </div>
    <div class="scroller" id="black">
      <div class="forcescroll"></div>
    </div>
  )HTML");

  PaintLayer* none_layer = GetPaintLayerByElementId("none");
  PaintLayer* white_layer = GetPaintLayerByElementId("white");
  PaintLayer* black_layer = GetPaintLayerByElementId("black");
  EXPECT_EQ(mojom::blink::ColorScheme::kDark,
            none_layer->GetScrollableArea()->GetOverlayScrollbarColorScheme());
  EXPECT_EQ(mojom::blink::ColorScheme::kLight,
            white_layer->GetScrollableArea()->GetOverlayScrollbarColorScheme());
  EXPECT_EQ(mojom::blink::ColorScheme::kDark,
            black_layer->GetScrollableArea()->GetOverlayScrollbarColorScheme());

  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kLight);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(mojom::blink::ColorScheme::kLight,
            none_layer->GetScrollableArea()->GetOverlayScrollbarColorScheme());
  EXPECT_EQ(mojom::blink::ColorScheme::kLight,
            white_layer->GetScrollableArea()->GetOverlayScrollbarColorScheme());
  EXPECT_EQ(mojom::blink::ColorScheme::kDark,
            black_layer->GetScrollableArea()->GetOverlayScrollbarColorScheme());
}

TEST_P(PaintLayerScrollableAreaTest, HideTooltipWhenScrollPositionChanges) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #scroller { width: 100px; height: 100px; overflow: scroll; }
    #scrolled { height: 300px; }
    </style>
    <div id="scroller"><div id="scrolled"></div></div>
  )HTML");

  Element* scroller = GetDocument().getElementById(AtomicString("scroller"));
  PaintLayerScrollableArea* scrollable_area =
      scroller->GetLayoutBox()->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);

  EXPECT_CALL(GetChromeClient(), MockUpdateTooltipUnderCursor(
                                     GetDocument().GetFrame(), String(), _))
      .Times(1);
  scrollable_area->SetScrollOffset(ScrollOffset(1, 1),
                                   mojom::blink::ScrollType::kUser);

  // Programmatic scrolling should not dismiss the tooltip, so
  // UpdateTooltipUnderCursor should not be called for this invocation.
  EXPECT_CALL(GetChromeClient(), MockUpdateTooltipUnderCursor(
                                     GetDocument().GetFrame(), String(), _))
      .Times(0);
  scrollable_area->SetScrollOffset(ScrollOffset(2, 2),
                                   mojom::blink::ScrollType::kProgrammatic);
}

TEST_P(PaintLayerScrollableAreaTest, IncludeOverlayScrollbarsInVisibleWidth) {
  USE_NON_OVERLAY_SCROLLBARS_OR_QUIT();

  SetBodyInnerHTML(R"HTML(
    <style>
    #scroller { overflow: overlay; height: 100px; width: 100px; }
    #scrolled { width: 100px; height: 200px; }
    </style>
    <div id="scroller"><div id="scrolled"></div></div>
  )HTML");

  Element* scroller = GetDocument().getElementById(AtomicString("scroller"));
  ASSERT_TRUE(scroller);
  PaintLayerScrollableArea* scrollable_area =
      scroller->GetLayoutBox()->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  scrollable_area->SetScrollOffset(ScrollOffset(100, 0),
                                   mojom::blink::ScrollType::kClamping);
  EXPECT_EQ(scrollable_area->GetScrollOffset().x(), 15);
}

TEST_P(PaintLayerScrollableAreaTest, ShowAutoScrollbarsForVisibleContent) {
  USE_NON_OVERLAY_SCROLLBARS_OR_QUIT();

  SetBodyInnerHTML(R"HTML(
    <style>
    #outerDiv {
      width: 15px;
      height: 100px;
      overflow-y: auto;
      overflow-x: hidden;
    }
    #innerDiv {
      height:300px;
      width: 1px;
    }
    </style>
    <div id='outerDiv'>
      <div id='innerDiv'></div>
    </div>
  )HTML");

  Element* outer_div = GetDocument().getElementById(AtomicString("outerDiv"));
  ASSERT_TRUE(outer_div);
  outer_div->GetLayoutBox()->SetNeedsLayout("test");
  UpdateAllLifecyclePhasesForTest();
  PaintLayerScrollableArea* scrollable_area =
      outer_div->GetLayoutBox()->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  EXPECT_TRUE(scrollable_area->HasVerticalScrollbar());
}

TEST_P(PaintLayerScrollableAreaTest, FloatOverflowInRtlContainer) {
  USE_NON_OVERLAY_SCROLLBARS_OR_QUIT();

  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    #container {
      width: 200px;
      overflow-x: auto;
      overflow-y: scroll;
      direction: rtl;
    }
    </style>
    <div id='container'>
      <div style='float:left'>
    lorem ipsum
      </div>
    </div>
  )HTML");

  Element* container = GetDocument().getElementById(AtomicString("container"));
  ASSERT_TRUE(container);
  PaintLayerScrollableArea* scrollable_area =
      container->GetLayoutBox()->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  EXPECT_FALSE(scrollable_area->HasHorizontalScrollbar());
}

TEST_P(PaintLayerScrollableAreaTest, ScrollOriginInRtlContainer) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    #container {
      width: 200px;
      overflow: auto;
      direction: rtl;
    }
    #content {
      width: 300px;
    }
    </style>
    <div id='container'>
      <div id='content'>
    lorem ipsum
      <div>
    </div>
  )HTML");

  Element* container = GetDocument().getElementById(AtomicString("container"));
  ASSERT_TRUE(container);
  PaintLayerScrollableArea* scrollable_area =
      container->GetLayoutBox()->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  EXPECT_EQ(scrollable_area->ScrollOrigin().x(), 100);
}

TEST_P(PaintLayerScrollableAreaTest, OverflowHiddenScrollOffsetInvalidation) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #scroller {
      overflow: hidden;
      height: 200px;
      width: 200px;
    }
    </style>
    <div id='scroller'>
      <div id='forceScroll' style='height: 2000px;'></div>
    </div>
  )HTML");

  auto* scroller = GetLayoutBoxByElementId("scroller");
  auto* scrollable_area = scroller->GetScrollableArea();

  const auto* properties = scroller->FirstFragment().PaintProperties();

  // No scroll offset translation is needed when scroll offset is zero.
  EXPECT_EQ(nullptr, properties->ScrollTranslation());
  EXPECT_EQ(ScrollOffset(0, 0), scrollable_area->GetScrollOffset());

  // A property update is needed when scroll offset changes.
  scrollable_area->SetScrollOffset(ScrollOffset(0, 1),
                                   mojom::blink::ScrollType::kProgrammatic);
  EXPECT_TRUE(scroller->NeedsPaintPropertyUpdate());
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_TRUE(scroller->PaintingLayer()->SelfNeedsRepaint());

  // A scroll offset translation is needed when scroll offset is non-zero.
  EXPECT_EQ(ScrollOffset(0, 1), scrollable_area->GetScrollOffset());
  EXPECT_NE(nullptr, properties->ScrollTranslation());

  UpdateAllLifecyclePhasesForTest();

  scrollable_area->SetScrollOffset(ScrollOffset(0, 2),
                                   mojom::blink::ScrollType::kProgrammatic);
  EXPECT_TRUE(scroller->NeedsPaintPropertyUpdate());
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_TRUE(scroller->PaintingLayer()->SelfNeedsRepaint());

  // A scroll offset translation is still needed when scroll offset is non-zero.
  EXPECT_EQ(ScrollOffset(0, 2), scrollable_area->GetScrollOffset());
  EXPECT_NE(nullptr, properties->ScrollTranslation());

  UpdateAllLifecyclePhasesForTest();

  scrollable_area->SetScrollOffset(ScrollOffset(0, 0),
                                   mojom::blink::ScrollType::kProgrammatic);
  EXPECT_TRUE(scroller->NeedsPaintPropertyUpdate());
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_TRUE(scroller->PaintingLayer()->SelfNeedsRepaint());

  // No scroll offset translation is needed when scroll offset is zero.
  EXPECT_EQ(nullptr, properties->ScrollTranslation());
  EXPECT_EQ(ScrollOffset(0, 0), scrollable_area->GetScrollOffset());
}

TEST_P(PaintLayerScrollableAreaTest, ScrollDoesNotInvalidate) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller {
        overflow: scroll;
        height: 200px;
        width: 200px;
        background: linear-gradient(black, white);
      }
    </style>
    <div id='scroller'>
      <div id='forceScroll' style='height: 2000px;'></div>
    </div>
  )HTML");

  auto* scroller = GetLayoutBoxByElementId("scroller");
  auto* scrollable_area = scroller->GetScrollableArea();

  const auto* properties = scroller->FirstFragment().PaintProperties();
  // Scroll offset translation is needed even when scroll offset is zero.
  EXPECT_NE(nullptr, properties->ScrollTranslation());
  EXPECT_EQ(ScrollOffset(0, 0), scrollable_area->GetScrollOffset());

  // Changing the scroll offset should not require paint invalidation.
  scrollable_area->SetScrollOffset(ScrollOffset(0, 1),
                                   mojom::blink::ScrollType::kProgrammatic);
  EXPECT_FALSE(scroller->ShouldDoFullPaintInvalidation());
  EXPECT_TRUE(scroller->NeedsPaintPropertyUpdate());
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(ScrollOffset(0, 1), scrollable_area->GetScrollOffset());
  EXPECT_NE(nullptr, properties->ScrollTranslation());
}

TEST_P(PaintLayerScrollableAreaTest, ScrollWithStickyNeedsCompositingUpdate) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * {
        margin: 0;
      }
      body {
        height: 610px;
        width: 820px;
      }
      #sticky {
        height: 10px;
        left: 50px;
        position: sticky;
        top: 50px;
        width: 10px;
      }
    </style>
    <div id=sticky></div>
  )HTML");

  auto* scrollable_area = GetLayoutView().GetScrollableArea();
  EXPECT_EQ(ScrollOffset(0, 0), scrollable_area->GetScrollOffset());

  // Changing the scroll offset requires a compositing update to rerun overlap
  // testing.
  scrollable_area->SetScrollOffset(ScrollOffset(0, 1),
                                   mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_FALSE(
      GetDocument().View()->GetPaintArtifactCompositor()->NeedsUpdate());
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(ScrollOffset(0, 1), scrollable_area->GetScrollOffset());
}

TEST_P(PaintLayerScrollableAreaTest,
       ScrollWithFixedDoesNotNeedCompositingUpdate) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * {
        margin: 0;
      }
      body {
        height: 610px;
        width: 820px;
      }
      #fixed {
        height: 10px;
        left: 50px;
        position: fixed;
        top: 50px;
        width: 10px;
      }
    </style>
    <div id=fixed></div>
  )HTML");

  auto* scrollable_area = GetLayoutView().GetScrollableArea();
  EXPECT_EQ(ScrollOffset(0, 0), scrollable_area->GetScrollOffset());

  // Changing the scroll offset should not require a compositing update even
  // though fixed-pos content is present as fixed bounds is already expanded to
  // include all possible scroll offsets.
  scrollable_area->SetScrollOffset(ScrollOffset(0, 1),
                                   mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_FALSE(
      GetDocument().View()->GetPaintArtifactCompositor()->NeedsUpdate());
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(ScrollOffset(0, 1), scrollable_area->GetScrollOffset());
}

TEST_P(PaintLayerScrollableAreaTest,
       ScrollWithLocalAttachmentBackgroundInScrollingContents) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller {
        overflow: scroll;
        height: 200px;
        width: 200px;
        background: linear-gradient(black, white);
        background-attachment: local;
      }
    </style>
    <div id='scroller'>
      <div id='forceScroll' style='height: 2000px;'></div>
    </div>
  )HTML");

  auto* scroller = GetLayoutBoxByElementId("scroller");
  auto* scrollable_area = scroller->GetScrollableArea();
  EXPECT_EQ(kBackgroundPaintInContentsSpace,
            scroller->GetBackgroundPaintLocation());
  EXPECT_FALSE(scrollable_area->BackgroundNeedsRepaintOnScroll());
  EXPECT_TRUE(UsesCompositedScrolling(scroller));

  // Programmatically changing the scroll offset.
  scrollable_area->SetScrollOffset(ScrollOffset(0, 1),
                                   mojom::blink::ScrollType::kProgrammatic);
  // No paint invalidation because it uses composited scrolling.
  EXPECT_FALSE(scroller->ShouldDoFullPaintInvalidation());
  EXPECT_FALSE(scroller->BackgroundNeedsFullPaintInvalidation());

  EXPECT_TRUE(scroller->NeedsPaintPropertyUpdate());
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(ScrollOffset(0, 1), scrollable_area->GetScrollOffset());
  const auto* properties = scroller->FirstFragment().PaintProperties();
  EXPECT_NE(nullptr, properties->ScrollTranslation());
}

TEST_P(PaintLayerScrollableAreaTest, ScrollWith3DPreserveParent) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller {
        overflow-y: scroll;
        height: 200px;
        width: 200px;
        background: white;
        /* TODO(crbug.com/1256990): This is to work around the issue of
           unexpected effect node on a non-self-painting PaintLayer. */
        position: relative;
      }
    </style>
    <div style='transform-style: preserve-3d;'>
      <div id='scroller'>
        <div style='height: 2000px;'></div>
      </div>
    </div>
  )HTML");

  auto* scroller = GetLayoutBoxByElementId("scroller");
  EXPECT_EQ(kBackgroundPaintInBorderBoxSpace,
            scroller->GetBackgroundPaintLocation());
}

TEST_P(PaintLayerScrollableAreaTest,
       ScrollWithLocalAttachmentBackgroundInMainLayer) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller {
        overflow: scroll;
        height: 200px;
        width: 200px;
        border: 10px dashed black;
        background: linear-gradient(black, white) local, yellow;
      }
    </style>
    <div id='scroller'>
      <div id='forceScroll' style='height: 2000px;'></div>
    </div>
  )HTML");

  auto* scroller = GetLayoutBoxByElementId("scroller");
  auto* scrollable_area = scroller->GetScrollableArea();
  EXPECT_EQ(kBackgroundPaintInBothSpaces,
            scroller->GetBackgroundPaintLocation());
  EXPECT_TRUE(scrollable_area->BackgroundNeedsRepaintOnScroll());

  // Programmatically changing the scroll offset.
  scrollable_area->SetScrollOffset(ScrollOffset(0, 1),
                                   mojom::blink::ScrollType::kProgrammatic);
  // No invalidation because the background paints into the main layer.
  EXPECT_TRUE(scroller->ShouldDoFullPaintInvalidation());
  EXPECT_TRUE(scroller->BackgroundNeedsFullPaintInvalidation());
  EXPECT_TRUE(scroller->NeedsPaintPropertyUpdate());
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(ScrollOffset(0, 1), scrollable_area->GetScrollOffset());
  const auto* properties = scroller->FirstFragment().PaintProperties();
  EXPECT_NE(nullptr, properties->ScrollTranslation());
}

TEST_P(PaintLayerScrollableAreaTest, ViewScrollWithFixedAttachmentBackground) {
  SetBodyInnerHTML(R"HTML(
    <style>
      html, #fixed-background {
        background: linear-gradient(black, white) fixed;
      }
      #fixed-background {
        width: 200px;
        height: 200px;
        overflow: scroll;
      }
    </style>
    <div id="fixed-background">
      <div style="height: 3000px"></div>
    </div>
    <div style="height: 3000px"></div>
  )HTML");

  EXPECT_EQ(kBackgroundPaintInContentsSpace,
            GetLayoutView().GetBackgroundPaintLocation());
  auto* fixed_background_div = GetLayoutBoxByElementId("fixed-background");
  EXPECT_EQ(kBackgroundPaintInBorderBoxSpace,
            fixed_background_div->GetBackgroundPaintLocation());
  auto* div_scrollable_area = fixed_background_div->GetScrollableArea();
  auto* view_scrollable_area = GetLayoutView().GetScrollableArea();

  // Programmatically changing the view's scroll offset. Should invalidate all
  // objects with fixed attachment background.
  view_scrollable_area->SetScrollOffset(
      ScrollOffset(0, 1), mojom::blink::ScrollType::kProgrammatic);
  EXPECT_TRUE(fixed_background_div->ShouldDoFullPaintInvalidation());
  EXPECT_TRUE(fixed_background_div->BackgroundNeedsFullPaintInvalidation());
  EXPECT_FALSE(fixed_background_div->NeedsPaintPropertyUpdate());
  EXPECT_TRUE(GetLayoutView().ShouldDoFullPaintInvalidation());
  EXPECT_TRUE(GetLayoutView().BackgroundNeedsFullPaintInvalidation());
  EXPECT_TRUE(GetLayoutView().NeedsPaintPropertyUpdate());
  UpdateAllLifecyclePhasesForTest();

  // Programmatically changing the div's scroll offset. Should invalidate the
  // scrolled div with fixed attachment background.
  div_scrollable_area->SetScrollOffset(ScrollOffset(0, 1),
                                       mojom::blink::ScrollType::kProgrammatic);
  EXPECT_TRUE(fixed_background_div->ShouldDoFullPaintInvalidation());
  EXPECT_TRUE(fixed_background_div->BackgroundNeedsFullPaintInvalidation());
  EXPECT_TRUE(fixed_background_div->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(GetLayoutView().ShouldDoFullPaintInvalidation());
  EXPECT_FALSE(GetLayoutView().BackgroundNeedsFullPaintInvalidation());
  EXPECT_FALSE(GetLayoutView().NeedsPaintPropertyUpdate());
}

TEST_P(PaintLayerScrollableAreaTest,
       ViewScrollWithSolidColorFixedAttachmentBackground) {
  SetBodyInnerHTML(R"HTML(
    <style>
      html, #fixed-background {
        background: green fixed;
      }
      #fixed-background {
        width: 200px;
        height: 200px;
        overflow: scroll;
      }
    </style>
    <div id="fixed-background">
      <div style="height: 3000px"></div>
    </div>
    <div style="height: 3000px"></div>
  )HTML");

  // Fixed-attachment solid-color background should be treated as default
  // attachment.
  EXPECT_EQ(kBackgroundPaintInContentsSpace,
            GetLayoutView().GetBackgroundPaintLocation());
  auto* fixed_background_div = GetLayoutBoxByElementId("fixed-background");
  EXPECT_EQ(kBackgroundPaintInContentsSpace,
            fixed_background_div->GetBackgroundPaintLocation());
  auto* div_scrollable_area = fixed_background_div->GetScrollableArea();
  auto* view_scrollable_area = GetLayoutView().GetScrollableArea();

  // Programmatically changing the view's scroll offset. Should invalidate all
  // objects with fixed attachment background.
  view_scrollable_area->SetScrollOffset(
      ScrollOffset(0, 1), mojom::blink::ScrollType::kProgrammatic);
  EXPECT_FALSE(fixed_background_div->ShouldDoFullPaintInvalidation());
  EXPECT_FALSE(fixed_background_div->BackgroundNeedsFullPaintInvalidation());
  EXPECT_FALSE(fixed_background_div->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(GetLayoutView().ShouldDoFullPaintInvalidation());
  EXPECT_FALSE(GetLayoutView().BackgroundNeedsFullPaintInvalidation());
  EXPECT_TRUE(GetLayoutView().NeedsPaintPropertyUpdate());
  UpdateAllLifecyclePhasesForTest();

  // Programmatically changing the div's scroll offset. Should invalidate the
  // scrolled div with fixed attachment background.
  div_scrollable_area->SetScrollOffset(ScrollOffset(0, 1),
                                       mojom::blink::ScrollType::kProgrammatic);
  EXPECT_FALSE(fixed_background_div->ShouldDoFullPaintInvalidation());
  EXPECT_FALSE(fixed_background_div->BackgroundNeedsFullPaintInvalidation());
  EXPECT_TRUE(fixed_background_div->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(GetLayoutView().ShouldDoFullPaintInvalidation());
  EXPECT_FALSE(GetLayoutView().BackgroundNeedsFullPaintInvalidation());
  EXPECT_FALSE(GetLayoutView().NeedsPaintPropertyUpdate());
}

TEST_P(PaintLayerScrollableAreaTest,
       ViewScrollWithFixedAttachmentBackgroundPreferCompositingToLCDText) {
  SetPreferCompositingToLCDText(true);
  SetBodyInnerHTML(R"HTML(
    <style>
      html {
        background: linear-gradient(black, white) fixed;
      }
      #fixed-background {
        background: linear-gradient(black, white) fixed,
                    linear-gradient(blue, yellow) local;
        width: 200px;
        height: 200px;
        overflow: scroll;
      }
    </style>
    <div id="fixed-background">
      <div style="height: 3000px"></div>
    </div>
    <div style="height: 3000px"></div>
  )HTML");

  EXPECT_EQ(kBackgroundPaintInBorderBoxSpace,
            GetLayoutView().GetBackgroundPaintLocation());
  auto* fixed_background_div = GetLayoutBoxByElementId("fixed-background");
  EXPECT_EQ(kBackgroundPaintInBorderBoxSpace,
            fixed_background_div->GetBackgroundPaintLocation());
  auto* div_scrollable_area = fixed_background_div->GetScrollableArea();
  auto* view_scrollable_area = GetLayoutView().GetScrollableArea();

  // Programmatically changing the view's scroll offset. Should invalidate all
  // objects with fixed attachment background except the layout view.
  view_scrollable_area->SetScrollOffset(
      ScrollOffset(0, 1), mojom::blink::ScrollType::kProgrammatic);
  EXPECT_TRUE(fixed_background_div->ShouldDoFullPaintInvalidation());
  EXPECT_TRUE(fixed_background_div->BackgroundNeedsFullPaintInvalidation());
  EXPECT_FALSE(fixed_background_div->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(GetLayoutView().ShouldDoFullPaintInvalidation());
  EXPECT_FALSE(GetLayoutView().BackgroundNeedsFullPaintInvalidation());
  EXPECT_TRUE(GetLayoutView().NeedsPaintPropertyUpdate());
  UpdateAllLifecyclePhasesForTest();

  // Programmatically changing the div's scroll offset. Should invalidate the
  // scrolled div with fixed attachment background.
  div_scrollable_area->SetScrollOffset(ScrollOffset(0, 1),
                                       mojom::blink::ScrollType::kProgrammatic);
  EXPECT_TRUE(fixed_background_div->ShouldDoFullPaintInvalidation());
  EXPECT_TRUE(fixed_background_div->BackgroundNeedsFullPaintInvalidation());
  EXPECT_TRUE(fixed_background_div->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(GetLayoutView().ShouldDoFullPaintInvalidation());
  EXPECT_FALSE(GetLayoutView().BackgroundNeedsFullPaintInvalidation());
  EXPECT_FALSE(GetLayoutView().NeedsPaintPropertyUpdate());
}

TEST_P(PaintLayerScrollableAreaTest, ViewScrollWithScrollAttachmentBackground) {
  SetPreferCompositingToLCDText(true);
  SetBodyInnerHTML(R"HTML(
    <style>html { background: linear-gradient(black, white) scroll; }</style>
    <div style="height: 3000px"></div>
  )HTML");

  // background-attachment: scroll on the view is equivalent to local.
  EXPECT_EQ(kBackgroundPaintInContentsSpace,
            GetLayoutView().GetBackgroundPaintLocation());
  auto* view_scrollable_area = GetLayoutView().GetScrollableArea();
  EXPECT_FALSE(view_scrollable_area->BackgroundNeedsRepaintOnScroll());
  view_scrollable_area->SetScrollOffset(
      ScrollOffset(0, 1), mojom::blink::ScrollType::kProgrammatic);
  EXPECT_FALSE(GetLayoutView().ShouldDoFullPaintInvalidation());
  EXPECT_FALSE(GetLayoutView().BackgroundNeedsFullPaintInvalidation());
  EXPECT_TRUE(GetLayoutView().NeedsPaintPropertyUpdate());
  UpdateAllLifecyclePhasesForTest();
}

TEST_P(PaintLayerScrollableAreaTest, ViewScrollWithLocalAttachmentBackground) {
  SetPreferCompositingToLCDText(true);
  SetBodyInnerHTML(R"HTML(
    <style>html { background: linear-gradient(black, white) local; }</style>
    <div style="height: 3000px"></div>
  )HTML");

  EXPECT_EQ(kBackgroundPaintInContentsSpace,
            GetLayoutView().GetBackgroundPaintLocation());
  auto* view_scrollable_area = GetLayoutView().GetScrollableArea();
  EXPECT_FALSE(view_scrollable_area->BackgroundNeedsRepaintOnScroll());
  view_scrollable_area->SetScrollOffset(
      ScrollOffset(0, 1), mojom::blink::ScrollType::kProgrammatic);
  EXPECT_FALSE(GetLayoutView().ShouldDoFullPaintInvalidation());
  EXPECT_FALSE(GetLayoutView().BackgroundNeedsFullPaintInvalidation());
  EXPECT_TRUE(GetLayoutView().NeedsPaintPropertyUpdate());
  UpdateAllLifecyclePhasesForTest();
}

TEST_P(PaintLayerScrollableAreaTest, HitTestOverlayScrollbars) {
  SetBodyInnerHTML(R"HTML(
    <style>
    html, body {
      margin: 0;
    }
    #scroller {
      overflow: scroll;
      height: 100px;
      width: 100px;
    }
    #scrolled {
      width: 1000px;
      height: 1000px;
    }
    </style>
    <div id='scroller'><div id='scrolled'></div></div>
  )HTML");

  auto* scroller = GetLayoutBoxByElementId("scroller");
  auto* scrollable_area = scroller->GetScrollableArea();

  scrollable_area->SetScrollbarsHiddenIfOverlay(true);

  HitTestRequest hit_request(HitTestRequest::kMove | HitTestRequest::kReadOnly);
  HitTestLocation location(PhysicalOffset(95, 5));
  HitTestResult hit_result(hit_request, location);
  GetDocument().GetLayoutView()->HitTest(location, hit_result);
  EXPECT_EQ(hit_result.GetScrollbar(), nullptr);
  location = HitTestLocation(PhysicalOffset(5, 95));
  hit_result = HitTestResult(hit_request, location);
  GetDocument().GetLayoutView()->HitTest(location, hit_result);
  EXPECT_EQ(hit_result.GetScrollbar(), nullptr);

  scrollable_area->SetScrollbarsHiddenIfOverlay(false);

  location = HitTestLocation(PhysicalOffset(95, 5));
  hit_result = HitTestResult(hit_request, location);
  GetDocument().GetLayoutView()->HitTest(location, hit_result);
  EXPECT_EQ(hit_result.GetScrollbar(), scrollable_area->VerticalScrollbar());
  location = HitTestLocation(PhysicalOffset(5, 95));
  hit_result = HitTestResult(hit_request, location);
  GetDocument().GetLayoutView()->HitTest(location, hit_result);
  EXPECT_EQ(hit_result.GetScrollbar(), scrollable_area->HorizontalScrollbar());
}

TEST_P(PaintLayerScrollableAreaTest,
       ShowNonCompositedScrollbarOnCompositorScroll) {
  // Scrollbars are always composited in RasterInducingScroll.
  if (RuntimeEnabledFeatures::RasterInducingScrollEnabled()) {
    return;
  }

  SetBodyInnerHTML(R"HTML(
    <style>
    html, body {
      margin: 0;
    }
    #scroller {
      overflow: scroll;
      height: 100px;
      width: 100px;
    }
    #scrolled {
      width: 1000px;
      height: 1000px;
    }
    </style>
    <div id='scroller'><div id='scrolled'></div></div>
  )HTML");

  auto* scroller = GetLayoutBoxByElementId("scroller");
  auto* scrollable_area = scroller->GetScrollableArea();

  scrollable_area->SetScrollbarsHiddenIfOverlay(true);

  EXPECT_TRUE(scrollable_area->ScrollbarsHiddenIfOverlay());

  // This is false because we prefer LCD-text by default and the scroller
  // doesn't have an opaque background to preserve LCD-text if composited.
  EXPECT_FALSE(scrollable_area->UsesCompositedScrolling());

  scrollable_area->SetScrollOffset(ScrollOffset(0, 20),
                                   mojom::blink::ScrollType::kCompositor);

  EXPECT_FALSE(scrollable_area->ScrollbarsHiddenIfOverlay());
}

TEST_P(PaintLayerScrollableAreaTest, CompositedStickyDescendant) {
  SetBodyInnerHTML(R"HTML(
    <div id=scroller style="overflow: scroll; width: 500px; height: 300px;
        will-change: transform">
      <div id=sticky style="top: 0px; position: sticky; background: green">
      </div>
      <div style="width: 10px; height: 700px; background: lightblue"></div>
    </div>
  )HTML");
  auto* scroller = GetLayoutBoxByElementId("scroller");
  auto* scrollable_area = scroller->GetScrollableArea();
  auto* sticky = GetLayoutBoxByElementId("sticky");

  EXPECT_EQ(&sticky->FirstFragment().LocalBorderBoxProperties().Transform(),
            sticky->FirstFragment().PaintProperties()->StickyTranslation());
  EXPECT_TRUE(sticky->FirstFragment()
                  .PaintProperties()
                  ->StickyTranslation()
                  ->IsIdentity());

  scrollable_area->SetScrollOffset(ScrollOffset(0, 50),
                                   mojom::blink::ScrollType::kUser);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(gfx::Vector2dF(0, 50), sticky->FirstFragment()
                                       .PaintProperties()
                                       ->StickyTranslation()
                                       ->Get2dTranslation());
}

TEST_P(PaintLayerScrollableAreaTest, StickyPositionUseCounter) {
  SetBodyInnerHTML(R"HTML(
    <div style="overflow: scroll; width: 500px; height: 300px;">
      <div id=test></div>
      <div id=forcescroll style="width: 10px; height: 700px;"></div>
    </div>
  )HTML");
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kPositionSticky));

  auto* test = GetElementById("test");
  test->setAttribute(html_names::kStyleAttr, AtomicString("position: sticky;"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kPositionSticky));

  test->setAttribute(html_names::kStyleAttr,
                     AtomicString("top: 0; position: sticky;"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kPositionSticky));
}

// Delayed scroll offset clamping should not crash. https://crbug.com/842495
TEST_P(PaintLayerScrollableAreaTest, IgnoreDelayedScrollOnDestroyedLayer) {
  SetBodyInnerHTML(R"HTML(
    <div id=scroller style="overflow: scroll; width: 200px; height: 200px;">
      <div style="height: 1000px;"></div>
    </div>
  )HTML");
  Element* scroller = GetDocument().getElementById(AtomicString("scroller"));
  {
    PaintLayerScrollableArea::DelayScrollOffsetClampScope scope;
    PaintLayerScrollableArea::DelayScrollOffsetClampScope::SetNeedsClamp(
        scroller->GetLayoutBox()->GetScrollableArea());
    scroller->SetInlineStyleProperty(CSSPropertyID::kDisplay,
                                     CSSValueID::kNone);
    UpdateAllLifecyclePhasesForTest();
  }
}

TEST_P(PaintLayerScrollableAreaTest, ScrollbarMaximum) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #spacer {
      height: 17.984375px;
    }
    #scroller {
      border-top: 0.328125px solid gray;
      border-bottom: 0.328125px solid gray;
      height:149.34375px;
      width: 100px;
      overflow-y:auto;
    }
    #content {
      height: 156.578125px;
    }
    </style>
    <div id='spacer'></div>
    <div id='scroller'>
      <div id='content'></div>
    </div>
  )HTML");

  LayoutBox* scroller = GetLayoutBoxByElementId("scroller");
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  Scrollbar* scrollbar = scrollable_area->VerticalScrollbar();

  scrollable_area->ScrollBy(ScrollOffset(0, 1000),
                            mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(scrollbar->CurrentPos(), scrollbar->Maximum());
}

TEST_P(PaintLayerScrollableAreaTest, ScrollingBackgroundVisualRect) {
  SetBodyInnerHTML(R"HTML(
    <style>
      ::-webkit-scrollbar { display: none; }
      #scroller {
        width: 100.7px;
        height: 100.4px;
        overflow: scroll;
        border-top: 2.6px solid blue;
        border-left: 2.4px solid blue;
        will-change: transform;
      }
      #content {
        width: 50.7px;
        height: 200.4px;
      }
    </style>
    <div id="scroller">
      <div id="content"></div>
    </div>
  )HTML");

  EXPECT_EQ(gfx::Rect(2, 2, 101, 200),
            GetLayoutBoxByElementId("scroller")
                ->GetScrollableArea()
                ->ScrollingBackgroundVisualRect(PhysicalOffset()));
}

TEST_P(PaintLayerScrollableAreaTest, RtlScrollOriginSnapping) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #container {
        direction: rtl;
        display: flex;
      }
      #scroller {
        width: 100%;
        height: 100px;
        overflow: hidden;
      }
      #scroller-content {
        width: 200%;
        height: 200px;
      }
    </style>
    <div id="container">
      <div id="first-child" style="flex:1; display:none"></div>
      <div style="flex:2.2">
        <div id="scroller">
          <div id ="scroller-content"></div>
        </div>
      </div>
    </div>
  )HTML");

  // Test that scroll origin is snapped such that maximum scroll offset is
  // always zero for an rtl block.

  GetFrame().View()->Resize(795, 600);
  UpdateAllLifecyclePhasesForTest();
  LayoutBox* scroller = GetLayoutBoxByElementId("scroller");
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  EXPECT_EQ(scrollable_area->MaximumScrollOffsetInt(), gfx::Vector2d(0, 100));

  Element* first_child = GetElementById("first-child");
  first_child->RemoveInlineStyleProperty(CSSPropertyID::kDisplay);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(scrollable_area->MaximumScrollOffsetInt(), gfx::Vector2d(0, 100));
}

TEST_P(PaintLayerScrollableAreaTest, ShowCustomResizerInTextarea) {
  GetPage().GetSettings().SetTextAreasAreResizable(true);
  SetBodyInnerHTML(R"HTML(
    <!doctype HTML>
    <style>
      textarea {
        width: 200px;
        height: 100px;
      }
      ::-webkit-resizer {
        background-color: red;
      }
    </style>
    <textarea id="target"></textarea>
  )HTML");

  const auto* paint_layer = GetPaintLayerByElementId("target");
  ASSERT_TRUE(paint_layer);

  EXPECT_NE(paint_layer->GetScrollableArea()->Resizer(), nullptr);
}

TEST_P(PaintLayerScrollableAreaTest,
       ApplyPendingHistoryRestoreScrollOffsetTwice) {
  GetPage().GetSettings().SetTextAreasAreResizable(true);
  SetBodyInnerHTML(R"HTML(
    <!doctype HTML>
    <div id="target" style="overflow: scroll; width: 50px; height: 50px">
      <div style="width: 50px; height: 500px">
      </div>
    </div>
  )HTML");

  const auto* paint_layer = GetPaintLayerByElementId("target");
  auto* scrollable_area = paint_layer->GetScrollableArea();

  HistoryItem::ViewState view_state;
  view_state.scroll_offset_ = ScrollOffset(0, 100);
  scrollable_area->SetPendingHistoryRestoreScrollOffset(
      view_state, true, mojom::blink::ScrollBehavior::kAuto);
  scrollable_area->ApplyPendingHistoryRestoreScrollOffset();
  EXPECT_EQ(ScrollOffset(0, 100), scrollable_area->GetScrollOffset());

  scrollable_area->SetScrollOffset(ScrollOffset(0, 50),
                                   mojom::blink::ScrollType::kUser);

  // The second call to ApplyPendingHistoryRestoreScrollOffset should
  // do nothing, since the history was already restored.
  scrollable_area->ApplyPendingHistoryRestoreScrollOffset();
  EXPECT_EQ(ScrollOffset(0, 50), scrollable_area->GetScrollOffset());
}

// Test that a trivial 3D transform results in composited scrolling.
TEST_P(PaintLayerScrollableAreaTest, CompositeWithTrivial3D) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller {
        width: 100px;
        height: 100px;
        overflow: scroll;
        transform: translateZ(0);
      }
      #scrolled {
        width: 200px;
        height: 200px;
      }
    </style>
    <div id="scroller">
      <div id="scrolled"></div>
    </div>
  )HTML");

  EXPECT_TRUE(UsesCompositedScrolling(GetLayoutBoxByElementId("scroller")));
}

class PaintLayerScrollableAreaTestLowEndPlatform
    : public TestingPlatformSupport {
 public:
  bool IsLowEndDevice() override { return true; }
};

// Test that a trivial 3D transform results in composited scrolling even on
// low-end devices that may not composite trivial 3D transforms.
TEST_P(PaintLayerScrollableAreaTest, LowEndCompositeWithTrivial3D) {
  ScopedTestingPlatformSupport<PaintLayerScrollableAreaTestLowEndPlatform>
      platform;
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller {
        width: 100px;
        height: 100px;
        overflow: scroll;
        transform: translateZ(0);
      }
      #scrolled {
        width: 200px;
        height: 200px;
      }
    </style>
    <div id="scroller">
      <div id="scrolled"></div>
    </div>
  )HTML");

  EXPECT_TRUE(UsesCompositedScrolling(GetLayoutBoxByElementId("scroller")));
}

TEST_P(PaintLayerScrollableAreaTest,
       RootScrollbarShouldUseParentOfOverscrollNodeAsTransformNode) {
  SetPreferCompositingToLCDText(true);
  SetBodyInnerHTML(R"HTML(
    <style>
    ::-webkit-scrollbar {
      width: 12px;
      background: darkblue;
    }
    ::-webkit-scrollbar-thumb {
      background: white;
    }
    #scroller {
      height: 100px;
      overflow-y: scroll;
    }
    .big {
      height: 1000px;
    }
    </style>

    <div class='big'></div>
    <div id='scroller'>
      <div class='big'></div>
    </div>
  )HTML");

  {
    const auto* root_scrollable = GetDocument().View()->LayoutViewport();
    const auto& visual_viewport = GetPage().GetVisualViewport();

    const auto& paint_chunks = ContentPaintChunks();
    bool found_root_scrollbar = false;
    const auto* parent_transform =
        visual_viewport.GetOverscrollElasticityTransformNode()
            ? visual_viewport.GetOverscrollElasticityTransformNode()->Parent()
            : visual_viewport.GetPageScaleNode()->Parent();
    for (const auto& chunk : paint_chunks) {
      if (chunk.id == PaintChunk::Id(root_scrollable->VerticalScrollbar()->Id(),
                                     DisplayItem::kScrollbarHitTest)) {
        EXPECT_EQ(parent_transform, &chunk.properties.Transform());
        found_root_scrollbar = true;
      }
    }
    EXPECT_TRUE(found_root_scrollbar);
  }

  // Non root scrollbar should use scroller's transform node.
  {
    PaintLayer* scroller_layer = GetPaintLayerByElementId("scroller");
    PaintLayerScrollableArea* scrollable_area =
        scroller_layer->GetScrollableArea();
    ASSERT_TRUE(scrollable_area);

    auto paint_properties = scroller_layer->GetLayoutObject()
                                .FirstFragment()
                                .LocalBorderBoxProperties();

    const auto& paint_chunks = ContentPaintChunks();
    bool found_subscroller_scrollbar = false;
    for (const auto& chunk : paint_chunks) {
      if (chunk.id == PaintChunk::Id(scrollable_area->VerticalScrollbar()->Id(),
                                     DisplayItem::kScrollbarHitTest)) {
        EXPECT_EQ(&chunk.properties.Transform(), &paint_properties.Transform());

        found_subscroller_scrollbar = true;
      }
    }
    EXPECT_TRUE(found_subscroller_scrollbar);
  }
}

TEST_P(PaintLayerScrollableAreaTest,
       ResizeSmallerToBeScrollableWithResizerAndStackedChild) {
  USE_NON_OVERLAY_SCROLLBARS_OR_QUIT();

  SetBodyInnerHTML(R"HTML(
    <div id="scroller"
         style="overflow: auto; width: 150px; height: 100px; resize: both">
      <div style="width: 149px; height: 98px; position: relative"></div>
    </div>
  )HTML");

  auto* scroller = GetDocument().getElementById(AtomicString("scroller"));
  auto* scrollable_area = scroller->GetLayoutBox()->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  EXPECT_FALSE(scrollable_area->HasScrollbar());
  // The resizer needs to be painted above the stacked child.
  EXPECT_TRUE(scrollable_area->HasOverlayOverflowControls());
  EXPECT_TRUE(
      scroller->GetLayoutBox()->Layer()->NeedsReorderOverlayOverflowControls());

  // Shrink the scroller, and it becomes scrollable.
  scroller->SetInlineStyleProperty(CSSPropertyID::kWidth, "140px");
  UpdateAllLifecyclePhasesForTest();
  ASSERT_TRUE(scrollable_area->HasScrollbar());
  ASSERT_FALSE(scrollable_area->HorizontalScrollbar()->IsOverlayScrollbar());
  // Because there is non-overlay scrollbar, the resizer on longer overlaps
  // with the contents, so no need to overlay.
  EXPECT_FALSE(scrollable_area->HasOverlayOverflowControls());
  EXPECT_FALSE(
      scroller->GetLayoutBox()->Layer()->NeedsReorderOverlayOverflowControls());
}

TEST_P(PaintLayerScrollableAreaTest, RemoveAddResizerWithoutScrollbars) {
  SetBodyInnerHTML(R"HTML(
    <div id="target"
         style="width: 100px; height: 100px; resize: both; overflow: hidden">
      <div style="position: relative; height: 50px"></div>
    </div>
  )HTML");

  auto* target = GetDocument().getElementById(AtomicString("target"));
  auto* scrollable_area = target->GetLayoutBox()->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  EXPECT_FALSE(scrollable_area->HasScrollbar());
  EXPECT_TRUE(scrollable_area->HasOverlayOverflowControls());
  EXPECT_TRUE(scrollable_area->Layer()->NeedsReorderOverlayOverflowControls());

  target->RemoveInlineStyleProperty(CSSPropertyID::kResize);
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(scrollable_area, target->GetLayoutBox()->GetScrollableArea());
  ASSERT_FALSE(scrollable_area->HasScrollbar());
  EXPECT_FALSE(scrollable_area->HasOverlayOverflowControls());
  EXPECT_FALSE(scrollable_area->Layer()->NeedsReorderOverlayOverflowControls());

  target->SetInlineStyleProperty(CSSPropertyID::kResize, "both");
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(scrollable_area, target->GetLayoutBox()->GetScrollableArea());
  ASSERT_FALSE(scrollable_area->HasScrollbar());
  EXPECT_TRUE(scrollable_area->HasOverlayOverflowControls());
  EXPECT_TRUE(scrollable_area->Layer()->NeedsReorderOverlayOverflowControls());
}

TEST_P(PaintLayerScrollableAreaTest, UsedColorSchemeRootScrollbarsDark) {
  USE_NON_OVERLAY_SCROLLBARS_OR_QUIT();

  SetHtmlInnerHTML(R"HTML(
    <style>
      body { height: 1000px; }
      .container { overflow-y: scroll; width: 100px; height: 100px; }
      .scrollable { height: 400px; }
      #dark { color-scheme: light dark; }
    </style>

    <div id="dark" class="container">
      <div class="scrollable"></div>
    </div>
    <div id="normal" class="container">
      <div class="scrollable"></div>
    </div>
  )HTML");

  AssertDefaultPreferredColorSchemes();

  const auto* root_scrollable_area = GetLayoutView().GetScrollableArea();
  ASSERT_TRUE(root_scrollable_area);
  const auto* non_root_scrollable_area_dark =
      GetPaintLayerByElementId("dark")->GetScrollableArea();
  ASSERT_TRUE(non_root_scrollable_area_dark);
  const auto* non_root_scrollable_area_normal =
      GetPaintLayerByElementId("normal")->GetScrollableArea();
  ASSERT_TRUE(non_root_scrollable_area_normal);

  EXPECT_EQ(root_scrollable_area->UsedColorSchemeScrollbars(),
            mojom::blink::ColorScheme::kLight);
  EXPECT_EQ(non_root_scrollable_area_dark->UsedColorSchemeScrollbars(),
            mojom::blink::ColorScheme::kLight);
  EXPECT_EQ(non_root_scrollable_area_normal->UsedColorSchemeScrollbars(),
            mojom::blink::ColorScheme::kLight);

  // Change color scheme to dark.
  ColorSchemeHelper color_scheme_helper(GetDocument());
  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kDark);
  UpdateAllLifecyclePhasesForTest();

  // Root scrollable area hasn't changed its value because the browser color
  // scheme is light.
  EXPECT_EQ(root_scrollable_area->UsedColorSchemeScrollbars(),
            mojom::blink::ColorScheme::kLight);
  EXPECT_EQ(non_root_scrollable_area_dark->UsedColorSchemeScrollbars(),
            mojom::blink::ColorScheme::kDark);
  EXPECT_EQ(non_root_scrollable_area_normal->UsedColorSchemeScrollbars(),
            mojom::blink::ColorScheme::kLight);

  // Change browser preferred color scheme to dark.
  color_scheme_helper.SetPreferredRootScrollbarColorScheme(
      mojom::blink::PreferredColorScheme::kDark);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(root_scrollable_area->UsedColorSchemeScrollbars(),
            mojom::blink::ColorScheme::kDark);
  EXPECT_EQ(non_root_scrollable_area_dark->UsedColorSchemeScrollbars(),
            mojom::blink::ColorScheme::kDark);
  EXPECT_EQ(non_root_scrollable_area_normal->UsedColorSchemeScrollbars(),
            mojom::blink::ColorScheme::kLight);
}

TEST_P(PaintLayerScrollableAreaTest,
       UsedColorSchemeRootScrollbarsMetaLightDark) {
  USE_NON_OVERLAY_SCROLLBARS_OR_QUIT();

  SetHtmlInnerHTML(R"HTML(
    <meta name="color-scheme" content="light dark">
    <style>
      html { height: 1000px; }
    </style>
  )HTML");

  AssertDefaultPreferredColorSchemes();

  const auto* root_scrollable_area = GetLayoutView().GetScrollableArea();
  ASSERT_TRUE(root_scrollable_area);

  EXPECT_EQ(root_scrollable_area->UsedColorSchemeScrollbars(),
            mojom::blink::ColorScheme::kLight);

  ColorSchemeHelper color_scheme_helper(GetDocument());
  SetPreferredColorSchemesToDark(color_scheme_helper);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(root_scrollable_area->UsedColorSchemeScrollbars(),
            mojom::blink::ColorScheme::kDark);
}

TEST_P(PaintLayerScrollableAreaTest, UsedColorSchemeRootScrollbarsHtmlLight) {
  USE_NON_OVERLAY_SCROLLBARS_OR_QUIT();

  SetHtmlInnerHTML(R"HTML(
    <meta name="color-scheme" content="dark">
    <style>
      html { height: 1000px; color-scheme: light; }
    </style>
  )HTML");

  AssertDefaultPreferredColorSchemes();

  const auto* root_scrollable_area = GetLayoutView().GetScrollableArea();
  ASSERT_TRUE(root_scrollable_area);

  EXPECT_EQ(root_scrollable_area->UsedColorSchemeScrollbars(),
            mojom::blink::ColorScheme::kLight);

  ColorSchemeHelper color_scheme_helper(GetDocument());
  SetPreferredColorSchemesToDark(color_scheme_helper);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(root_scrollable_area->UsedColorSchemeScrollbars(),
            mojom::blink::ColorScheme::kLight);
}

TEST_P(PaintLayerScrollableAreaTest, UsedColorSchemeRootScrollbarsBodyLight) {
  USE_NON_OVERLAY_SCROLLBARS_OR_QUIT();

  SetHtmlInnerHTML(R"HTML(
    <meta name="color-scheme" content="dark">
    <style>
      body { height: 1000px; color-scheme: light; }
    </style>
  )HTML");

  AssertDefaultPreferredColorSchemes();

  const auto* root_scrollable_area = GetLayoutView().GetScrollableArea();
  ASSERT_TRUE(root_scrollable_area);

  EXPECT_EQ(root_scrollable_area->UsedColorSchemeScrollbars(),
            mojom::blink::ColorScheme::kDark);
}

TEST_P(PaintLayerScrollableAreaTest,
       UsedColorSchemeRootScrollbarsInvalidateOnPreferredColorSchemeChange) {
  USE_NON_OVERLAY_SCROLLBARS_OR_QUIT();

  SetHtmlInnerHTML(R"HTML(
    <style>
      html { height: 1000px; width: 1000px; }
      .container { overflow: scroll; width: 100px; height: 100px; }
      .scrollable { height: 400px; width: 400px; }
    </style>
    <div id="normal" class="container">
      <div class="scrollable"></div>
    </div>
  )HTML");

  AssertDefaultPreferredColorSchemes();

  const auto* non_root_scroller = GetLayoutBoxByElementId("normal");
  ASSERT_TRUE(non_root_scroller);

  // Change preferred color scheme to dark.
  ColorSchemeHelper color_scheme_helper(GetDocument());
  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kDark);

  // Root scrollbars should be set for invalidation after the preferred color
  // scheme change.
  EXPECT_TRUE(GetLayoutView().ShouldDoFullPaintInvalidation());

  // Non root scrollbars should not change.
  EXPECT_FALSE(non_root_scroller->ShouldDoFullPaintInvalidation());
}

TEST_P(PaintLayerScrollableAreaTest,
       UsedColorSchemeRootScrollbarsInvalidateOnNormalToLightChange) {
  USE_NON_OVERLAY_SCROLLBARS_OR_QUIT();

  SetHtmlInnerHTML(R"HTML(
    <style>
      html { height: 1000px; width: 1000px; }
      .container { overflow: scroll; width: 100px; height: 100px; }
      .scrollable { height: 400px; width: 400px; }
    </style>
    <div id="normal" class="container">
      <div class="scrollable"></div>
    </div>
  )HTML");

  AssertDefaultPreferredColorSchemes();

  const auto* root_scrollable_area = GetLayoutView().GetScrollableArea();
  ASSERT_TRUE(root_scrollable_area);
  const auto* non_root_scrollable_area =
      GetPaintLayerByElementId("normal")->GetScrollableArea();
  ASSERT_TRUE(non_root_scrollable_area);

  ColorSchemeHelper color_scheme_helper(GetDocument());
  SetPreferredColorSchemesToDark(color_scheme_helper);
  UpdateAllLifecyclePhasesForTest();

  // Set root element's color scheme to light.
  GetDocument().documentElement()->SetInlineStyleProperty(
      CSSPropertyID::kColorScheme, AtomicString("light"));

  // Update lifecycle up until the pre-paint before the scrollbars paint is
  // invalidated.
  GetDocument().View()->UpdateLifecycleToCompositingInputsClean(
      DocumentUpdateReason::kTest);

  // Root scrollbars should be set for invalidation after the color scheme
  // change.
  ExpectEqAllScrollControlsNeedPaintInvalidation(root_scrollable_area, true);

  // Non root scrollbars should not change.
  ExpectEqAllScrollControlsNeedPaintInvalidation(non_root_scrollable_area,
                                                 false);

  EXPECT_EQ(root_scrollable_area->UsedColorSchemeScrollbars(),
            mojom::blink::ColorScheme::kLight);
}

TEST_P(PaintLayerScrollableAreaTest,
       UsedColorSchemeRootScrollbarsInvalidateOnLightToNormalChange) {
  USE_NON_OVERLAY_SCROLLBARS_OR_QUIT();

  SetHtmlInnerHTML(R"HTML(
    <style>
      html { height: 1000px; width: 1000px; color-scheme: light; }
      .container { overflow: scroll; width: 100px; height: 100px; }
      .scrollable { height: 400px; width: 400px; }
    </style>
    <div id="normal" class="container">
      <div class="scrollable"></div>
    </div>
  )HTML");

  AssertDefaultPreferredColorSchemes();

  const auto* root_scrollable_area = GetLayoutView().GetScrollableArea();
  ASSERT_TRUE(root_scrollable_area);
  const auto* non_root_scrollable_area =
      GetPaintLayerByElementId("normal")->GetScrollableArea();
  ASSERT_TRUE(non_root_scrollable_area);

  ColorSchemeHelper color_scheme_helper(GetDocument());
  SetPreferredColorSchemesToDark(color_scheme_helper);
  UpdateAllLifecyclePhasesForTest();

  // Set root element's color scheme to normal.
  GetDocument().documentElement()->SetInlineStyleProperty(
      CSSPropertyID::kColorScheme, AtomicString("normal"));

  // Update lifecycle up until the pre-paint before the scrollbars paint is
  // invalidated.
  GetDocument().View()->UpdateLifecycleToCompositingInputsClean(
      DocumentUpdateReason::kTest);

  // Root scrollbars should be set for invalidation after the color scheme
  // change.
  ExpectEqAllScrollControlsNeedPaintInvalidation(root_scrollable_area, true);

  // Non root scrollbars should not change.
  ExpectEqAllScrollControlsNeedPaintInvalidation(non_root_scrollable_area,
                                                 false);

  EXPECT_EQ(root_scrollable_area->UsedColorSchemeScrollbars(),
            mojom::blink::ColorScheme::kDark);
}

TEST_P(PaintLayerScrollableAreaTest,
       UsedColorSchemeRootScrollbarsUseCounterUpdated) {
  USE_NON_OVERLAY_SCROLLBARS_OR_QUIT();

  SetHtmlInnerHTML(R"HTML(
    <style>
      :root { height: 1000px; }
    </style>
  )HTML");

  AssertDefaultPreferredColorSchemes();

  const auto* root_scrollable_area = GetLayoutView().GetScrollableArea();
  ASSERT_TRUE(root_scrollable_area);

  ColorSchemeHelper color_scheme_helper(GetDocument());
  SetPreferredColorSchemesToDark(color_scheme_helper);
  UpdateAllLifecyclePhasesForTest();

  root_scrollable_area->UsedColorSchemeScrollbars();
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kUsedColorSchemeRootScrollbarsDark));
}

// TODO(crbug.com/1020913): Actually this tests a situation that should not
// exist but it does exist due to different or incorrect rounding methods for
// scroll geometries. This test can be converted to test the correct behavior
// when we fix the bug. For now it just ensures we won't crash.
TEST_P(PaintLayerScrollableAreaTest,
       NotScrollsOverflowWithScrollableScrollbar) {
  USE_NON_OVERLAY_SCROLLBARS_OR_QUIT();

  SetBodyInnerHTML(R"HTML(
    <div id="scroller"
         style="box-sizing: border-box; width: 54.6px; height: 99.9px;
                padding: 20.1px; overflow: scroll; direction: rtl;
                will-change: scroll-position">
      <div style="width: 0; height: 20px"></div>
    </div>
  )HTML");

  auto* scroller = GetLayoutBoxByElementId("scroller");
  auto* scrollable_area = scroller->GetScrollableArea();
  EXPECT_FALSE(scrollable_area->ScrollsOverflow());
  ASSERT_TRUE(scrollable_area->HorizontalScrollbar());
  EXPECT_TRUE(scrollable_area->HorizontalScrollbar()->Maximum());
}

// TODO(crbug.com/340578714): The expectations match the current actual outputs
// which may not be fully correct.
TEST_P(PaintLayerScrollableAreaTest,
       ScrollbarGutterBothEdgesWithHorizontalScrollbar) {
  USE_NON_OVERLAY_SCROLLBARS_OR_QUIT();
  SetBodyInnerHTML(R"HTML(
    <div id="scroll" style="width: 100px; height: 100px; overflow: auto;
                            border: 20px solid blue; padding: 10px;
                            scrollbar-gutter: stable both-edges">
      <div style="width: 300px"></div>
    </div>
  )HTML");

  auto* scroll = GetLayoutBoxByElementId("scroll")->GetScrollableArea();
  EXPECT_EQ(PhysicalRect(0, 0, 90, 105),
            scroll->LayoutContentRect(kExcludeScrollbars));
  EXPECT_EQ(PhysicalRect(0, 0, 120, 120),
            scroll->LayoutContentRect(kIncludeScrollbars));
  EXPECT_EQ(gfx::Size(320, 105), scroll->ContentsSize());
  EXPECT_EQ(PhysicalRect(35, 20, 320, 105),
            scroll->GetLayoutBox()->ScrollableOverflowRect());
  EXPECT_EQ(PhysicalRect(20, 20, 120, 105),
            scroll->GetLayoutBox()->OverflowClipRect(PhysicalOffset()));
  EXPECT_EQ(gfx::Point(), scroll->ScrollOrigin());
}

// TODO(crbug.com/340578714): The expectations match the current actual outputs
// which may not be fully correct.
TEST_P(PaintLayerScrollableAreaTest,
       ScrollbarGutterBothEdgesWithVerticalScrollbars) {
  USE_NON_OVERLAY_SCROLLBARS_OR_QUIT();
  SetBodyInnerHTML(R"HTML(
    <div id="scroll" style="width: 100px; height: 100px; overflow: auto;
                            border: 20px solid blue; padding: 10px;
                            scrollbar-gutter: stable both-edges">
      <div style="height: 300px"></div>
    </div>
  )HTML");

  auto* scroll = GetLayoutBoxByElementId("scroll")->GetScrollableArea();
  EXPECT_EQ(PhysicalRect(0, 0, 90, 120),
            scroll->LayoutContentRect(kExcludeScrollbars));
  EXPECT_EQ(PhysicalRect(0, 0, 120, 120),
            scroll->LayoutContentRect(kIncludeScrollbars));
  EXPECT_EQ(gfx::Size(90, 320), scroll->ContentsSize());
  EXPECT_EQ(PhysicalRect(35, 20, 90, 320),
            scroll->GetLayoutBox()->ScrollableOverflowRect());
  EXPECT_EQ(PhysicalRect(20, 20, 105, 120),
            scroll->GetLayoutBox()->OverflowClipRect(PhysicalOffset()));
  EXPECT_EQ(gfx::Point(), scroll->ScrollOrigin());
}

// TODO(crbug.com/340578714): The expectations match the current actual outputs
// which may not be fully correct.
TEST_P(PaintLayerScrollableAreaTest,
       ScrollbarGutterBothEdgesWithBothScrollbars) {
  USE_NON_OVERLAY_SCROLLBARS_OR_QUIT();
  SetBodyInnerHTML(R"HTML(
    <div id="scroll" style="width: 100px; height: 100px; overflow: auto;
                            border: 20px solid blue; padding: 10px;
                            scrollbar-gutter: stable both-edges">
      <div style="width: 300px; height: 300px"></div>
    </div>
  )HTML");

  auto* scroll = GetLayoutBoxByElementId("scroll")->GetScrollableArea();
  EXPECT_EQ(PhysicalRect(0, 0, 90, 105),
            scroll->LayoutContentRect(kExcludeScrollbars));
  EXPECT_EQ(PhysicalRect(0, 0, 120, 120),
            scroll->LayoutContentRect(kIncludeScrollbars));
  EXPECT_EQ(gfx::Size(320, 320), scroll->ContentsSize());
  EXPECT_EQ(PhysicalRect(35, 20, 320, 320),
            scroll->GetLayoutBox()->ScrollableOverflowRect());
  EXPECT_EQ(PhysicalRect(20, 20, 105, 105),
            scroll->GetLayoutBox()->OverflowClipRect(PhysicalOffset()));
  EXPECT_EQ(gfx::Point(), scroll->ScrollOrigin());
}

// TODO(crbug.com/340578714): The expectations match the current actual outputs
// which may not be fully correct.
TEST_P(PaintLayerScrollableAreaTest,
       ScrollbarGutterBothEdgesOverflowIntoGutter) {
  USE_NON_OVERLAY_SCROLLBARS_OR_QUIT();
  SetBodyInnerHTML(R"HTML(
    <div id="scroll" style="width: 100px; height: 100px; overflow: auto;
                            border: 20px solid blue; padding: 10px;
                            scrollbar-gutter: stable both-edges">
      <div style="position: relative; left: -15px; width: 100px"></div>
    </div>
  )HTML");

  auto* scroll = GetLayoutBoxByElementId("scroll")->GetScrollableArea();
  EXPECT_EQ(PhysicalRect(0, 0, 90, 120),
            scroll->LayoutContentRect(kExcludeScrollbars));
  EXPECT_EQ(PhysicalRect(0, 0, 120, 120),
            scroll->LayoutContentRect(kIncludeScrollbars));
  EXPECT_EQ(gfx::Size(120, 120), scroll->ContentsSize());
  EXPECT_EQ(PhysicalRect(35, 20, 120, 120),
            scroll->GetLayoutBox()->ScrollableOverflowRect());
  EXPECT_EQ(PhysicalRect(20, 20, 120, 120),
            scroll->GetLayoutBox()->OverflowClipRect(PhysicalOffset()));
  EXPECT_EQ(gfx::Point(), scroll->ScrollOrigin());
}

// TODO(crbug.com/340578714): The expectations match the current actual outputs
// which may not be fully correct.
TEST_P(PaintLayerScrollableAreaTest,
       ScrollbarGutterBothEdgesRtlWithHorizontalScrollbar) {
  USE_NON_OVERLAY_SCROLLBARS_OR_QUIT();
  SetBodyInnerHTML(R"HTML(
    <div id="scroll" style="width: 100px; height: 100px; overflow: auto;
                            border: 20px solid blue; padding: 10px;
                            scrollbar-gutter: stable both-edges;
                            direction: rtl">
      <div style="width: 300px"></div>
    </div>
  )HTML");

  auto* scroll = GetLayoutBoxByElementId("scroll")->GetScrollableArea();
  EXPECT_EQ(PhysicalRect(200, 0, 90, 105),
            scroll->LayoutContentRect(kExcludeScrollbars));
  EXPECT_EQ(PhysicalRect(200, 0, 120, 120),
            scroll->LayoutContentRect(kIncludeScrollbars));
  EXPECT_EQ(gfx::Size(320, 105), scroll->ContentsSize());
  EXPECT_EQ(PhysicalRect(-195, 20, 320, 105),
            scroll->GetLayoutBox()->ScrollableOverflowRect());
  EXPECT_EQ(PhysicalRect(20, 20, 120, 105),
            scroll->GetLayoutBox()->OverflowClipRect(PhysicalOffset()));
  EXPECT_EQ(gfx::Point(230, 0), scroll->ScrollOrigin());
}

// TODO(crbug.com/340578714): The expectations match the current actual outputs
// which may not be fully correct.
TEST_P(PaintLayerScrollableAreaTest,
       ScrollbarGutterBothEdgesRtlWithVerticalScrollbar) {
  USE_NON_OVERLAY_SCROLLBARS_OR_QUIT();
  SetBodyInnerHTML(R"HTML(
    <div id="scroll" style="width: 100px; height: 100px; overflow: auto;
                            border: 20px solid blue; padding: 10px;
                            scrollbar-gutter: stable both-edges;
                            direction: rtl">
      <div style="height: 300px"></div>
    </div>
  )HTML");

  auto* scroll = GetLayoutBoxByElementId("scroll")->GetScrollableArea();
  EXPECT_EQ(PhysicalRect(0, 0, 90, 120),
            scroll->LayoutContentRect(kExcludeScrollbars));
  EXPECT_EQ(PhysicalRect(0, 0, 120, 120),
            scroll->LayoutContentRect(kIncludeScrollbars));
  EXPECT_EQ(gfx::Size(90, 320), scroll->ContentsSize());
  EXPECT_EQ(PhysicalRect(35, 20, 90, 320),
            scroll->GetLayoutBox()->ScrollableOverflowRect());
  EXPECT_EQ(PhysicalRect(35, 20, 105, 120),
            scroll->GetLayoutBox()->OverflowClipRect(PhysicalOffset()));
  EXPECT_EQ(gfx::Point(0, 0), scroll->ScrollOrigin());
}

// TODO(crbug.com/340578714): The expectations match the current actual outputs
// which may not be fully correct.
TEST_P(PaintLayerScrollableAreaTest,
       ScrollbarGutterBothEdgesRtlWithBothScrollbars) {
  USE_NON_OVERLAY_SCROLLBARS_OR_QUIT();
  SetBodyInnerHTML(R"HTML(
    <div id="scroll" style="width: 100px; height: 100px; overflow: auto;
                            border: 20px solid blue; padding: 10px;
                            scrollbar-gutter: stable both-edges;
                            direction: rtl">
      <div style="width: 300px; height: 300px"></div>
    </div>
  )HTML");

  auto* scroll = GetLayoutBoxByElementId("scroll")->GetScrollableArea();
  EXPECT_EQ(PhysicalRect(215, 0, 90, 105),
            scroll->LayoutContentRect(kExcludeScrollbars));
  EXPECT_EQ(PhysicalRect(215, 0, 120, 120),
            scroll->LayoutContentRect(kIncludeScrollbars));
  EXPECT_EQ(gfx::Size(320, 320), scroll->ContentsSize());
  EXPECT_EQ(PhysicalRect(-195, 20, 320, 320),
            scroll->GetLayoutBox()->ScrollableOverflowRect());
  EXPECT_EQ(PhysicalRect(35, 20, 105, 105),
            scroll->GetLayoutBox()->OverflowClipRect(PhysicalOffset()));
  EXPECT_EQ(gfx::Point(230, 0), scroll->ScrollOrigin());
}

class PaintLayerScrollableAreaWithWebFrameTest : public ::testing::Test {
 public:
  void SetUp() override { web_view_helper_.Initialize(); }
  void TearDown() override { web_view_helper_.Reset(); }

  Document& GetDocument() {
    return *web_view_helper_.LocalMainFrame()->GetFrame()->GetDocument();
  }

 private:
  test::TaskEnvironment task_environment;
  frame_test_helpers::WebViewHelper web_view_helper_;
};

// This test needs a WebLocalFrame for accurate main thread scrolling reasons.
// Otherwise we'll force main-thread scrolling for reason kPopupNoThreadedInput
// because threaded scrolling is not possible without a WebLocalFrame.
TEST_F(PaintLayerScrollableAreaWithWebFrameTest,
       UpdateShouldAnimateScrollOnMainThread) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <div id="scroller"
         style="width: 100px; height: 100px; background: red; overflow: hidden">
      <div style="height: 2000px"></div>
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  auto* scroller = GetDocument().getElementById(AtomicString("scroller"));
  auto* box = scroller->GetLayoutBox();
  auto* scrollable_area = box->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  EXPECT_TRUE(scrollable_area->ShouldScrollOnMainThread());
  EXPECT_FALSE(box->FirstFragment().PaintProperties()->Scroll());

  scroller->SetInlineStyleProperty(CSSPropertyID::kOverflow, CSSValueID::kAuto);
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(scrollable_area->ShouldScrollOnMainThread());
  EXPECT_TRUE(box->FirstFragment().PaintProperties()->Scroll());

  scroller->SetInlineStyleProperty(CSSPropertyID::kOverflow,
                                   CSSValueID::kHidden);
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(scrollable_area->ShouldScrollOnMainThread());
  EXPECT_FALSE(box->FirstFragment().PaintProperties()->Scroll());

  scroller->scrollTo(0, 200);
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(scrollable_area->ShouldScrollOnMainThread());
  if (RuntimeEnabledFeatures::ScrollNodeForOverflowHiddenEnabled()) {
    EXPECT_TRUE(box->FirstFragment().PaintProperties()->Scroll());
  } else {
    EXPECT_FALSE(box->FirstFragment().PaintProperties()->Scroll());
  }
}

}  // namespace blink
