// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"

namespace blink {

class CompositedLayerAssignerTest : public RenderingTest {
 private:
  void SetUp() override {
    EnableCompositing();
    RenderingTest::SetUp();
  }
};

TEST_F(CompositedLayerAssignerTest, SquashingSimple) {
  SetBodyInnerHTML(R"HTML(
    <div>
      <div style="width: 20px; height: 20px; will-change: transform"></div>
    </div>
    <div id="squashed" style="position: absolute; top: 0; width: 100px;
        height: 100px; background: green"></div>
    )HTML");

  PaintLayer* squashed =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("squashed"))->Layer();
  EXPECT_EQ(kPaintsIntoGroupedBacking, squashed->GetCompositingState());
}

TEST_F(CompositedLayerAssignerTest, SquashingAcrossClipPathDisallowed) {
  SetBodyInnerHTML(R"HTML(
    <div style="clip-path: circle(100%)">
      <div style="width: 20px; height: 20px; will-change: transform"></div>
    </div>
    <div id="squashed" style="position: absolute; top: 0; width: 100px;
        height: 100px; background: green"></div>
    )HTML");
  // #squashed should not be squashed after all, because of the clip path above
  // #squashing.
  PaintLayer* squashed =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("squashed"))->Layer();
  EXPECT_EQ(kPaintsIntoOwnBacking, squashed->GetCompositingState());
}

TEST_F(CompositedLayerAssignerTest, SquashingAcrossMaskDisallowed) {
  SetBodyInnerHTML(R"HTML(
    <div style="-webkit-mask-image: linear-gradient(black, white);">
      <div style="width: 20px; height: 20px; will-change: transform"></div>
    </div>
    <div id="squashed" style="position: absolute; top: 0; width: 100px;
        height: 100px; background: green"></div>
    )HTML");
  // #squashed should not be squashed after all, because of the mask above
  // #squashing.
  PaintLayer* squashed =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("squashed"))->Layer();
  EXPECT_EQ(kPaintsIntoOwnBacking, squashed->GetCompositingState());
}

TEST_F(CompositedLayerAssignerTest,
       SquashingAcrossLayoutContainmentDisallowed) {
  SetBodyInnerHTML(R"HTML(
    <div style="contain: layout">
      <div style="width: 20px; height: 20px; will-change: transform"></div>
    </div>
    <div id="squashed" style="position: absolute; top: 0; width: 100px;
        height: 100px; background: green"></div>
    )HTML");
  // #squashed should not be squashed after all, because of 'contain: layout' on
  // #squashing.
  PaintLayer* squashed =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("squashed"))->Layer();
  EXPECT_EQ(kPaintsIntoOwnBacking, squashed->GetCompositingState());
}

TEST_F(CompositedLayerAssignerTest,
       SquashingAcrossSelfLayoutContainmentDisallowed) {
  SetBodyInnerHTML(R"HTML(
    <div style="width: 20px; height: 20px; will-change: transform"></div>
    <div id="squashed" style="contain: layout; position: absolute; top: 0; width: 100px;
        height: 100px; background: green"></div>
    )HTML");
  // #squashed should not be squashed after all, because of 'contain: layout' on
  // #squahed.
  PaintLayer* squashed =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("squashed"))->Layer();
  EXPECT_EQ(kPaintsIntoOwnBacking, squashed->GetCompositingState());
}

}  // namespace blink
