// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/drag_controller.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/page/drag_operation.h"
#include "third_party/blink/renderer/core/clipboard/data_object.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer_access_policy.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/page/autoscroll_controller.h"
#include "third_party/blink/renderer/core/page/drag_data.h"
#include "third_party/blink/renderer/core/page/drag_image.h"
#include "third_party/blink/renderer/core/page/drag_state.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"

namespace blink {

class DragMockChromeClient : public RenderingTestChromeClient {
 public:
  DragMockChromeClient() = default;

  void StartDragging(LocalFrame*,
                     const WebDragData&,
                     DragOperationsMask,
                     const SkBitmap& drag_image,
                     const gfx::Vector2d& cursor_offset,
                     const gfx::Rect& drag_obj_rect) override {
    last_drag_image_size = gfx::Size(drag_image.width(), drag_image.height());
    last_cursor_offset = cursor_offset;
  }

  gfx::Size last_drag_image_size;
  gfx::Vector2d last_cursor_offset;
};

class DragControllerTest : public RenderingTest {
 protected:
  DragControllerTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()),

        chrome_client_(MakeGarbageCollected<DragMockChromeClient>()) {}
  LocalFrame& GetFrame() const { return *GetDocument().GetFrame(); }
  DragMockChromeClient& GetChromeClient() const override {
    return *chrome_client_;
  }

 private:
  Persistent<DragMockChromeClient> chrome_client_;
};

TEST_F(DragControllerTest, DragImageForSelectionUsesPageScaleFactor) {
  SetBodyInnerHTML(
      "<div>Hello world! This tests that the bitmap for drag image is scaled "
      "by page scale factor</div>");
  GetFrame().GetPage()->GetVisualViewport().SetScale(1);
  GetFrame().Selection().SelectAll();
  UpdateAllLifecyclePhasesForTest();
  const std::unique_ptr<DragImage> image1(
      DragController::DragImageForSelection(GetFrame(), 0.75f));
  GetFrame().GetPage()->GetVisualViewport().SetScale(2);
  GetFrame().Selection().SelectAll();
  UpdateAllLifecyclePhasesForTest();
  const std::unique_ptr<DragImage> image2(
      DragController::DragImageForSelection(GetFrame(), 0.75f));

  EXPECT_GT(image1->Size().width(), 0);
  EXPECT_GT(image1->Size().height(), 0);
  EXPECT_EQ(image1->Size().width() * 2, image2->Size().width());
  EXPECT_EQ(image1->Size().height() * 2, image2->Size().height());
}

class DragControllerSimTest : public SimTest {};

// Tests that dragging a URL onto a WebWidget that doesn't navigate on Drag and
// Drop clears out the Autoscroll state. Regression test for
// https://crbug.com/733996.
TEST_F(DragControllerSimTest, DropURLOnNonNavigatingClearsState) {
  WebView().GetPage()->GetSettings().SetNavigateOnDragDrop(false);
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/test.html", "text/html");

  LoadURL("https://example.com/test.html");

  // Page must be scrollable so that Autoscroll is engaged.
  main_resource.Complete(
      "<!DOCTYPE html>"
      "<style>body,html { height: 1000px; width: 1000px; }</style>");

  Compositor().BeginFrame();

  DataObject* object = DataObject::Create();
  object->SetURLAndTitle("https://www.example.com/index.html", "index");
  DragData data(
      object, gfx::PointF(10, 10), gfx::PointF(10, 10),
      static_cast<DragOperationsMask>(kDragOperationCopy | kDragOperationLink |
                                      kDragOperationMove));

  WebView().GetPage()->GetDragController().DragEnteredOrUpdated(
      &data, *GetDocument().GetFrame());

  // The page should tell the AutoscrollController about the drag.
  EXPECT_TRUE(
      WebView().GetPage()->GetAutoscrollController().AutoscrollInProgress());

  WebView().GetPage()->GetDragController().PerformDrag(
      &data, *GetDocument().GetFrame());

  // Once we've "performed" the drag (in which nothing happens), the
  // AutoscrollController should have been cleared.
  EXPECT_FALSE(
      WebView().GetPage()->GetAutoscrollController().AutoscrollInProgress());
}

// Verify that conditions that prevent hit testing - such as throttled
// lifecycle updates for frames - are accounted for in the DragController.
// Regression test for https://crbug.com/685030
TEST_F(DragControllerSimTest, ThrottledDocumentHandled) {
  WebView().GetPage()->GetSettings().SetNavigateOnDragDrop(false);
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/test.html", "text/html");

  LoadURL("https://example.com/test.html");

  // Intercept event to indicate that the document will be handling the drag.
  main_resource.Complete(
      "<!DOCTYPE html>"
      "<script>"
      "  document.addEventListener('dragenter', e => e.preventDefault());"
      "</script>");

  DataObject* object = DataObject::Create();
  object->SetURLAndTitle("https://www.example.com/index.html", "index");
  DragData data(
      object, gfx::PointF(10, 10), gfx::PointF(10, 10),
      static_cast<DragOperationsMask>(kDragOperationCopy | kDragOperationLink |
                                      kDragOperationMove));

  WebView().GetPage()->GetDragController().DragEnteredOrUpdated(
      &data, *GetDocument().GetFrame());

  // Throttle updates, which prevents hit testing from yielding a node.
  WebView()
      .MainFrameImpl()
      ->GetFrameView()
      ->SetLifecycleUpdatesThrottledForTesting();

  WebView().GetPage()->GetDragController().PerformDrag(
      &data, *GetDocument().GetFrame());

  // Test passes if we don't crash.
}

TEST_F(DragControllerTest, DragImageForSelectionClipsToViewport) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * { margin: 0; }
      html, body { height: 2000px; }
      div {
        width: 20px;
        height: 1000px;
        font-size: 30px;
        overflow: hidden;
        margin-top: 2px;
      }
    </style>
    <div>
      a<br>b<br>c<br>d<br>e<br>f<br>g<br>h<br>i<br>j<br>k<br>l<br>m<br>n<br>
      a<br>b<br>c<br>d<br>e<br>f<br>g<br>h<br>i<br>j<br>k<br>l<br>m<br>n<br>
      a<br>b<br>c<br>d<br>e<br>f<br>g<br>h<br>i<br>j<br>k<br>l<br>m<br>n<br>
    </div>
  )HTML");
  const int page_scale_factor = 2;
  GetFrame().GetPage()->SetPageScaleFactor(page_scale_factor);
  GetFrame().Selection().SelectAll();

  const int node_width = 20;
  const int node_height = 1000;
  const int node_margin_top = 2;
  const int viewport_height_dip = 600;
  const int viewport_height_css = viewport_height_dip / page_scale_factor;

  // The top of the node should be visible but the bottom should be outside the
  // viewport.
  gfx::RectF expected_selection(0, node_margin_top, node_width,
                                viewport_height_css - node_margin_top);
  EXPECT_EQ(expected_selection, DragController::ClippedSelection(GetFrame()));
  auto selection_image(DragController::DragImageForSelection(GetFrame(), 1));
  gfx::Size expected_image_size = gfx::ToRoundedSize(
      gfx::ScaleSize(expected_selection.size(), page_scale_factor));
  EXPECT_EQ(expected_image_size, selection_image->Size());

  // Scroll 500 css px down so the top of the node is outside the viewport.
  // Because the viewport is scaled to 300 css px tall, the bottom of the node
  // should also be outside the viewport. Therefore, the selection should cover
  // the entire viewport.
  int scroll_offset = 500;
  LocalFrameView* frame_view = GetDocument().View();
  frame_view->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, scroll_offset), mojom::blink::ScrollType::kProgrammatic);
  expected_selection = gfx::RectF(0, 0, node_width, viewport_height_css);
  EXPECT_EQ(expected_selection, DragController::ClippedSelection(GetFrame()));
  selection_image = DragController::DragImageForSelection(GetFrame(), 1);
  expected_image_size = gfx::ToRoundedSize(
      gfx::ScaleSize(expected_selection.size(), page_scale_factor));
  EXPECT_EQ(expected_image_size, selection_image->Size());

  // Scroll 800 css px down so the top of the node is outside the viewport and
  // the bottom of the node is now visible.
  scroll_offset = 800;
  frame_view->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, scroll_offset), mojom::blink::ScrollType::kProgrammatic);
  expected_selection = gfx::RectF(
      0, 0, node_width, node_height + node_margin_top - scroll_offset);
  EXPECT_EQ(expected_selection, DragController::ClippedSelection(GetFrame()));
  selection_image = DragController::DragImageForSelection(GetFrame(), 1);
  expected_image_size = gfx::ToRoundedSize(
      gfx::ScaleSize(expected_selection.size(), page_scale_factor));
  EXPECT_EQ(expected_image_size, selection_image->Size());
}

TEST_F(DragControllerTest, DragImageForSelectionClipsChildFrameToViewport) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * { margin: 0; }
      html, body { height: 2000px; }
      iframe {
        margin-top: 200px;
        border: none;
        width: 50px;
        height: 50px;
      }
    </style>
    <iframe></iframe>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <style>
      * { margin: 0; }
      html, body { height: 2000px; }
      div {
        width: 30px;
        height: 20px;
        font-size: 30px;
        overflow: hidden;
        margin-top: 5px;
        margin-bottom: 500px;
      }
    </style>
    <div>abcdefg</div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  auto& child_frame = *To<LocalFrame>(GetFrame().Tree().FirstChild());
  child_frame.Selection().SelectAll();

  // The iframe's selection rect is in the frame's local coordinates and should
  // not include the iframe's margin.
  gfx::RectF expected_selection(0, 5, 30, 20);
  EXPECT_EQ(expected_selection, DragController::ClippedSelection(child_frame));
  auto selection_image(DragController::DragImageForSelection(child_frame, 1));
  gfx::Size expected_image_size = gfx::ToRoundedSize(expected_selection.size());
  EXPECT_EQ(expected_image_size, selection_image->Size());

  // The iframe's selection rect is in the frame's local coordinates and should
  // not include scroll offset.
  int scroll_offset = 50;
  LocalFrameView* frame_view = GetDocument().View();
  frame_view->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, scroll_offset), mojom::blink::ScrollType::kProgrammatic);
  expected_selection = gfx::RectF(0, 5, 30, 20);
  EXPECT_EQ(expected_selection, DragController::ClippedSelection(child_frame));
  selection_image = DragController::DragImageForSelection(child_frame, 1);
  expected_image_size = gfx::ToRoundedSize(expected_selection.size());
  EXPECT_EQ(expected_image_size, selection_image->Size());

  // The parent frame's scroll offset of 210 should cause the iframe content to
  // be shifted which should cause the iframe's selection rect to be clipped by
  // the visual viewport.
  scroll_offset = 210;
  frame_view->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, scroll_offset), mojom::blink::ScrollType::kProgrammatic);
  expected_selection = gfx::RectF(0, 10, 30, 15);
  EXPECT_EQ(expected_selection, DragController::ClippedSelection(child_frame));
  selection_image = DragController::DragImageForSelection(child_frame, 1);
  expected_image_size = gfx::ToRoundedSize(expected_selection.size());
  EXPECT_EQ(expected_image_size, selection_image->Size());

  // Scrolling the iframe should shift the content so it is further under the
  // visual viewport clip.
  int iframe_scroll_offset = 7;
  child_frame.View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, iframe_scroll_offset),
      mojom::blink::ScrollType::kProgrammatic);
  expected_selection = gfx::RectF(0, 10, 30, 8);
  EXPECT_EQ(expected_selection, DragController::ClippedSelection(child_frame));
  selection_image = DragController::DragImageForSelection(child_frame, 1);
  expected_image_size = gfx::ToRoundedSize(expected_selection.size());
  EXPECT_EQ(expected_image_size, selection_image->Size());
}

TEST_F(DragControllerTest,
       DragImageForSelectionClipsChildFrameToViewportWithPageScaleFactor) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * { margin: 0; }
      html, body { height: 2000px; }
      iframe {
        margin-top: 200px;
        border: none;
        width: 50px;
        height: 50px;
      }
    </style>
    <iframe></iframe>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <style>
      * { margin: 0; }
      html, body { height: 2000px; }
      div {
        width: 30px;
        height: 20px;
        font-size: 30px;
        overflow: hidden;
        margin-top: 5px;
        margin-bottom: 500px;
      }
    </style>
    <div>abcdefg</div>
  )HTML");
  const int page_scale_factor = 2;
  GetFrame().GetPage()->SetPageScaleFactor(page_scale_factor);
  UpdateAllLifecyclePhasesForTest();
  auto& child_frame = *To<LocalFrame>(GetFrame().Tree().FirstChild());
  child_frame.Selection().SelectAll();

  // The iframe's selection rect is in the frame's local coordinates and should
  // not include the iframe's margin.
  gfx::RectF expected_selection(0, 5, 30, 20);
  EXPECT_EQ(expected_selection, DragController::ClippedSelection(child_frame));
  auto selection_image(DragController::DragImageForSelection(child_frame, 1));
  gfx::Size expected_image_size = gfx::ToRoundedSize(
      gfx::ScaleSize(expected_selection.size(), page_scale_factor));
  EXPECT_EQ(expected_image_size, selection_image->Size());

  // The iframe's selection rect is in the frame's local coordinates and should
  // not include the parent frame's scroll offset.
  int scroll_offset = 50;
  LocalFrameView* frame_view = GetDocument().View();
  frame_view->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, scroll_offset), mojom::blink::ScrollType::kProgrammatic);
  expected_selection = gfx::RectF(0, 5, 30, 20);
  EXPECT_EQ(expected_selection, DragController::ClippedSelection(child_frame));
  selection_image = DragController::DragImageForSelection(child_frame, 1);
  expected_image_size = gfx::ToRoundedSize(
      gfx::ScaleSize(expected_selection.size(), page_scale_factor));
  EXPECT_EQ(expected_image_size, selection_image->Size());

  // The parent frame's scroll offset of 210 should cause the iframe content to
  // be shifted which should cause the iframe's selection rect to be clipped by
  // the visual viewport.
  scroll_offset = 210;
  frame_view->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, scroll_offset), mojom::blink::ScrollType::kProgrammatic);
  expected_selection = gfx::RectF(0, 10, 30, 15);
  EXPECT_EQ(expected_selection, DragController::ClippedSelection(child_frame));
  selection_image = DragController::DragImageForSelection(child_frame, 1);
  expected_image_size = gfx::ToRoundedSize(
      gfx::ScaleSize(expected_selection.size(), page_scale_factor));
  EXPECT_EQ(expected_image_size, selection_image->Size());

  // Scrolling the iframe should shift the content so it is further under the
  // visual viewport clip.
  int iframe_scroll_offset = 7;
  child_frame.View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, iframe_scroll_offset),
      mojom::blink::ScrollType::kProgrammatic);
  expected_selection = gfx::RectF(0, 10, 30, 8);
  EXPECT_EQ(expected_selection, DragController::ClippedSelection(child_frame));
  selection_image = DragController::DragImageForSelection(child_frame, 1);
  expected_image_size = gfx::ToRoundedSize(
      gfx::ScaleSize(expected_selection.size(), page_scale_factor));
  EXPECT_EQ(expected_image_size, selection_image->Size());
}

TEST_F(DragControllerTest, DragImageOffsetWithPageScaleFactor) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * { margin: 0; }
      div {
        width: 50px;
        height: 40px;
        font-size: 30px;
        overflow: hidden;
        margin-top: 2px;
      }
    </style>
    <div id='drag'>abcdefg<br>abcdefg<br>abcdefg</div>
  )HTML");
  const int page_scale_factor = 2;
  GetFrame().GetPage()->SetPageScaleFactor(page_scale_factor);
  GetFrame().Selection().SelectAll();

  WebMouseEvent mouse_event(WebInputEvent::Type::kMouseDown,
                            WebInputEvent::kNoModifiers,
                            WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = WebMouseEvent::Button::kRight;
  mouse_event.SetPositionInWidget(5, 10);

  auto& drag_state = GetFrame().GetPage()->GetDragController().GetDragState();
  drag_state.drag_type_ = kDragSourceActionSelection;
  drag_state.drag_src_ = GetDocument().getElementById(AtomicString("drag"));
  drag_state.drag_data_transfer_ = DataTransfer::Create(
      DataTransfer::kDragAndDrop, DataTransferAccessPolicy::kWritable,
      DataObject::Create());
  GetFrame().GetPage()->GetDragController().StartDrag(
      &GetFrame(), drag_state, mouse_event, gfx::Point(5, 10));

  gfx::Size expected_image_size =
      gfx::Size(50 * page_scale_factor, 40 * page_scale_factor);
  EXPECT_EQ(expected_image_size, GetChromeClient().last_drag_image_size);
  // The drag image has a margin of 2px which should offset the selection
  // image by 2px from the dragged location of (5, 10).
  gfx::Vector2d expected_offset(5 * page_scale_factor,
                                (10 - 2) * page_scale_factor);
  EXPECT_EQ(expected_offset, GetChromeClient().last_cursor_offset);
}

TEST_F(DragControllerTest, DragLinkWithPageScaleFactor) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * { margin: 0; }
      a {
        width: 50px;
        height: 40px;
        font-size: 30px;
        margin-top: 2px;
        display: block;
      }
    </style>
    <a id='drag' href='https://foobarbaz.com'>foobarbaz</a>
  )HTML");
  const int page_scale_factor = 2;
  GetFrame().GetPage()->SetPageScaleFactor(page_scale_factor);
  GetFrame().Selection().SelectAll();

  WebMouseEvent mouse_event(WebInputEvent::Type::kMouseDown,
                            WebInputEvent::kNoModifiers,
                            WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = WebMouseEvent::Button::kRight;
  mouse_event.SetFrameScale(1);
  mouse_event.SetPositionInWidget(5, 10);

  auto& drag_state = GetFrame().GetPage()->GetDragController().GetDragState();
  drag_state.drag_type_ = kDragSourceActionLink;
  drag_state.drag_src_ = GetDocument().getElementById(AtomicString("drag"));
  drag_state.drag_data_transfer_ = DataTransfer::Create(
      DataTransfer::kDragAndDrop, DataTransferAccessPolicy::kWritable,
      DataObject::Create());
  GetFrame().GetPage()->GetDragController().StartDrag(
      &GetFrame(), drag_state, mouse_event, gfx::Point(5, 10));

  gfx::Size link_image_size = GetChromeClient().last_drag_image_size;
  // The drag link image should be a textual representation of the drag url in a
  // system font (see: DragImageForLink in DragController.cpp) and should not be
  // an empty image.
  EXPECT_GT(link_image_size.Area64(), 0u);
  // Unlike the drag image in DragImageOffsetWithPageScaleFactor, the link
  // image is not offset by margin because the link image is not based on the
  // link's painting but instead is a generated image of the link's url. Because
  // link_image_size is already scaled, no additional scaling is expected.
  gfx::Vector2d expected_offset(link_image_size.width() / 2, 2);
  // The offset is mapped using integers which can introduce rounding errors
  // (see TODO in DragController::DoSystemDrag) so we accept values near our
  // expectation until more precise offset mapping is available.
  EXPECT_NEAR(expected_offset.x(), GetChromeClient().last_cursor_offset.x(), 1);
  EXPECT_NEAR(expected_offset.y(), GetChromeClient().last_cursor_offset.y(), 1);
}

}  // namespace blink
