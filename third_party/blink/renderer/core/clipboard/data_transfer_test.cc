// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/clipboard/data_transfer.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/page/drag_image.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"

namespace blink {

class DataTransferTest : public PaintTestConfigurations, public RenderingTest {
 protected:
  Page& GetPage() const { return *GetDocument().GetPage(); }
  LocalFrame& GetFrame() const { return *GetDocument().GetFrame(); }
};

INSTANTIATE_PAINT_TEST_SUITE_P(DataTransferTest);

TEST_P(DataTransferTest, NodeImage) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #sample { width: 100px; height: 100px; }
    </style>
    <div id=sample></div>
  )HTML");
  Element* sample = GetDocument().getElementById(AtomicString("sample"));
  const std::unique_ptr<DragImage> image =
      DataTransfer::NodeImage(GetFrame(), *sample);
  EXPECT_EQ(gfx::Size(100, 100), image->Size());
}

TEST_P(DataTransferTest, NodeImageWithNestedElement) {
  SetBodyInnerHTML(R"HTML(
    <style>
      div { -webkit-user-drag: element }
      span:-webkit-drag { color: #0F0 }
    </style>
    <div id=sample><span>Green when dragged</span></div>
  )HTML");
  Element* sample = GetDocument().getElementById(AtomicString("sample"));
  const std::unique_ptr<DragImage> image =
      DataTransfer::NodeImage(GetFrame(), *sample);
  EXPECT_EQ(Color::FromRGB(0, 255, 0),
            sample->firstChild()->GetLayoutObject()->ResolveColor(
                GetCSSPropertyColor()))
      << "Descendants node should have :-webkit-drag.";
}

TEST_P(DataTransferTest, NodeImageWithPsuedoClassWebKitDrag) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #sample { width: 100px; height: 100px; }
      #sample:-webkit-drag { width: 200px; height: 200px; }
    </style>
    <div id=sample></div>
  )HTML");
  Element* sample = GetDocument().getElementById(AtomicString("sample"));
  const std::unique_ptr<DragImage> image =
      DataTransfer::NodeImage(GetFrame(), *sample);
  EXPECT_EQ(gfx::Size(200, 200), image->Size())
      << ":-webkit-drag should affect dragged image.";
}

TEST_P(DataTransferTest, NodeImageWithoutDraggedLayoutObject) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #sample { width: 100px; height: 100px; }
      #sample:-webkit-drag { display:none }
    </style>
    <div id=sample></div>
  )HTML");
  Element* sample = GetDocument().getElementById(AtomicString("sample"));
  const std::unique_ptr<DragImage> image =
      DataTransfer::NodeImage(GetFrame(), *sample);
  EXPECT_EQ(nullptr, image.get()) << ":-webkit-drag blows away layout object";
}

TEST_P(DataTransferTest, NodeImageWithChangingLayoutObject) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #sample { color: blue; }
      #sample:-webkit-drag { display: inline-block; color: red; }
    </style>
    <span id=sample>foo</span>
  )HTML");
  Element* sample = GetDocument().getElementById(AtomicString("sample"));
  UpdateAllLifecyclePhasesForTest();
  LayoutObject* before_layout_object = sample->GetLayoutObject();
  const std::unique_ptr<DragImage> image =
      DataTransfer::NodeImage(GetFrame(), *sample);

  EXPECT_TRUE(sample->GetLayoutObject() != before_layout_object)
      << ":-webkit-drag causes sample to have different layout object.";
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            sample->GetLayoutObject()->ResolveColor(GetCSSPropertyColor()))
      << "#sample has :-webkit-drag.";

  // Layout w/o :-webkit-drag
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(Color::FromRGB(0, 0, 255),
            sample->GetLayoutObject()->ResolveColor(GetCSSPropertyColor()))
      << "#sample doesn't have :-webkit-drag.";
}

TEST_P(DataTransferTest, NodeImageExceedsViewportBounds) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * { margin: 0; }
      #node { width: 2000px; height: 2000px; }
    </style>
    <div id='node'></div>
  )HTML");
  Element& node = *GetDocument().getElementById(AtomicString("node"));
  const auto image = DataTransfer::NodeImage(GetFrame(), node);
  EXPECT_EQ(gfx::Size(2000, 2000), image->Size());
}

TEST_P(DataTransferTest, NodeImageUnderScrollOffset) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * { margin: 0; }
      #first { width: 500px; height: 500px; }
      #second { width: 800px; height: 900px; }
    </style>
    <div id='first'></div>
    <div id='second'></div>
  )HTML");

  const int scroll_amount = 10;
  LocalFrameView* frame_view = GetDocument().View();
  frame_view->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, scroll_amount), mojom::blink::ScrollType::kProgrammatic);

  // The first div should be offset by the scroll offset.
  Element& first = *GetDocument().getElementById(AtomicString("first"));
  const auto first_image = DataTransfer::NodeImage(GetFrame(), first);
  const int first_height = 500;
  EXPECT_EQ(gfx::Size(500, first_height), first_image->Size());

  // The second div should also be offset by the scroll offset. In addition,
  // the second div should be clipped by the viewport.
  Element& second = *GetDocument().getElementById(AtomicString("second"));
  const auto second_image = DataTransfer::NodeImage(GetFrame(), second);
  EXPECT_EQ(gfx::Size(800, 900), second_image->Size());
}

TEST_P(DataTransferTest, NodeImageSizeWithPageScaleFactor) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * { margin: 0; }
      html, body { height: 2000px; }
      #node { width: 200px; height: 141px; }
    </style>
    <div id='node'></div>
  )HTML");
  const int page_scale_factor = 2;
  GetPage().SetPageScaleFactor(page_scale_factor);
  Element& node = *GetDocument().getElementById(AtomicString("node"));
  const auto image = DataTransfer::NodeImage(GetFrame(), node);
  const int node_width = 200;
  const int node_height = 141;
  EXPECT_EQ(gfx::Size(node_width * page_scale_factor,
                      node_height * page_scale_factor),
            image->Size());

  // Check that a scroll offset is scaled to device coordinates which includes
  // page scale factor.
  const int scroll_amount = 10;
  LocalFrameView* frame_view = GetDocument().View();
  frame_view->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, scroll_amount), mojom::blink::ScrollType::kProgrammatic);
  const auto image_with_offset = DataTransfer::NodeImage(GetFrame(), node);
  EXPECT_EQ(gfx::Size(node_width * page_scale_factor,
                      node_height * page_scale_factor),
            image_with_offset->Size());
}

TEST_P(DataTransferTest, NodeImageSizeWithPageScaleFactorTooLarge) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * { margin: 0; }
      html, body { height: 2000px; }
      #node { width: 800px; height: 601px; }
    </style>
    <div id='node'></div>
  )HTML");
  const int page_scale_factor = 2;
  GetPage().SetPageScaleFactor(page_scale_factor);
  Element& node = *GetDocument().getElementById(AtomicString("node"));
  const auto image = DataTransfer::NodeImage(GetFrame(), node);
  const int node_width = 800;
  const int node_height = 601;
  EXPECT_EQ(gfx::Size(node_width * page_scale_factor,
                      node_height * page_scale_factor),
            image->Size());

  // Check that a scroll offset is scaled to device coordinates which includes
  // page scale factor.
  const int scroll_amount = 10;
  LocalFrameView* frame_view = GetDocument().View();
  frame_view->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, scroll_amount), mojom::blink::ScrollType::kProgrammatic);
  const auto image_with_offset = DataTransfer::NodeImage(GetFrame(), node);
  EXPECT_EQ(gfx::Size(node_width * page_scale_factor,
                      node_height * page_scale_factor),
            image_with_offset->Size());
}

TEST_P(DataTransferTest, NodeImageWithPageScaleFactor) {
  // #bluegreen is a 2x1 rectangle where the left pixel is blue and the right
  // pixel is green. The element is offset by a margin of 1px.
  SetBodyInnerHTML(R"HTML(
    <style>
      * { margin: 0; }
      #bluegreen {
        width: 1px;
        height: 1px;
        background: #0f0;
        border-left: 1px solid #00f;
        margin: 1px;
      }
    </style>
    <div id='bluegreen'></div>
  )HTML");
  const int page_scale_factor = 2;
  GetPage().SetPageScaleFactor(page_scale_factor);
  Element& blue_green =
      *GetDocument().getElementById(AtomicString("bluegreen"));
  const auto image = DataTransfer::NodeImage(GetFrame(), blue_green);
  const int blue_green_width = 2;
  const int blue_green_height = 1;
  EXPECT_EQ(gfx::Size(blue_green_width * page_scale_factor,
                      blue_green_height * page_scale_factor),
            image->Size());

  // Even though #bluegreen is offset by a margin of 1px (which is 2px in device
  // coordinates), we expect it to be painted at 0x0 and completely fill the 4x2
  // bitmap.
  SkBitmap expected_bitmap;
  expected_bitmap.allocN32Pixels(4, 2);
  expected_bitmap.eraseArea(SkIRect::MakeXYWH(0, 0, 2, 2), 0xFF0000FF);
  expected_bitmap.eraseArea(SkIRect::MakeXYWH(2, 0, 2, 2), 0xFF00FF00);
  const SkBitmap& bitmap = image->Bitmap();
  for (int x = 0; x < bitmap.width(); ++x)
    for (int y = 0; y < bitmap.height(); ++y)
      EXPECT_EQ(expected_bitmap.getColor(x, y), bitmap.getColor(x, y));
}

TEST_P(DataTransferTest, NodeImageFullyOffscreen) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #target {
      position: absolute;
      top: 800px;
      left: 0;
      height: 100px;
      width: 200px;
      background: lightblue;
      isolation: isolate;
    }
    </style>
    <div id="target" draggable="true" ondragstart="drag(event)"></div>
  )HTML");

  const int scroll_amount = 800;
  LocalFrameView* frame_view = GetDocument().View();
  frame_view->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, scroll_amount), mojom::blink::ScrollType::kProgrammatic);

  Element& target = *GetDocument().getElementById(AtomicString("target"));
  const auto image = DataTransfer::NodeImage(GetFrame(), target);

  EXPECT_EQ(gfx::Size(200, 100), image->Size());
}

TEST_P(DataTransferTest, NodeImageWithScrolling) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #target {
      position: absolute;
      top: 800px;
      left: 0;
      height: 100px;
      width: 200px;
      background: lightblue;
      isolation: isolate;
    }
    </style>
    <div id="target" draggable="true" ondragstart="drag(event)"></div>
  )HTML");

  Element& target = *GetDocument().getElementById(AtomicString("target"));
  const auto image = DataTransfer::NodeImage(GetFrame(), target);

  EXPECT_EQ(gfx::Size(200, 100), image->Size());
}

TEST_P(DataTransferTest, NodeImageInOffsetStackingContext) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * { margin: 0; }
      #container {
        position: absolute;
        top: 4px;
        z-index: 10;
      }
      #drag {
        width: 5px;
        height: 5px;
        background: #0F0;
      }
    </style>
    <div id="container">
      <div id="drag" draggable="true"></div>
    </div>
  )HTML");
  Element& drag = *GetDocument().getElementById(AtomicString("drag"));
  const auto image = DataTransfer::NodeImage(GetFrame(), drag);
  constexpr int drag_width = 5;
  constexpr int drag_height = 5;
  EXPECT_EQ(gfx::Size(drag_width, drag_height), image->Size());

  // The dragged image should be (drag_width x drag_height) and fully green.
  const SkBitmap& bitmap = image->Bitmap();
  for (int x = 0; x < drag_width; ++x) {
    for (int y = 0; y < drag_height; ++y)
      EXPECT_EQ(SK_ColorGREEN, bitmap.getColor(x, y));
  }
}

TEST_P(DataTransferTest, NodeImageWithLargerPositionedDescendant) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * { margin: 0; }
      #drag {
        position: absolute;
        top: 100px;
        left: 0;
        height: 1px;
        width: 1px;
        background: #00f;
      }
      #child {
        position: absolute;
        top: -1px;
        left: 0;
        height: 3px;
        width: 1px;
        background: #0f0;
      }
    </style>
    <div id="drag" draggable="true">
      <div id="child"></div>
    </div>
  )HTML");
  Element& drag = *GetDocument().getElementById(AtomicString("drag"));
  const auto image = DataTransfer::NodeImage(GetFrame(), drag);

  // The positioned #child should expand the dragged image's size.
  constexpr int drag_width = 1;
  constexpr int drag_height = 3;
  EXPECT_EQ(gfx::Size(drag_width, drag_height), image->Size());

  // The dragged image should be (drag_width x drag_height) and fully green
  // which is the color of the #child which fully covers the dragged element.
  const SkBitmap& bitmap = image->Bitmap();
  for (int x = 0; x < drag_width; ++x) {
    for (int y = 0; y < drag_height; ++y)
      EXPECT_EQ(SK_ColorGREEN, bitmap.getColor(x, y));
  }
}

TEST_P(DataTransferTest, NodeImageOutOfView) {
  SetBodyInnerHTML(R"HTML(
    <div id="drag" style="position: absolute; z-index: 1; top: -200px; left: 0;
                          width: 100px; height: 100px; background: green">
    </div>
  )HTML");

  auto image = DataTransfer::NodeImage(
      GetFrame(), *GetDocument().getElementById(AtomicString("drag")));
  EXPECT_EQ(gfx::Size(100, 100), image->Size());
  SkColor green = SkColorSetRGB(0, 0x80, 0);
  const SkBitmap& bitmap = image->Bitmap();
  for (int x = 0; x < 100; ++x) {
    for (int y = 0; y < 100; ++y)
      ASSERT_EQ(green, bitmap.getColor(x, y));
  }
}

TEST_P(DataTransferTest, NodeImageFixedChild) {
  SetBodyInnerHTML(R"HTML(
    <div id="drag" style="position: absolute; z-index: 1; top: 100px; left: 0;
                          width: 50px; height: 100px; background: green">
      <div style="position: fixed; top: 50px; width: 100px; height: 50px;
                  background: blue">
      </div>
    </div>
    <div style="height: 2000px"></div>
  )HTML");

  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 100), mojom::blink::ScrollType::kProgrammatic);

  auto image = DataTransfer::NodeImage(
      GetFrame(), *GetDocument().getElementById(AtomicString("drag")));
  EXPECT_EQ(gfx::Size(100, 100), image->Size());
  SkColor green = SkColorSetRGB(0, 0x80, 0);
  SkColor blue = SkColorSetRGB(0, 0, 0xFF);
  const SkBitmap& bitmap = image->Bitmap();
  for (int x = 0; x < 100; ++x) {
    for (int y = 0; y < 50; ++y) {
      ASSERT_EQ(x < 50 ? green : SK_ColorTRANSPARENT, bitmap.getColor(x, y));
    }
    for (int y = 50; y < 100; ++y)
      ASSERT_EQ(blue, bitmap.getColor(x, y));
  }
}

TEST_P(DataTransferTest, CreateDragImageWithEmptyImageResource) {
  DataTransfer* data_transfer = DataTransfer::Create();
  data_transfer->SetDragImageResource(
      MakeGarbageCollected<ImageResourceContent>(nullptr), gfx::Point());

  gfx::Point drag_offset;
  std::unique_ptr<DragImage> drag_image = data_transfer->CreateDragImage(
      drag_offset, /* device_scale_factor*/ 1, &GetFrame());
  // The test passes if the above call does not crash.
}

TEST_P(DataTransferTest, NodeImageTranslatedOutOfView) {
  // Given a node larger than the viewport and which is translated out of the
  // view, verify that the drag image is rendered without any clipping.
  SetBodyInnerHTML(R"HTML(
    <style>
      * { margin: 0; }
      #container {
        background: #F00;
      }
      #drag {
        width: calc(100vw + 20px);
        height: calc(100vh + 20px);
        position: absolute;
        left: calc(-150vw);
        background: #0F0;
      }
    </style>
    <div id="container">
      <div id="drag" draggable="true"></div>
    </div>
  )HTML");

  const int viewport_width = 8;
  const int viewport_height = 6;

  GetDocument().View()->Resize(viewport_width, viewport_height);
  Element& drag = *GetDocument().getElementById(AtomicString("drag"));
  const auto image = DataTransfer::NodeImage(GetFrame(), drag);

  // The drag image size should be unchanged.
  EXPECT_EQ(gfx::Size(viewport_width + 20, viewport_height + 20),
            image->Size());

  const SkBitmap& bitmap = image->Bitmap();

  // Ensure all pixels are green.
  for (int x = 0; x < viewport_width + 20; ++x) {
    for (int y = 0; y < viewport_height + 20; ++y) {
      EXPECT_EQ(SK_ColorGREEN, bitmap.getColor(x, y));
    }
  }
}

}  // namespace blink
