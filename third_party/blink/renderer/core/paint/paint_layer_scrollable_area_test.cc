// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/scroll/scroll_types.h"

using testing::_;

namespace blink {
namespace {

class ScrollableAreaMockChromeClient : public EmptyChromeClient {
 public:
  MOCK_METHOD3(MockSetToolTip, void(LocalFrame*, const String&, TextDirection));
  void SetToolTip(LocalFrame& frame,
                  const String& tooltip_text,
                  TextDirection dir) override {
    MockSetToolTip(&frame, tooltip_text, dir);
  }
};

}  // namespace {

class PaintLayerScrollableAreaTest : public RenderingTest {
 public:
  PaintLayerScrollableAreaTest()
      : RenderingTest(EmptyLocalFrameClient::Create()),
        chrome_client_(new ScrollableAreaMockChromeClient) {}

  ~PaintLayerScrollableAreaTest() override {
    testing::Mock::VerifyAndClearExpectations(&GetChromeClient());
  }

  ScrollableAreaMockChromeClient& GetChromeClient() const override {
    return *chrome_client_;
  }

  BackgroundPaintLocation GetBackgroundPaintLocation(const char* element_id) {
    return ToLayoutBoxModelObject(GetLayoutObjectByElementId(element_id))
        ->GetBackgroundPaintLocation();
  }

 private:
  void SetUp() override {
    RenderingTest::SetUp();
    EnableCompositing();
  }

  Persistent<ScrollableAreaMockChromeClient> chrome_client_;
};

TEST_F(PaintLayerScrollableAreaTest,
       CanPaintBackgroundOntoScrollingContentsLayer) {
  GetDocument().GetFrame()->GetSettings()->SetPreferCompositingToLCDTextEnabled(
      true);
  SetBodyInnerHTML(R"HTML(
    <style>
      .scroller {
        overflow: scroll;
        will-change: transform;
        width: 300px;
        height: 300px;
      }
      .spacer { height: 1000px; }
      #scroller13::-webkit-scrollbar {
        width: 13px;
        height: 13px;
      }
    </style>
    <div id='scroller1' class='scroller' style='background: white local;'>
      <div id='negative-composited-child' style='background-color: red;
          width: 1px; height: 1px; position: absolute;
          backface-visibility: hidden; z-index: -1'></div>
      <div class='spacer'></div>
    </div>
    <div id='scroller2' class='scroller' style='background: white content-box;
        padding: 10px;'>
      <div class='spacer'></div>
    </div>
    <div id='scroller3' class='scroller'
        style='background: white local content-box; padding: 10px;'>
      <div class='spacer'></div>
    </div>
    <div id='scroller4' class='scroller'
        style='background: url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUg),
        white local;'>
      <div class='spacer'></div>
    </div>
    <div id='scroller5' class='scroller'
        style='background:
        url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUg) local, white local;'>
      <div class='spacer'></div>
    </div>
    <div id='scroller6' class='scroller'
        style='background: url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUg)
        local, white padding-box; padding: 10px;'>
      <div class='spacer'></div>
    </div>
    <div id='scroller7' class='scroller'
        style='background: url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUg)
        local, white content-box; padding: 10px;'>
      <div class='spacer'></div>
    </div>
    <div id='scroller8' class='scroller' style='background: white border-box;'>
      <div class='spacer'></div>
    </div>
    <div id='scroller9' class='scroller' style='background: white border-box;
        border: 10px solid black;'>
      <div class='spacer'></div>
    </div>
    <div id='scroller10' class='scroller' style='background: white border-box;
        border: 10px solid rgba(0, 0, 0, 0.5);'>
      <div class='spacer'></div>
    </div>
    <div id='scroller11' class='scroller'
        style='background: white content-box;'>
      <div class='spacer'></div>
    </div>
    <div id='scroller12' class='scroller' style='background: white content-box;
        padding: 10px;'>
      <div class='spacer'></div>
    </div>
    <div id='scroller13' class='scroller' style='background: white border-box;'>
      <div class='spacer'></div>
    </div>
    <div id='scroller14' class='scroller' style='background: white;
        border: 1px solid black; outline: 1px solid blue;
        outline-offset: -1px;'>
      <div class='spacer'></div>
    </div>
    <div id='scroller15' class='scroller' style='background: white;
        border: 1px solid black; outline: 1px solid blue;
        outline-offset: -2px;'>
      <div class='spacer'></div>
    </div>
    <div id='scroller16' class='scroller' style='position: absolute;
        background: white; clip: rect(0px,10px,10px,0px);'>
      <div class='spacer'></div>
    </div>
    <div id='scroller17' class='scroller'
        style='background: rgba(255, 255, 255, 0.5) border-box;
        border: 5px solid rgba(0, 0, 0, 0.5);'>
      <div class='spacer'></div>
    </div>
    <div id='scroller18' class='scroller'
        style='background: white;
        border: 5px dashed black;'>
      <div class='spacer'></div>
    </div>
  )HTML");

  // #scroller1 can paint background into scrolling contents layer even with a
  // negative z-index child.
  EXPECT_EQ(kBackgroundPaintInScrollingContents,
            GetBackgroundPaintLocation("scroller1"));

  // #scroller2 cannot paint background into scrolling contents layer because it
  // has a content-box clip without local attachment.
  EXPECT_EQ(kBackgroundPaintInGraphicsLayer,
            GetBackgroundPaintLocation("scroller2"));

  // #scroller3 can paint background into scrolling contents layer.
  EXPECT_EQ(kBackgroundPaintInScrollingContents,
            GetBackgroundPaintLocation("scroller3"));

  // #scroller4 cannot paint background into scrolling contents layer because
  // the background image is not locally attached.
  EXPECT_EQ(kBackgroundPaintInGraphicsLayer,
            GetBackgroundPaintLocation("scroller4"));

  // #scroller5 can paint background into scrolling contents layer because both
  // the image and color are locally attached.
  EXPECT_EQ(kBackgroundPaintInScrollingContents,
            GetBackgroundPaintLocation("scroller5"));

  // #scroller6 can paint background into scrolling contents layer because the
  // image is locally attached and even though the color is not, it is filled to
  // the padding box so it will be drawn the same as a locally attached
  // background.
  EXPECT_EQ(kBackgroundPaintInScrollingContents,
            GetBackgroundPaintLocation("scroller6"));

  // #scroller7 cannot paint background into scrolling contents layer because
  // the color is filled to the content box and we have padding so it is not
  // equivalent to a locally attached background.
  EXPECT_EQ(kBackgroundPaintInGraphicsLayer,
            GetBackgroundPaintLocation("scroller7"));

  // #scroller8 can paint background into scrolling contents layer because its
  // border-box is equivalent to its padding box since it has no border.
  EXPECT_EQ(kBackgroundPaintInScrollingContents,
            GetBackgroundPaintLocation("scroller8"));

  // #scroller9 can paint background into scrolling contents layer because its
  // border is opaque so it completely covers the background outside of the
  // padding-box.
  EXPECT_EQ(kBackgroundPaintInScrollingContents,
            GetBackgroundPaintLocation("scroller9"));

  // #scroller10 paints the background into both layers because its border is
  // partially transparent so the background must be drawn to the
  // border-box edges.
  EXPECT_EQ(
      kBackgroundPaintInGraphicsLayer | kBackgroundPaintInScrollingContents,
      GetBackgroundPaintLocation("scroller10"));

  // #scroller11 can paint background into scrolling contents layer because its
  // content-box is equivalent to its padding box since it has no padding.
  EXPECT_EQ(kBackgroundPaintInScrollingContents,
            GetBackgroundPaintLocation("scroller11"));

  // #scroller12 cannot paint background into scrolling contents layer because
  // it has padding so its content-box is not equivalent to its padding-box.
  EXPECT_EQ(kBackgroundPaintInGraphicsLayer,
            GetBackgroundPaintLocation("scroller12"));

  // #scroller13 paints the background into both layers because it has a custom
  // scrollbar which the background may need to draw under.
  EXPECT_EQ(
      kBackgroundPaintInGraphicsLayer | kBackgroundPaintInScrollingContents,
      GetBackgroundPaintLocation("scroller13"));

  // #scroller14 can paint background into scrolling contents layer because the
  // outline is drawn outside the padding box.
  EXPECT_EQ(kBackgroundPaintInScrollingContents,
            GetBackgroundPaintLocation("scroller14"));

  // #scroller15 can paint background into scrolling contents layer because
  // the outline is drawn into the decoration layer which will not be covered
  // up.
  EXPECT_EQ(kBackgroundPaintInScrollingContents,
            GetBackgroundPaintLocation("scroller15"));

  // #scroller16 cannot paint background into scrolling contents layer because
  // the scroller has a clip which would not be respected by the scrolling
  // contents layer.
  EXPECT_EQ(kBackgroundPaintInGraphicsLayer,
            GetBackgroundPaintLocation("scroller16"));

  // #scroller17 can only be painted once as it is translucent, and it must
  // be painted in the graphics layer to be under the translucent border.
  EXPECT_EQ(kBackgroundPaintInGraphicsLayer,
            GetBackgroundPaintLocation("scroller17"));

  // #scroller18 can be painted in both layers because the background is a
  // solid color, it must be because the dashed border reveals the background
  // underneath it.
  EXPECT_EQ(
      kBackgroundPaintInGraphicsLayer | kBackgroundPaintInScrollingContents,
      GetBackgroundPaintLocation("scroller18"));
}

TEST_F(PaintLayerScrollableAreaTest, OpaqueContainedLayersPromoted) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #scroller { overflow: scroll; height: 200px; width: 200px;
    contain: paint; background: white local content-box;
    border: 10px solid rgba(0, 255, 0, 0.5); }
    #scrolled { height: 300px; }
    </style>
    <div id="scroller"><div id="scrolled"></div></div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* scroller = GetDocument().getElementById("scroller");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_TRUE(paint_layer->NeedsCompositedScrolling());
  ASSERT_TRUE(paint_layer->GraphicsLayerBacking());
  EXPECT_TRUE(paint_layer->GraphicsLayerBacking()->ContentsOpaque());
}

// Tests that we don't promote scrolling content which would not be contained.
// Promoting the scroller would also require promoting the positioned div
// which would lose subpixel anti-aliasing due to its transparent background.
TEST_F(PaintLayerScrollableAreaTest, NonContainedLayersNotPromoted) {
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
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* scroller = GetDocument().getElementById("scroller");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_FALSE(paint_layer->NeedsCompositedScrolling());
  EXPECT_FALSE(paint_layer->GraphicsLayerBacking());
}

TEST_F(PaintLayerScrollableAreaTest, TransparentLayersNotPromoted) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #scroller { overflow: scroll; height: 200px; width: 200px; background:
    rgba(0, 255, 0, 0.5) local content-box; border: 10px solid rgba(0, 255,
    0, 0.5); contain: paint; }
    #scrolled { height: 300px; }
    </style>
    <div id="scroller"><div id="scrolled"></div></div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* scroller = GetDocument().getElementById("scroller");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_FALSE(paint_layer->NeedsCompositedScrolling());
  EXPECT_FALSE(paint_layer->GraphicsLayerBacking());
}

TEST_F(PaintLayerScrollableAreaTest, OpaqueLayersDepromotedOnStyleChange) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #scroller { overflow: scroll; height: 200px; width: 200px; background:
    white local content-box; contain: paint; }
    #scrolled { height: 300px; }
    </style>
    <div id="scroller"><div id="scrolled"></div></div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* scroller = GetDocument().getElementById("scroller");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_TRUE(paint_layer->NeedsCompositedScrolling());

  // Change the background to transparent
  scroller->setAttribute(
      HTMLNames::styleAttr,
      "background: rgba(255,255,255,0.5) local content-box;");
  GetDocument().View()->UpdateAllLifecyclePhases();
  paint_layer = ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_FALSE(paint_layer->NeedsCompositedScrolling());
  EXPECT_FALSE(paint_layer->GraphicsLayerBacking());
}

TEST_F(PaintLayerScrollableAreaTest, OpaqueLayersPromotedOnStyleChange) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #scroller { overflow: scroll; height: 200px; width: 200px; background:
    rgba(255,255,255,0.5) local content-box; contain: paint; }
    #scrolled { height: 300px; }
    </style>
    <div id="scroller"><div id="scrolled"></div></div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* scroller = GetDocument().getElementById("scroller");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_FALSE(paint_layer->NeedsCompositedScrolling());

  // Change the background to opaque
  scroller->setAttribute(HTMLNames::styleAttr,
                         "background: white local content-box;");
  GetDocument().View()->UpdateAllLifecyclePhases();
  paint_layer = ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_TRUE(paint_layer->NeedsCompositedScrolling());
  EXPECT_TRUE(paint_layer->GraphicsLayerBacking());
  ASSERT_TRUE(paint_layer->GraphicsLayerBacking());
  EXPECT_TRUE(paint_layer->GraphicsLayerBacking()->ContentsOpaque());
}

// Tests that a transform on the scroller or an ancestor will prevent promotion
// TODO(flackr): Allow integer transforms as long as all of the ancestor
// transforms are also integer.
TEST_F(PaintLayerScrollableAreaTest, OnlyNonTransformedOpaqueLayersPromoted) {
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
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* parent = GetDocument().getElementById("parent");
  Element* scroller = GetDocument().getElementById("scroller");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_TRUE(paint_layer->NeedsCompositedScrolling());
  EXPECT_TRUE(paint_layer->GraphicsLayerBacking());
  ASSERT_TRUE(paint_layer->GraphicsLayerBacking());
  EXPECT_TRUE(paint_layer->GraphicsLayerBacking()->ContentsOpaque());

  // Change the parent to have a transform.
  parent->setAttribute(HTMLNames::styleAttr, "transform: translate(1px, 0);");
  GetDocument().View()->UpdateAllLifecyclePhases();
  paint_layer = ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_FALSE(paint_layer->NeedsCompositedScrolling());
  EXPECT_FALSE(paint_layer->GraphicsLayerBacking());

  // Change the parent to have no transform again.
  parent->removeAttribute(HTMLNames::styleAttr);
  GetDocument().View()->UpdateAllLifecyclePhases();
  paint_layer = ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_TRUE(paint_layer->NeedsCompositedScrolling());
  EXPECT_TRUE(paint_layer->GraphicsLayerBacking());
  ASSERT_TRUE(paint_layer->GraphicsLayerBacking());
  EXPECT_TRUE(paint_layer->GraphicsLayerBacking()->ContentsOpaque());

  // Apply a transform to the scroller directly.
  scroller->setAttribute(HTMLNames::styleAttr, "transform: translate(1px, 0);");
  GetDocument().View()->UpdateAllLifecyclePhases();
  paint_layer = ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_FALSE(paint_layer->NeedsCompositedScrolling());
  EXPECT_FALSE(paint_layer->GraphicsLayerBacking());
}

// Test that opacity applied to the scroller or an ancestor will cause the
// scrolling contents layer to not be promoted.
TEST_F(PaintLayerScrollableAreaTest, OnlyOpaqueLayersPromoted) {
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
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* parent = GetDocument().getElementById("parent");
  Element* scroller = GetDocument().getElementById("scroller");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_TRUE(paint_layer->NeedsCompositedScrolling());
  EXPECT_TRUE(paint_layer->GraphicsLayerBacking());
  ASSERT_TRUE(paint_layer->GraphicsLayerBacking());
  EXPECT_TRUE(paint_layer->GraphicsLayerBacking()->ContentsOpaque());

  // Change the parent to be partially translucent.
  parent->setAttribute(HTMLNames::styleAttr, "opacity: 0.5;");
  GetDocument().View()->UpdateAllLifecyclePhases();
  paint_layer = ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_FALSE(paint_layer->NeedsCompositedScrolling());
  EXPECT_FALSE(paint_layer->GraphicsLayerBacking());

  // Change the parent to be opaque again.
  parent->setAttribute(HTMLNames::styleAttr, "opacity: 1;");
  GetDocument().View()->UpdateAllLifecyclePhases();
  paint_layer = ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_TRUE(paint_layer->NeedsCompositedScrolling());
  EXPECT_TRUE(paint_layer->GraphicsLayerBacking());
  ASSERT_TRUE(paint_layer->GraphicsLayerBacking());
  EXPECT_TRUE(paint_layer->GraphicsLayerBacking()->ContentsOpaque());

  // Make the scroller translucent.
  scroller->setAttribute(HTMLNames::styleAttr, "opacity: 0.5");
  GetDocument().View()->UpdateAllLifecyclePhases();
  paint_layer = ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_FALSE(paint_layer->NeedsCompositedScrolling());
  EXPECT_FALSE(paint_layer->GraphicsLayerBacking());
}

// Test that <input> elements get promoted with "will-change:transform".
TEST_F(PaintLayerScrollableAreaTest, InputElementPromotionTest) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
     .composited { will-change: transform; }
    </style>
    <input id='input' width=10 style='font-size:40pt;'/>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* element = GetDocument().getElementById("input");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  ASSERT_FALSE(paint_layer);

  element->setAttribute("class", "composited");
  GetDocument().View()->UpdateAllLifecyclePhases();
  paint_layer = ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  ASSERT_TRUE(paint_layer->HasCompositedLayerMapping());
}

// Test that <select> elements get promoted with "will-change:transform".
TEST_F(PaintLayerScrollableAreaTest, SelectElementPromotionTest) {
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
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* element = GetDocument().getElementById("select");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  // Paint layer is created on most platforms but not on all of them, e.g.
  // Android Nexus 4. It's better not to check paint_layer separately.
  ASSERT_TRUE(!paint_layer || !paint_layer->HasCompositedLayerMapping());

  element->setAttribute("class", "composited");
  GetDocument().View()->UpdateAllLifecyclePhases();
  paint_layer = ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  ASSERT_TRUE(paint_layer->HasCompositedLayerMapping());
}

// Ensure OverlayScrollbarColorTheme get updated when page load
TEST_F(PaintLayerScrollableAreaTest, OverlayScrollbarColorThemeUpdated) {
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
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* none = GetDocument().getElementById("none");
  Element* white = GetDocument().getElementById("white");
  Element* black = GetDocument().getElementById("black");

  PaintLayer* none_layer =
      ToLayoutBoxModelObject(none->GetLayoutObject())->Layer();
  PaintLayer* white_layer =
      ToLayoutBoxModelObject(white->GetLayoutObject())->Layer();
  PaintLayer* black_layer =
      ToLayoutBoxModelObject(black->GetLayoutObject())->Layer();

  ASSERT_TRUE(none_layer);
  ASSERT_TRUE(white_layer);
  ASSERT_TRUE(black_layer);

  ASSERT_EQ(ScrollbarOverlayColorTheme::kScrollbarOverlayColorThemeDark,
            none_layer->GetScrollableArea()->GetScrollbarOverlayColorTheme());
  ASSERT_EQ(ScrollbarOverlayColorTheme::kScrollbarOverlayColorThemeDark,
            white_layer->GetScrollableArea()->GetScrollbarOverlayColorTheme());
  ASSERT_EQ(ScrollbarOverlayColorTheme::kScrollbarOverlayColorThemeLight,
            black_layer->GetScrollableArea()->GetScrollbarOverlayColorTheme());
}

// Test that css clip applied to the scroller will cause the
// scrolling contents layer to not be promoted.
TEST_F(PaintLayerScrollableAreaTest,
       OnlyAutoClippedScrollingContentsLayerPromoted) {
  SetBodyInnerHTML(R"HTML(
    <style>
    .clip { clip: rect(0px,60px,50px,0px); }
    #scroller { position: absolute; overflow: auto;
    height: 100px; width: 100px; background: grey;
    will-change:transform; }
    #scrolled { height: 300px; }
    </style>
    <div id="scroller"><div id="scrolled"></div></div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* scroller = GetDocument().getElementById("scroller");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_TRUE(paint_layer->NeedsCompositedScrolling());

  // Add clip to scroller.
  scroller->setAttribute(HTMLNames::classAttr, "clip");
  GetDocument().View()->UpdateAllLifecyclePhases();
  paint_layer = ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_FALSE(paint_layer->NeedsCompositedScrolling());

  // Change the scroller to be auto clipped again.
  scroller->removeAttribute("class");
  GetDocument().View()->UpdateAllLifecyclePhases();
  paint_layer = ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_TRUE(paint_layer->NeedsCompositedScrolling());
}

TEST_F(PaintLayerScrollableAreaTest, HideTooltipWhenScrollPositionChanges) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #scroller { width: 100px; height: 100px; overflow: scroll; }
    #scrolled { height: 300px; }
    </style>
    <div id="scroller"><div id="scrolled"></div></div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* scroller = GetDocument().getElementById("scroller");
  PaintLayerScrollableArea* scrollable_area =
      ToLayoutBoxModelObject(scroller->GetLayoutObject())->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);

  EXPECT_CALL(GetChromeClient(),
              MockSetToolTip(GetDocument().GetFrame(), String(), _))
      .Times(1);
  scrollable_area->SetScrollOffset(ScrollOffset(1, 1), kUserScroll);

  // Programmatic scrolling should not dismiss the tooltip, so setToolTip
  // should not be called for this invocation.
  EXPECT_CALL(GetChromeClient(),
              MockSetToolTip(GetDocument().GetFrame(), String(), _))
      .Times(0);
  scrollable_area->SetScrollOffset(ScrollOffset(2, 2), kProgrammaticScroll);
}

TEST_F(PaintLayerScrollableAreaTest, IncludeOverlayScrollbarsInVisibleWidth) {
  ScopedOverlayScrollbarsForTest overlay_scrollbars(false);
  SetBodyInnerHTML(R"HTML(
    <style>
    #scroller { overflow: overlay; height: 100px; width: 100px; }
    #scrolled { width: 100px; height: 200px; }
    </style>
    <div id="scroller"><div id="scrolled"></div></div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* scroller = GetDocument().getElementById("scroller");
  ASSERT_TRUE(scroller);
  PaintLayerScrollableArea* scrollable_area =
      ToLayoutBoxModelObject(scroller->GetLayoutObject())->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  scrollable_area->SetScrollOffset(ScrollOffset(100, 0), kClampingScroll);
  EXPECT_EQ(scrollable_area->GetScrollOffset().Width(), 0);
}

TEST_F(PaintLayerScrollableAreaTest, ShowAutoScrollbarsForVisibleContent) {
  ScopedOverlayScrollbarsForTest overlay_scrollbars(false);
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
  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* outer_div = GetDocument().getElementById("outerDiv");
  ASSERT_TRUE(outer_div);
  outer_div->GetLayoutObject()->SetNeedsLayout("test");
  GetDocument().View()->UpdateAllLifecyclePhases();
  PaintLayerScrollableArea* scrollable_area =
      ToLayoutBoxModelObject(outer_div->GetLayoutObject())->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  EXPECT_TRUE(scrollable_area->HasVerticalScrollbar());
}

TEST_F(PaintLayerScrollableAreaTest, FloatOverflowInRtlContainer) {
  ScopedOverlayScrollbarsForTest overlay_scrollbars(false);
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
  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* container = GetDocument().getElementById("container");
  ASSERT_TRUE(container);
  PaintLayerScrollableArea* scrollable_area =
      ToLayoutBoxModelObject(container->GetLayoutObject())->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  EXPECT_FALSE(scrollable_area->HasHorizontalScrollbar());
}

TEST_F(PaintLayerScrollableAreaTest, ScrollOriginInRtlContainer) {
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
  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* container = GetDocument().getElementById("container");
  ASSERT_TRUE(container);
  PaintLayerScrollableArea* scrollable_area =
      ToLayoutBoxModelObject(container->GetLayoutObject())->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  EXPECT_EQ(scrollable_area->ScrollOrigin().X(), 100);
}

TEST_F(PaintLayerScrollableAreaTest,
       SlimmingPaintV2OverflowHiddenScrollOffsetInvalidation) {
  ScopedSlimmingPaintV2ForTest enabler(true);

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
  GetDocument().View()->UpdateAllLifecyclePhases();

  auto* scroller = GetLayoutObjectByElementId("scroller");
  auto* scrollable_area = ToLayoutBoxModelObject(scroller)->GetScrollableArea();

  const auto* properties = scroller->FirstFragment().PaintProperties();

  // No scroll offset translation is needed when scroll offset is zero.
  EXPECT_EQ(nullptr, properties->ScrollTranslation());
  EXPECT_EQ(FloatSize(0, 0), scrollable_area->GetScrollOffset());

  // Going from zero scroll offset to non-zero may require a new paint property
  // and should invalidate paint and paint properties.
  scrollable_area->SetScrollOffset(ScrollOffset(0, 1), kProgrammaticScroll);
  EXPECT_TRUE(scroller->PaintingLayer()->NeedsRepaint());
  EXPECT_TRUE(scroller->NeedsPaintPropertyUpdate());
  GetDocument().View()->UpdateAllLifecyclePhases();

  // A scroll offset translation is needed when scroll offset is non-zero.
  EXPECT_EQ(FloatSize(0, 1), scrollable_area->GetScrollOffset());
  EXPECT_NE(nullptr, properties->ScrollTranslation());

  // A property update is needed when scroll offset changes.
  scrollable_area->SetScrollOffset(ScrollOffset(0, 2), kProgrammaticScroll);
  EXPECT_TRUE(scroller->NeedsPaintPropertyUpdate());
  GetDocument().View()->UpdateAllLifecyclePhases();

  // A scroll offset translation is still needed when scroll offset is non-zero.
  EXPECT_EQ(FloatSize(0, 2), scrollable_area->GetScrollOffset());
  EXPECT_NE(nullptr, properties->ScrollTranslation());

  // Going from non-zero scroll offset to zero may require destroying a paint
  // property and should invalidate paint and paint properties.
  scrollable_area->SetScrollOffset(ScrollOffset(0, 0), kProgrammaticScroll);
  EXPECT_TRUE(scroller->PaintingLayer()->NeedsRepaint());
  EXPECT_TRUE(scroller->NeedsPaintPropertyUpdate());
  GetDocument().View()->UpdateAllLifecyclePhases();

  // No scroll offset translation is needed when scroll offset is zero.
  EXPECT_EQ(nullptr, properties->ScrollTranslation());
  EXPECT_EQ(FloatSize(0, 0), scrollable_area->GetScrollOffset());
}

TEST_F(PaintLayerScrollableAreaTest, SlimmingPaintV2ScrollDoesNotInvalidate) {
  ScopedSlimmingPaintV2ForTest enabler(true);

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
  GetDocument().View()->UpdateAllLifecyclePhases();

  auto* scroller = GetLayoutObjectByElementId("scroller");
  auto* scrollable_area = ToLayoutBoxModelObject(scroller)->GetScrollableArea();

  const auto* properties = scroller->FirstFragment().PaintProperties();
  // Scroll offset translation is needed even when scroll offset is zero.
  EXPECT_NE(nullptr, properties->ScrollTranslation());
  EXPECT_EQ(FloatSize(0, 0), scrollable_area->GetScrollOffset());

  // Changing the scroll offset should not require paint invalidation.
  scrollable_area->SetScrollOffset(ScrollOffset(0, 1), kProgrammaticScroll);
  EXPECT_FALSE(scroller->ShouldDoFullPaintInvalidation());
  EXPECT_TRUE(scroller->NeedsPaintPropertyUpdate());
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(FloatSize(0, 1), scrollable_area->GetScrollOffset());
  EXPECT_NE(nullptr, properties->ScrollTranslation());
}

TEST_F(PaintLayerScrollableAreaTest,
       SlimmingPaintV2ScrollWithLocalBackgroundAttachment) {
  ScopedSlimmingPaintV2ForTest enabler(true);

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
  GetDocument().View()->UpdateAllLifecyclePhases();

  auto* scroller = GetLayoutObjectByElementId("scroller");
  auto* scrollable_area = ToLayoutBoxModelObject(scroller)->GetScrollableArea();

  // Programmatically changing the scroll offset should require paint
  // invalidation due to background attachment.
  scrollable_area->SetScrollOffset(ScrollOffset(0, 1), kProgrammaticScroll);
  EXPECT_TRUE(scroller->ShouldDoFullPaintInvalidation());
  EXPECT_TRUE(scroller->NeedsPaintPropertyUpdate());
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(FloatSize(0, 1), scrollable_area->GetScrollOffset());
  const auto* properties = scroller->FirstFragment().PaintProperties();
  EXPECT_NE(nullptr, properties->ScrollTranslation());
}

TEST_F(PaintLayerScrollableAreaTest, HitTestOverlayScrollbars) {
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
  GetDocument().View()->UpdateAllLifecyclePhases();
  auto* scroller = GetLayoutObjectByElementId("scroller");
  auto* scrollable_area = ToLayoutBoxModelObject(scroller)->GetScrollableArea();

  scrollable_area->SetScrollbarsHiddenIfOverlay(true);

  HitTestRequest hit_request(HitTestRequest::kMove | HitTestRequest::kReadOnly);
  HitTestLocation location(LayoutPoint(95, 5));
  HitTestResult hit_result(hit_request, location);
  GetDocument().GetLayoutView()->HitTest(location, hit_result);
  EXPECT_EQ(hit_result.GetScrollbar(), nullptr);
  location = HitTestLocation(LayoutPoint(5, 95));
  hit_result = HitTestResult(hit_request, location);
  GetDocument().GetLayoutView()->HitTest(location, hit_result);
  EXPECT_EQ(hit_result.GetScrollbar(), nullptr);

  scrollable_area->SetScrollbarsHiddenIfOverlay(false);

  location = HitTestLocation(LayoutPoint(95, 5));
  hit_result = HitTestResult(hit_request, location);
  GetDocument().GetLayoutView()->HitTest(location, hit_result);
  EXPECT_EQ(hit_result.GetScrollbar(), scrollable_area->VerticalScrollbar());
  location = HitTestLocation(LayoutPoint(5, 95));
  hit_result = HitTestResult(hit_request, location);
  GetDocument().GetLayoutView()->HitTest(location, hit_result);
  EXPECT_EQ(hit_result.GetScrollbar(), scrollable_area->HorizontalScrollbar());
}

TEST_F(PaintLayerScrollableAreaTest, CompositedStickyDescendant) {
  SetBodyInnerHTML(R"HTML(
    <div id=scroller style="overflow: scroll; width: 500px; height: 300px;
        will-change: transform">
      <div id=sticky style="top: 0px; position: sticky; background: green">
      </div>
      <div style="width: 10px; height: 700px; background: lightblue"></div>
    </div>
  )HTML");
  auto* scroller =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("scroller"));
  auto* scrollable_area = scroller->GetScrollableArea();
  EXPECT_EQ(kPaintsIntoOwnBacking, scroller->Layer()->GetCompositingState());
  auto* sticky = ToLayoutBoxModelObject(GetLayoutObjectByElementId("sticky"));

  EXPECT_EQ(FloatSize(0, 0), sticky->FirstFragment()
                                 .LocalBorderBoxProperties()
                                 .Transform()
                                 ->Matrix()
                                 .To2DTranslation());

  scrollable_area->SetScrollOffset(ScrollOffset(0, 50), kUserScroll);
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_EQ(FloatSize(0, 50), sticky->FirstFragment()
                                  .LocalBorderBoxProperties()
                                  .Transform()
                                  ->Matrix()
                                  .To2DTranslation());
}

// Delayed scroll offset clamping should not crash. https://crbug.com/842495
TEST_F(PaintLayerScrollableAreaTest, IgnoreDelayedScrollOnDestroyedLayer) {
  SetBodyInnerHTML(R"HTML(
    <div id=scroller style="overflow: scroll; width: 200px; height: 200px;">
      <div style="height: 1000px;"></div>
    </div>
  )HTML");
  Element* scroller = GetDocument().getElementById("scroller");
  {
    PaintLayerScrollableArea::DelayScrollOffsetClampScope scope;
    PaintLayerScrollableArea::DelayScrollOffsetClampScope::SetNeedsClamp(
        scroller->GetLayoutBox()->GetScrollableArea());
    scroller->SetInlineStyleProperty(CSSPropertyDisplay, CSSValueNone);
    GetDocument().View()->UpdateAllLifecyclePhases();
  }
}

TEST_F(PaintLayerScrollableAreaTest, ScrollbarMaximum) {
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

  LayoutBox* scroller = ToLayoutBox(GetLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  Scrollbar* scrollbar = scrollable_area->VerticalScrollbar();

  scrollable_area->ScrollBy(ScrollOffset(0, 1000), kProgrammaticScroll);
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(scrollbar->CurrentPos(), scrollbar->Maximum());
}

}  // namespace blink
