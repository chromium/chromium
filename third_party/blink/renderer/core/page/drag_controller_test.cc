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
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/page/autoscroll_controller.h"
#include "third_party/blink/renderer/core/page/drag_data.h"
#include "third_party/blink/renderer/core/page/drag_image.h"
#include "third_party/blink/renderer/core/page/drag_state.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/graphics/bitmap_image.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

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

// PopulateDragDataTransfer() must always be called before StartDrag() because
// StartDrag() relies on state (the drag-origin hit test result and drag
// overlay) that PopulateDragDataTransfer() caches.
static bool PopulateAndStartDrag(LocalFrame& frame,
                                 DragState& drag_state,
                                 const WebMouseEvent& mouse_event,
                                 const gfx::Point& drag_initiation_location) {
  DragController& drag_controller = frame.GetPage()->GetDragController();
  if (!drag_controller.PopulateDragDataTransfer(
          &frame, drag_state, drag_initiation_location,
          gfx::ToFlooredPoint(mouse_event.PositionInRootFrame()))) {
    return false;
  }
  return drag_controller.StartDrag(&frame, drag_state, mouse_event,
                                   drag_initiation_location);
}

class DragControllerTest : public RenderingTest {
 protected:
  DragControllerTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()),

        chrome_client_(MakeGarbageCollected<DragMockChromeClient>()) {}
  LocalFrame& GetFrame() const { return *GetDocument().GetFrame(); }
  DragMockChromeClient& GetChromeClient() const override {
    return *chrome_client_;
  }
  void PerformDragAndDropFromTextareaToTargetElement(
      HTMLTextAreaElement* drag_text_area,
      DataObject* data_object,
      Element* drop_target) {
    const gfx::PointF drag_client_point(drag_text_area->OffsetLeft(),
                                        drag_text_area->OffsetTop());
    const gfx::PointF drop_client_point(drop_target->OffsetLeft(),
                                        drop_target->OffsetTop());

    WebMouseEvent mouse_event(WebInputEvent::Type::kMouseDown,
                              WebInputEvent::kNoModifiers,
                              WebInputEvent::GetStaticTimeStampForTests());
    mouse_event.button = WebMouseEvent::Button::kLeft;
    mouse_event.SetPositionInWidget(drag_client_point);

    drag_text_area->SetValue("https://www.example.com/index.html");
    drag_text_area->Focus();
    UpdateAllLifecyclePhasesForTest();
    GetFrame().Selection().SelectAll();
    const gfx::Point drag_initiation_location(
        gfx::Point(drag_text_area->OffsetLeft(), drag_text_area->OffsetTop()));
    EXPECT_TRUE(PopulateAndStartDrag(
        GetFrame(), GetFrame().GetPage()->GetDragController().GetDragState(),
        mouse_event, drag_initiation_location));
    DragData data(data_object,
                  GetFrame().GetPage()->GetVisualViewport().ViewportToRootFrame(
                      drop_client_point),
                  drop_client_point,
                  static_cast<DragOperationsMask>(kDragOperationMove), false);
    GetFrame().GetPage()->GetDragController().DragEnteredOrUpdated(&data,
                                                                   GetFrame());
    GetFrame().GetPage()->GetDragController().PerformDrop(
        &data, GetFrame(), DragController::Operation());
  }

  // Injects a fully decoded 6 MP (3000x2000) image into an <img> and starts an
  // image drag from it, bypassing the network. Returns the drag image size
  // recorded by the mock ChromeClient so callers can assert whether a preview
  // was produced.
  gfx::Size StartDragForLargeImage() {
    SetBodyInnerHTML(R"HTML(
      <style>
        * { margin: 0; }
        img { display: block; }
      </style>
      <img id='image' src='http://test.com/large.png' width='100' height='100'>
    )HTML");

    // Build a real, decoded image larger than 2.25 MP (3000x2000 = 6 MP) and
    // inject it into the <img>. `CanDragImage()` requires a resource buffer and
    // a filename extension, both of which a file-backed `BitmapImage` provides.
    std::optional<Vector<char>> image_file = test::ReadFromFile(
        test::CoreTestDataPath("notifications/3000x2000.png"));
    EXPECT_TRUE(image_file);
    scoped_refptr<SharedBuffer> image_data =
        SharedBuffer::Create(std::move(*image_file));
    scoped_refptr<BitmapImage> bitmap_image = BitmapImage::Create();
    bitmap_image->SetData(image_data, true);
    EXPECT_GT(bitmap_image->Size().Area64(), int64_t{1500} * 1500);

    auto* image_element = To<HTMLImageElement>(
        GetDocument().getElementById(AtomicString("image")));
    EXPECT_TRUE(image_element);
    image_element->SetImageForTest(
        ImageResourceContent::CreateLoaded(bitmap_image.get()));
    UpdateAllLifecyclePhasesForTest();

    const gfx::Point drag_origin(5, 5);
    WebMouseEvent mouse_event(WebInputEvent::Type::kMouseDown,
                              WebInputEvent::kNoModifiers,
                              WebInputEvent::GetStaticTimeStampForTests());
    mouse_event.button = WebMouseEvent::Button::kLeft;
    mouse_event.SetPositionInWidget(drag_origin.x(), drag_origin.y());

    auto& drag_state = GetFrame().GetPage()->GetDragController().GetDragState();
    drag_state.drag_type_ = kDragSourceActionImage;
    drag_state.drag_src_ = image_element;
    drag_state.drag_data_transfer_ = DataTransfer::Create(
        DataTransfer::kDragAndDrop, DataTransferAccessPolicy::kWritable,
        DataObject::Create());

    // The drag must start regardless of the image's intrinsic size.
    EXPECT_TRUE(
        PopulateAndStartDrag(GetFrame(), drag_state, mouse_event, drag_origin));

    return GetChromeClient().last_drag_image_size;
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
  auto renderer_preferences = WebView().GetRendererPreferences();
  renderer_preferences.can_accept_load_drops = false;
  WebView().SetRendererPreferences(renderer_preferences);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/test.html", "text/html");

  LoadURL("https://example.com/test.html");

  // Page must be scrollable so that Autoscroll is engaged.
  main_resource.Complete(
      "<!DOCTYPE html>"
      "<style>body,html { height: 1000px; width: 1000px; }</style>");

  Compositor().BeginFrame();

  WebDragData drag_data;
  WebDragData::StringItem item;
  item.type = "text/uri-list";
  item.data = WebString("https://www.example.com/index.html");
  drag_data.AddItem(item);

  const gfx::PointF client_point(10, 10);
  const gfx::PointF screen_point(10, 10);
  WebFrameWidget* widget = WebView().MainFrameImpl()->FrameWidget();
  widget->DragTargetDragEnter(drag_data, client_point, screen_point,
                              kDragOperationCopy, 0, base::DoNothing());

  // The page should tell the AutoscrollController about the drag.
  EXPECT_TRUE(
      WebView().GetPage()->GetAutoscrollController().AutoscrollInProgress());

  widget->DragTargetDrop(drag_data, client_point, screen_point, 0,
                         base::DoNothing());
  frame_test_helpers::PumpPendingRequestsForFrameToLoad(
      WebView().MainFrameImpl());

  // Once we've "performed" the drag (in which nothing happens), the
  // AutoscrollController should have been cleared.
  EXPECT_FALSE(
      WebView().GetPage()->GetAutoscrollController().AutoscrollInProgress());
}

// Verify that conditions that prevent hit testing - such as throttled
// lifecycle updates for frames - are accounted for in the DragController.
// Regression test for https://crbug.com/685030
TEST_F(DragControllerSimTest, ThrottledDocumentHandled) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/test.html", "text/html");

  LoadURL("https://example.com/test.html");

  // Intercept event to indicate that the document will be handling the drag.
  main_resource.Complete(
      "<!DOCTYPE html>"
      "<script>"
      "  document.addEventListener('dragenter', e => e.preventDefault());"
      "</script>");

  DataObject* object = DataObject::CreateFromString("hello world");
  DragData data(
      object, gfx::PointF(10, 10), gfx::PointF(10, 10),
      static_cast<DragOperationsMask>(kDragOperationCopy | kDragOperationLink |
                                      kDragOperationMove),
      false);

  WebView().GetPage()->GetDragController().DragEnteredOrUpdated(
      &data, *GetDocument().GetFrame());

  // Throttle updates, which prevents hit testing from yielding a node.
  WebView()
      .MainFrameImpl()
      ->GetFrameView()
      ->SetLifecycleUpdatesThrottledForTesting();

  WebView().GetPage()->GetDragController().PerformDrop(
      &data, *GetDocument().GetFrame(), DragController::Operation());

  // Test passes if we don't crash.
}

// Dragging an image whose intrinsic area exceeds the resolution of a typical
// photo must still start a drag. Regression coverage for the drag still
// proceeding regardless of the source image's intrinsic size.
TEST_F(DragControllerSimTest, LargeImageStartsDrag) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest image_resource("https://example.com/big.png", "image/png");
  LoadURL("https://example.com/test.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>* { margin: 0; }</style>
    <img id="big" src="big.png">
  )HTML");

  // The 3000x2000.png test image (6 MP) is larger than an ordinary photo but
  // still passes `CanDragImage()` and produces a (down-scaled) drag preview.
  image_resource.Complete(*test::ReadFromFile(
      test::CoreTestDataPath("notifications/3000x2000.png")));
  test::RunPendingTasks();
  Compositor().BeginFrame();

  Element* image_element = GetDocument().getElementById(AtomicString("big"));
  ASSERT_TRUE(image_element);

  const gfx::Point drag_origin(50, 50);
  WebMouseEvent mouse_event(WebInputEvent::Type::kMouseDown,
                            WebInputEvent::kNoModifiers,
                            WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = WebMouseEvent::Button::kLeft;
  mouse_event.SetPositionInWidget(drag_origin.x(), drag_origin.y());

  DragController& drag_controller = WebView().GetPage()->GetDragController();
  DragState& drag_state = drag_controller.GetDragState();
  drag_state.drag_type_ = kDragSourceActionImage;
  drag_state.drag_src_ = image_element;
  drag_state.drag_data_transfer_ = DataTransfer::Create(
      DataTransfer::kDragAndDrop, DataTransferAccessPolicy::kWritable,
      DataObject::Create());

  EXPECT_TRUE(drag_controller.PopulateDragDataTransfer(
      GetDocument().GetFrame(), drag_state, drag_origin,
      gfx::ToFlooredPoint(mouse_event.PositionInRootFrame())));

  EXPECT_TRUE(drag_controller.StartDrag(GetDocument().GetFrame(), drag_state,
                                        mouse_event, drag_origin));
}

// An image drag whose source `<img>` has no image URL should not start a
// drag at all.
TEST_F(DragControllerSimTest, ImageDragWithEmptyUrlDoesNotStartDrag) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  // `<img>` with no `src` and explicit dimensions so the element is still
  // hittable. `HitTestResult::AbsoluteImageURL()` is empty in this case.
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>* { margin: 0; }</style>
    <img id="empty" width="200" height="200">
  )HTML");
  Compositor().BeginFrame();

  Element* image_element = GetDocument().getElementById(AtomicString("empty"));
  ASSERT_TRUE(image_element);

  const gfx::Point drag_origin(50, 50);
  WebMouseEvent mouse_event(WebInputEvent::Type::kMouseDown,
                            WebInputEvent::kNoModifiers,
                            WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = WebMouseEvent::Button::kLeft;
  mouse_event.SetPositionInWidget(drag_origin.x(), drag_origin.y());

  DragController& drag_controller = WebView().GetPage()->GetDragController();
  DragState& drag_state = drag_controller.GetDragState();
  drag_state.drag_type_ = kDragSourceActionImage;
  drag_state.drag_src_ = image_element;
  drag_state.drag_data_transfer_ = DataTransfer::Create(
      DataTransfer::kDragAndDrop, DataTransferAccessPolicy::kWritable,
      DataObject::Create());
  // PopulateDragDataTransfer should fail which means StartDrag should not be
  // called.
  EXPECT_FALSE(drag_controller.PopulateDragDataTransfer(
      GetDocument().GetFrame(), drag_state, drag_origin,
      gfx::ToFlooredPoint(mouse_event.PositionInRootFrame())));
}

// Verifies that a link drag whose source `<a>` has no `href` does not start a
// drag.
TEST_F(DragControllerSimTest, LinkDragWithEmptyHrefDoesNotStartDrag) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>* { margin: 0; }
           a { display: inline-block; width: 100px; height: 30px; }</style>
    <a id="link">click</a>
  )HTML");
  Compositor().BeginFrame();

  Element* link_element = GetDocument().getElementById(AtomicString("link"));
  ASSERT_TRUE(link_element);

  const gfx::Point drag_origin(10, 10);
  WebMouseEvent mouse_event(WebInputEvent::Type::kMouseDown,
                            WebInputEvent::kNoModifiers,
                            WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = WebMouseEvent::Button::kLeft;
  mouse_event.SetPositionInWidget(drag_origin.x(), drag_origin.y());

  DragController& drag_controller = WebView().GetPage()->GetDragController();
  DragState& drag_state = drag_controller.GetDragState();
  drag_state.drag_type_ = kDragSourceActionLink;
  drag_state.drag_src_ = link_element;
  drag_state.drag_data_transfer_ = DataTransfer::Create(
      DataTransfer::kDragAndDrop, DataTransferAccessPolicy::kWritable,
      DataObject::Create());

  // PopulateDragDataTransfer should fail which means StartDrag should not be
  // called.
  EXPECT_FALSE(drag_controller.PopulateDragDataTransfer(
      GetDocument().GetFrame(), drag_state, drag_origin,
      gfx::ToFlooredPoint(mouse_event.PositionInRootFrame())));
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
      ScrollOffset(0, scroll_offset), mojom::blink::ScrollType::kProgrammatic,
      cc::ScrollSourceType::kNone);
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
      ScrollOffset(0, scroll_offset), mojom::blink::ScrollType::kProgrammatic,
      cc::ScrollSourceType::kNone);
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
      ScrollOffset(0, scroll_offset), mojom::blink::ScrollType::kProgrammatic,
      cc::ScrollSourceType::kNone);
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
      ScrollOffset(0, scroll_offset), mojom::blink::ScrollType::kProgrammatic,
      cc::ScrollSourceType::kNone);
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
      mojom::blink::ScrollType::kProgrammatic, cc::ScrollSourceType::kNone);
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
      ScrollOffset(0, scroll_offset), mojom::blink::ScrollType::kProgrammatic,
      cc::ScrollSourceType::kNone);
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
      ScrollOffset(0, scroll_offset), mojom::blink::ScrollType::kProgrammatic,
      cc::ScrollSourceType::kNone);
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
      mojom::blink::ScrollType::kProgrammatic, cc::ScrollSourceType::kNone);
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
  const gfx::Point drag_initiation_location(5, 10);
  EXPECT_TRUE(PopulateAndStartDrag(GetFrame(), drag_state, mouse_event,
                                   drag_initiation_location));

  gfx::Size expected_image_size =
      gfx::Size(50 * page_scale_factor, 40 * page_scale_factor);
  EXPECT_EQ(expected_image_size, GetChromeClient().last_drag_image_size);
  // The drag image has a margin of 2px which should offset the selection
  // image by 2px from the dragged location of (5, 10).
  gfx::Vector2d expected_offset(5 * page_scale_factor,
                                (10 - 2) * page_scale_factor);
  EXPECT_EQ(expected_offset, GetChromeClient().last_cursor_offset);
}

// A large image whose intrinsic area exceeds the former 1500*1500 (2.25 MP)
// cap used to make `DragImageForImage()` return null, leaving the dragged
// image with no preview even though its data payload still transferred. The
// drag image is always scaled down to `MaxDragImageSize`, so the source
// image's intrinsic size must not suppress the preview. With the
// DragImageForLargeImages feature enabled, a large image still produces a
// non-empty drag image.
TEST_F(DragControllerTest, LargeImageProducesDragImage) {
  ScopedDragImageForLargeImagesForTest scoped_feature(true);

  // With the obsolete intrinsic-area cap removed, the large image yields a
  // (down-scaled) drag preview instead of an empty bitmap.
  EXPECT_FALSE(StartDragForLargeImage().IsEmpty());
}

// Kill-switch coverage: when the DragImageForLargeImages feature is disabled,
// the legacy intrinsic-area cap is restored, so a large image produces no drag
// preview while the drag itself still starts.
TEST_F(DragControllerTest, LargeImageProducesNoDragImageWhenFeatureDisabled) {
  ScopedDragImageForLargeImagesForTest scoped_feature(false);

  EXPECT_TRUE(StartDragForLargeImage().IsEmpty());
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
  const gfx::Point drag_initiation_location(5, 10);
  EXPECT_TRUE(PopulateAndStartDrag(GetFrame(), drag_state, mouse_event,
                                   drag_initiation_location));

  gfx::Size link_image_size = GetChromeClient().last_drag_image_size;
  // The drag link image should be a textual representation of the drag url in a
  // system font (see: DeriveDragLabelFont in drag_image.cc) and should not be
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

// Verify that drag and drop of URL from textarea to textarea drops the entire
// URL
TEST_F(DragControllerTest, DragAndDropUrlFromTextareaToTextarea) {
  SetBodyInnerHTML(R"HTML(
    <style>
    body,html { height: 1000px; width: 1000px; }
    textarea { height: 100px; width: 250px; }
    </style>
    <textarea id='drag'>httts://www.example.com/index.html</textarea>
    <textarea id='drop'></textarea>
  )HTML");
  HTMLTextAreaElement* drag_text_area = DynamicTo<HTMLTextAreaElement>(
      *(GetDocument().getElementById(AtomicString("drag"))));
  HTMLTextAreaElement* drop_text_area = DynamicTo<HTMLTextAreaElement>(
      *(GetDocument().getElementById(AtomicString("drop"))));
  WebDragData web_drag_data;
  WebDragData::StringItem item1;
  item1.type = "text/uri-list";
  item1.data = WebString("https://www.example.com/index.html");
  item1.title = "index.html";
  WebDragData::StringItem item2;
  item2.type = "text/plain";
  item2.data = "https://www.example.com/index.html";

  web_drag_data.AddItem(item1);
  web_drag_data.AddItem(item2);
  DataObject* data_object = DataObject::Create(web_drag_data);
  auto& drag_state = GetFrame().GetPage()->GetDragController().GetDragState();
  drag_state.drag_type_ = kDragSourceActionSelection;
  drag_state.drag_src_ = drag_text_area;
  drag_state.drag_data_transfer_ =
      DataTransfer::Create(DataTransfer::kDragAndDrop,
                           DataTransferAccessPolicy::kWritable, data_object);

  PerformDragAndDropFromTextareaToTargetElement(drag_text_area, data_object,
                                                drop_text_area);
  EXPECT_EQ("https://www.example.com/index.html", drop_text_area->Value());
  EXPECT_EQ("", drag_text_area->Value());  // verify drag operation is move
}

// Verify that drag and drop of URL from textarea to richly editable div adds an
// anchor element
TEST_F(DragControllerTest, DragAndDropUrlFromTextareaToRichlyEditableDiv) {
  SetBodyInnerHTML(R"HTML(
    <style>
    body,html { height: 1000px; width: 1000px; }
    textarea { height: 100px; width: 250px; }
    </style>
    <textarea id='drag'>httts://www.example.com/index.html</textarea>
    <div id='drop' contenteditable='true'></div>
  )HTML");
  HTMLTextAreaElement* drag_text_area = DynamicTo<HTMLTextAreaElement>(
      *(GetDocument().getElementById(AtomicString("drag"))));
  Element* drop_div_rich = GetDocument().getElementById(AtomicString("drop"));
  WebDragData web_drag_data;
  WebDragData::StringItem item1;
  item1.type = "text/uri-list";
  item1.data = WebString("https://www.example.com/index.html");
  item1.title = "index.html";
  WebDragData::StringItem item2;
  item2.type = "text/plain";
  item2.data = "https://www.example.com/index.html";

  web_drag_data.AddItem(item1);
  web_drag_data.AddItem(item2);
  DataObject* data_object = DataObject::Create(web_drag_data);
  auto& drag_state = GetFrame().GetPage()->GetDragController().GetDragState();
  drag_state.drag_type_ = kDragSourceActionSelection;
  drag_state.drag_src_ = drag_text_area;
  drag_state.drag_data_transfer_ =
      DataTransfer::Create(DataTransfer::kDragAndDrop,
                           DataTransferAccessPolicy::kWritable, data_object);

  PerformDragAndDropFromTextareaToTargetElement(drag_text_area, data_object,
                                                drop_div_rich);
  EXPECT_EQ("<a href=\"https://www.example.com/index.html\">index.html</a>",
            drop_div_rich->GetInnerHTMLString());
  EXPECT_EQ("", drag_text_area->Value());
}

// Verify that drag and drop of URL from textarea to plaintext-only editable div
// populates the entire URL as text
TEST_F(DragControllerTest,
       DragAndDropUrlFromTextareaToPlaintextonlyEditableDiv) {
  SetBodyInnerHTML(R"HTML(
    <style>
    body,html { height: 1000px; width: 1000px; }
    textarea { height: 100px; width: 250px; }
    </style>
    <textarea id='drag'>httts://www.example.com/index.html</textarea>
    <div id='drop' contenteditable='plaintext-only'></div>
  )HTML");
  HTMLTextAreaElement* drag_text_area = DynamicTo<HTMLTextAreaElement>(
      *(GetDocument().getElementById(AtomicString("drag"))));
  Element* drop_div_plain = GetDocument().getElementById(AtomicString("drop"));
  WebDragData web_drag_data;
  WebDragData::StringItem item1;
  item1.type = "text/uri-list";
  item1.data = WebString("https://www.example.com/index.html");
  item1.title = "index.html";
  WebDragData::StringItem item2;
  item2.type = "text/plain";
  item2.data = "https://www.example.com/index.html";

  web_drag_data.AddItem(item1);
  web_drag_data.AddItem(item2);
  DataObject* data_object = DataObject::Create(web_drag_data);
  auto& drag_state = GetFrame().GetPage()->GetDragController().GetDragState();
  drag_state.drag_type_ = kDragSourceActionSelection;
  drag_state.drag_src_ = drag_text_area;
  drag_state.drag_data_transfer_ =
      DataTransfer::Create(DataTransfer::kDragAndDrop,
                           DataTransferAccessPolicy::kWritable, data_object);

  PerformDragAndDropFromTextareaToTargetElement(drag_text_area, data_object,
                                                drop_div_plain);
  EXPECT_EQ("https://www.example.com/index.html",
            drop_div_plain->GetInnerHTMLString());
  EXPECT_EQ("", drag_text_area->Value());
}

TEST_F(DragControllerTest,
       DragAndDropUrlFromTextareaToRichlyEditableParagraph) {
  SetBodyInnerHTML(R"HTML(
    <style>
    body,html { height: 1000px; width: 1000px; }
    textarea { height: 100px; width: 250px; }
    </style>
    <textarea id='drag'>httts://www.example.com/index.html</textarea>
    <p id='drop' contenteditable='true'></p>
  )HTML");
  HTMLTextAreaElement* drag_text_area = DynamicTo<HTMLTextAreaElement>(
      *(GetDocument().getElementById(AtomicString("drag"))));
  Element* drop_paragraph_rich =
      GetDocument().getElementById(AtomicString("drop"));
  WebDragData web_drag_data;
  WebDragData::StringItem item1;
  item1.type = "text/uri-list";
  item1.data = WebString("https://www.example.com/index.html");
  item1.title = "index.html";
  WebDragData::StringItem item2;
  item2.type = "text/plain";
  item2.data = "https://www.example.com/index.html";

  web_drag_data.AddItem(item1);
  web_drag_data.AddItem(item2);
  DataObject* data_object = DataObject::Create(web_drag_data);
  auto& drag_state = GetFrame().GetPage()->GetDragController().GetDragState();
  drag_state.drag_type_ = kDragSourceActionSelection;
  drag_state.drag_src_ = drag_text_area;
  drag_state.drag_data_transfer_ =
      DataTransfer::Create(DataTransfer::kDragAndDrop,
                           DataTransferAccessPolicy::kWritable, data_object);

  PerformDragAndDropFromTextareaToTargetElement(drag_text_area, data_object,
                                                drop_paragraph_rich);
  EXPECT_EQ("<a href=\"https://www.example.com/index.html\">index.html</a>",
            drop_paragraph_rich->GetInnerHTMLString());
  EXPECT_EQ("", drag_text_area->Value());
}

TEST_F(DragControllerTest,
       DragAndDropUrlFromTextareaToPlaintextonlyEditableParagraph) {
  SetBodyInnerHTML(R"HTML(
    <style>
    body,html { height: 1000px; width: 1000px; }
    textarea { height: 100px; width: 250px; }
    </style>
    <textarea id='drag'>httts://www.example.com/index.html</textarea>
    <p id='drop' contenteditable='plaintext-only'></p>
  )HTML");
  HTMLTextAreaElement* drag_text_area = DynamicTo<HTMLTextAreaElement>(
      *(GetDocument().getElementById(AtomicString("drag"))));
  Element* drop_paragraph_plain =
      GetDocument().getElementById(AtomicString("drop"));
  WebDragData web_drag_data;
  WebDragData::StringItem item1;
  item1.type = "text/uri-list";
  item1.data = WebString("https://www.example.com/index.html");
  item1.title = "index.html";
  WebDragData::StringItem item2;
  item2.type = "text/plain";
  item2.data = "https://www.example.com/index.html";

  web_drag_data.AddItem(item1);
  web_drag_data.AddItem(item2);
  DataObject* data_object = DataObject::Create(web_drag_data);
  auto& drag_state = GetFrame().GetPage()->GetDragController().GetDragState();
  drag_state.drag_type_ = kDragSourceActionSelection;
  drag_state.drag_src_ = drag_text_area;
  drag_state.drag_data_transfer_ =
      DataTransfer::Create(DataTransfer::kDragAndDrop,
                           DataTransferAccessPolicy::kWritable, data_object);

  PerformDragAndDropFromTextareaToTargetElement(drag_text_area, data_object,
                                                drop_paragraph_plain);
  EXPECT_EQ("https://www.example.com/index.html",
            drop_paragraph_plain->GetInnerHTMLString());
  EXPECT_EQ("", drag_text_area->Value());
}

// https://issues.chromium.org/issues/379761996
TEST_F(DragControllerTest, ResumeCaretBlinkingAfterDrag) {
  SetBodyInnerHTML(R"HTML(
    <style>
    body,html { height: 1000px; width: 1000px; }
    textarea { height: 100px; width: 250px; }
    </style>
    <textarea id='drag'>httts://www.example.com/index.html</textarea>
    <p id='drop' contenteditable='plaintext-only'></p>
  )HTML");
  auto* drag_text_area = DynamicTo<HTMLTextAreaElement>(GetElementById("drag"));
  Element* drop_paragraph_plain = GetElementById("drop");
  WebDragData web_drag_data;
  WebDragData::StringItem item;
  item.type = "text/plain";
  item.data = WebString("hello");
  item.title = "index.html";
  web_drag_data.AddItem(item);

  DataObject* data_object = DataObject::Create(web_drag_data);
  DragController& drag_controller = GetPage().GetDragController();
  auto& drag_state = drag_controller.GetDragState();
  drag_state.drag_type_ = kDragSourceActionSelection;
  drag_state.drag_src_ = drag_text_area;
  drag_state.drag_data_transfer_ =
      DataTransfer::Create(DataTransfer::kDragAndDrop,
                           DataTransferAccessPolicy::kWritable, data_object);
  // The mousedown event does not trigger, manually set the caret blinking state
  // to suspended.
  Selection().SetCaretBlinkingSuspended(true);
  EXPECT_TRUE(Selection().IsCaretBlinkingSuspended());
  PerformDragAndDropFromTextareaToTargetElement(drag_text_area, data_object,
                                                drop_paragraph_plain);
  EXPECT_TRUE(Selection().IsCaretBlinkingSuspended());
  drag_controller.DragEnded();
  EXPECT_FALSE(Selection().IsCaretBlinkingSuspended());
}

}  // namespace blink
