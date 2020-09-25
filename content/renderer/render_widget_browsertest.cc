// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "content/common/widget_messages.h"
#include "content/public/renderer/render_frame_visitor.h"
#include "content/public/test/fake_render_widget_host.h"
#include "content/public/test/render_view_test.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/render_widget.h"
#include "third_party/blink/public/common/widget/visual_properties.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_input_method_controller.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_range.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/base/ime/text_input_type.h"

namespace content {

class RenderWidgetTest : public RenderViewTest {
 protected:
  RenderWidget* widget() {
    auto* view_impl = static_cast<RenderViewImpl*>(view_);
    return view_impl->GetMainRenderFrame()->GetLocalRootRenderWidget();
  }

  gfx::Range LastCompositionRange() {
    render_widget_host_->GetWidgetInputHandler()->RequestCompositionUpdates(
        true, false);
    base::RunLoop().RunUntilIdle();
    return render_widget_host_->LastCompositionRange();
  }

  blink::WebInputMethodController* GetInputMethodController() {
    return widget()->GetInputMethodController();
  }

  void CommitText(std::string text) {
    render_widget_host_->GetWidgetInputHandler()->ImeCommitText(
        base::UTF8ToUTF16(text), std::vector<ui::ImeTextSpan>(),
        gfx::Range::InvalidRange(), 0, base::DoNothing());
    base::RunLoop().RunUntilIdle();
  }

  void SetFocus(bool focused) { GetWebWidget()->SetFocus(focused); }

  gfx::PointF GetCenterPointOfElement(const blink::WebString& id) {
    auto rect =
        GetMainFrame()->GetDocument().GetElementById(id).BoundsInViewport();
    return gfx::PointF(rect.x + rect.width / 2, rect.y + rect.height / 2);
  }

  // Returns Compositor scrolling ElementId for a given id. If id is empty it
  // returns the document scrolling ElementId.
  uint64_t GetCompositorElementId(const blink::WebString& id = "") {
    blink::WebNode node;
    if (id.IsEmpty())
      node = GetMainFrame()->GetDocument();
    else
      node = GetMainFrame()->GetDocument().GetElementById(id);
    return node.ScrollingElementIdForTesting();
  }
};

class RenderWidgetInitialSizeTest : public RenderWidgetTest {
 protected:
  blink::VisualProperties InitialVisualProperties() override {
    blink::VisualProperties initial_visual_properties;
    initial_visual_properties.new_size = initial_size_;
    initial_visual_properties.compositor_viewport_pixel_rect =
        gfx::Rect(initial_size_);
    initial_visual_properties.local_surface_id =
        local_surface_id_allocator_.GetCurrentLocalSurfaceId();
    return initial_visual_properties;
  }

  gfx::Size initial_size_ = gfx::Size(200, 100);
  viz::ParentLocalSurfaceIdAllocator local_surface_id_allocator_;
};

TEST_F(RenderWidgetTest, CompositorIdHitTestAPI) {
  LoadHTML(
      R"HTML(
      <style>
        body { padding: 0; margin: 0; }
      </style>

      <div id='green' style='background:green; height:50px; margin-top:50px;'>
      </div>

      <div id='red' style='background:red; height:50px; overflow-y:scroll'>
        <div style='height:200px'>long content</div>
      </div>

      <div id='blue' style='background:blue; height:50px; overflow:hidden'>
        <div style='height:200px'>long content</div>
      </div>

      <div style='height:50px; overflow-y:scroll'>
        <div id='yellow' style='height:50px; width:200px; position:fixed;
        background:yellow;'>position fixed</div>
        <div style='height:200px; background: black'>long content</div>
      </div>

      <div id='cyan-parent' style='height:50px; overflow:scroll'>
        <div id='cyan' style='background:cyan; height:100px; overflow:scroll'>
          <div style='height:200px'>long content</div>
        </div>
      </div>
      )HTML");

  float scale_factors[] = {1, 1.5, 2};

  for (float factor : scale_factors) {
    view_->GetWebView()->SetPageScaleFactor(factor);

    // Hit the root
    EXPECT_EQ(GetCompositorElementId(),
              widget()
                  ->GetWebWidget()
                  ->HitTestResultAt(gfx::PointF(10, 10))
                  .GetScrollableContainerId());

    // Hit non-scrollable div
    EXPECT_EQ(GetCompositorElementId(),
              widget()
                  ->GetWebWidget()
                  ->HitTestResultAt(GetCenterPointOfElement("green"))
                  .GetScrollableContainerId());

    // Hit scrollable div
    EXPECT_EQ(GetCompositorElementId("red"),
              widget()
                  ->GetWebWidget()
                  ->HitTestResultAt(GetCenterPointOfElement("red"))
                  .GetScrollableContainerId());

    // Hit overflow:hidden div
    EXPECT_EQ(GetCompositorElementId(),
              widget()
                  ->GetWebWidget()
                  ->HitTestResultAt(GetCenterPointOfElement("blue"))
                  .GetScrollableContainerId());

    // Hit position fixed div
    EXPECT_EQ(GetCompositorElementId(),
              widget()
                  ->GetWebWidget()
                  ->HitTestResultAt(GetCenterPointOfElement("yellow"))
                  .GetScrollableContainerId());

    // Hit inner scroller inside another scroller
    EXPECT_EQ(GetCompositorElementId("cyan"),
              widget()
                  ->GetWebWidget()
                  ->HitTestResultAt(GetCenterPointOfElement("cyan-parent"))
                  .GetScrollableContainerId());
  }
}

TEST_F(RenderWidgetTest, CompositorIdHitTestAPIWithImplicitRootScroller) {
  blink::WebRuntimeFeatures::EnableOverlayScrollbars(true);
  blink::WebRuntimeFeatures::EnableImplicitRootScroller(true);

  LoadHTML(
      R"HTML(
      <style>
      html,body {
        width: 100%;
        height: 100%;
        margin: 0px;
      }
      #scroller {
        width: 100%;
        height: 100%;
        overflow: auto;
      }
      </style>

      <div id='scroller'>
        <div style='height:3000px; background:red;'>very long content</div>
      </div>
      <div id='white' style='position:absolute; top:100px; left:50px;
        height:50px; background:white;'>some more content</div>
      )HTML");
  // Hit sibling of a implicit root scroller node
  EXPECT_EQ(GetMainFrame()
                ->GetDocument()
                .GetVisualViewportScrollingElementIdForTesting(),
            widget()
                ->GetWebWidget()
                ->HitTestResultAt(GetCenterPointOfElement("white"))
                .GetScrollableContainerId());
}

TEST_F(RenderWidgetTest, GetCompositionRangeValidComposition) {
  LoadHTML(
      "<div contenteditable>EDITABLE</div>"
      "<script> document.querySelector('div').focus(); </script>");
  gfx::Range range = LastCompositionRange();
  EXPECT_FALSE(range.IsValid());
  blink::WebVector<ui::ImeTextSpan> empty_ime_text_spans;
  DCHECK(widget()->GetInputMethodController());
  widget()->GetInputMethodController()->SetComposition(
      "hello", empty_ime_text_spans, blink::WebRange(), 3, 3);
  range = LastCompositionRange();
  EXPECT_TRUE(range.IsValid());
  EXPECT_EQ(0U, range.start());
  EXPECT_EQ(5U, range.end());
}

TEST_F(RenderWidgetTest, GetCompositionRangeForSelection) {
  LoadHTML(
      "<div>NOT EDITABLE</div>"
      "<script> document.execCommand('selectAll'); </script>");
  gfx::Range range = LastCompositionRange();
  // Selection range should not be treated as composition range.
  EXPECT_FALSE(range.IsValid());
}

TEST_F(RenderWidgetTest, GetCompositionRangeInvalid) {
  LoadHTML("<div>NOT EDITABLE</div>");
  gfx::Range range = LastCompositionRange();
  // If this test ever starts failing, one likely outcome is that WebRange
  // and gfx::Range::InvalidRange are no longer expressed in the same
  // values of start/end.
  EXPECT_FALSE(range.IsValid());
}

// This test verifies that WebInputMethodController always exists as long as
// there is a focused frame inside the page, but, IME events are only executed
// if there is also page focus.
TEST_F(RenderWidgetTest, PageFocusIme) {
  LoadHTML(
      "<input/>"
      " <script> document.querySelector('input').focus(); </script>");

  // Give initial focus to the widget.
  SetFocus(true);

  // We must have an active WebInputMethodController.
  EXPECT_TRUE(GetInputMethodController());

  // Verify the text input type.
  EXPECT_EQ(blink::WebTextInputType::kWebTextInputTypeText,
            GetInputMethodController()->TextInputType());

  // Commit some text.
  std::string text = "hello";
  CommitText(text);

  // The text should be committed since there is page focus in the beginning.
  EXPECT_EQ(text, GetInputMethodController()->TextInputInfo().value.Utf8());

  // Drop focus.
  SetFocus(false);

  // We must still have an active WebInputMethodController.
  EXPECT_TRUE(GetInputMethodController());

  // The text input type should not change.
  EXPECT_EQ(blink::WebTextInputType::kWebTextInputTypeText,
            GetInputMethodController()->TextInputType());

  // Commit the text again.
  text = " world";
  CommitText(text);

  // This time is should not work since |m_imeAcceptEvents| is not set.
  EXPECT_EQ("hello", GetInputMethodController()->TextInputInfo().value.Utf8());

  // Now give focus back again and commit text.
  SetFocus(true);
  CommitText(text);
  EXPECT_EQ("hello world",
            GetInputMethodController()->TextInputInfo().value.Utf8());
}

// Tests that the value of VisualProperties::is_pinch_gesture_active is
// not propagated to the LayerTreeHost when properties are synced for main
// frame.
TEST_F(RenderWidgetTest, ActivePinchGestureUpdatesLayerTreeHost) {
  auto* layer_tree_host = widget()->layer_tree_host();
  EXPECT_FALSE(layer_tree_host->is_external_pinch_gesture_active_for_testing());
  blink::VisualProperties visual_properties;

  // Sync visual properties on a mainframe RenderWidget.
  visual_properties.is_pinch_gesture_active = true;
  widget()->GetWebWidget()->ApplyVisualProperties(visual_properties);
  // We do not expect the |is_pinch_gesture_active| value to propagate to the
  // LayerTreeHost for the main-frame. Since GesturePinch events are handled
  // directly by the layer tree for the main frame, it already knows whether or
  // not a pinch gesture is active, and so we shouldn't propagate this
  // information to the layer tree for a main-frame's widget.
  EXPECT_FALSE(layer_tree_host->is_external_pinch_gesture_active_for_testing());
}

}  // namespace content
