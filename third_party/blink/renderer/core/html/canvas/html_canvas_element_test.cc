// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"

#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

using HTMLCanvasElementTest = RenderingTest;

TEST_F(HTMLCanvasElementTest, CreateLayerUpdatesCompositing) {
  // Enable script so that the canvas will create a LayoutHTMLCanvas.
  GetDocument().GetSettings()->SetScriptEnabled(true);

  SetBodyInnerHTML("<canvas id='canvas'></canvas>");
  auto* canvas = ToHTMLCanvasElement(GetDocument().getElementById("canvas"));
  auto* layer = ToLayoutBoxModelObject(canvas->GetLayoutObject())->Layer();
  ASSERT_TRUE(layer);
  EXPECT_EQ(CompositingReason::kNone, layer->DirectCompositingReasons());

  EXPECT_FALSE(layer->NeedsCompositingInputsUpdate());
  canvas->CreateLayer();
  EXPECT_TRUE(layer->NeedsCompositingInputsUpdate());
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(layer, ToLayoutBoxModelObject(canvas->GetLayoutObject())->Layer());
  EXPECT_EQ(CompositingReason::kCanvas, layer->DirectCompositingReasons());
}

}  // namespace blink
