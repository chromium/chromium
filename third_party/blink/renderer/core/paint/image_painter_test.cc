// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/image_painter.h"

#include "cc/paint/paint_op.h"
#include "cc/paint/paint_op_buffer_iterator.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"

namespace blink {

namespace {

const cc::DrawImageRectOp* FirstDrawImageRectOp(const cc::PaintRecord& record) {
  for (const cc::PaintOp& op : record) {
    if (op.GetType() == cc::PaintOpType::kDrawImageRect) {
      const auto& image_op = static_cast<const cc::DrawImageRectOp&>(op);
      return &image_op;
    } else if (op.GetType() == cc::PaintOpType::kDrawRecord) {
      const auto& record_op = static_cast<const cc::DrawRecordOp&>(op);
      if (const auto* image_op = FirstDrawImageRectOp(record_op.record)) {
        return image_op;
      }
    }
  }
  return nullptr;
}

}  // namespace

class ImagePainterSimTest : public SimTest,
                            private ScopedMockOverlayScrollbars {};

// The bitmap image codepath does not support subrect decoding and vetoes some
// optimizations if subrects are used to avoid bleeding (see:
// https://crbug.com/1404998#c12). We should prefer full draw image bounds for
// bitmap images until the bitmap src rect codepaths improve.
TEST_F(ImagePainterSimTest, ClippedBitmapSpriteSheetsUseFullBounds) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <!doctype html>
    <style>
      div {
        width: 1px;
        height: 1px;
        overflow: hidden;
        position: relative;
      }
      img {
        position: absolute;
        left: 0;
        top: -1px;
      }
    </style>
    <div>
      <!-- 2x3 image. -->
      <img src="data:image/gif;base64,R0lGODdhAgADAKEDAAAA//8AAAD/AP///ywAAAAAAgADAAACBEwkAAUAOw==">
    </div>
  )HTML");

  Compositor().BeginFrame();

  cc::PaintRecord record = GetDocument().View()->GetPaintRecord();
  const cc::DrawImageRectOp* draw_image_rect = FirstDrawImageRectOp(record);
  EXPECT_EQ(0, draw_image_rect->src.x());
  EXPECT_EQ(0, draw_image_rect->src.y());
  EXPECT_EQ(2, draw_image_rect->src.width());
  EXPECT_EQ(3, draw_image_rect->src.height());
}

}  // namespace blink
