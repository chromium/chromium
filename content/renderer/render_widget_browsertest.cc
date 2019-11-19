// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "content/common/visual_properties.h"
#include "content/common/widget_messages.h"
#include "content/public/renderer/render_frame_visitor.h"
#include "content/public/test/render_view_test.h"
#include "content/renderer/compositor/layer_tree_view.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/render_widget.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_input_method_controller.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_range.h"
#include "ui/base/ime/text_input_type.h"

namespace content {

class RenderWidgetTest : public RenderViewTest {
 protected:
  RenderWidget* widget() {
    return static_cast<RenderViewImpl*>(view_)->GetWidget();
  }

  void OnSynchronizeVisualProperties(
      const VisualProperties& visual_properties) {
    WidgetMsg_UpdateVisualProperties msg(widget()->routing_id(),
                                         visual_properties);
    widget()->OnMessageReceived(msg);
  }

  void GetCompositionRange(gfx::Range* range) {
    widget()->GetCompositionRange(range);
  }

  blink::WebInputMethodController* GetInputMethodController() {
    return widget()->GetInputMethodController();
  }

  void CommitText(std::string text) {
    widget()->OnImeCommitText(base::UTF8ToUTF16(text),
                              std::vector<blink::WebImeTextSpan>(),
                              gfx::Range::InvalidRange(), 0);
  }

  ui::TextInputType GetTextInputType() { return widget()->GetTextInputType(); }

  void SetFocus(bool focused) { widget()->OnSetFocus(focused); }
};

TEST_F(RenderWidgetTest, OnSynchronizeVisualProperties) {
  widget()->DidNavigate();
  // The initial bounds is empty, so setting it to the same thing should do
  // nothing.
  VisualProperties visual_properties;
  visual_properties.screen_info = ScreenInfo();
  visual_properties.new_size = gfx::Size();
  visual_properties.compositor_viewport_pixel_rect = gfx::Rect();
  visual_properties.top_controls_height = 0.f;
  visual_properties.browser_controls_shrink_blink_size = false;
  visual_properties.is_fullscreen_granted = false;
  OnSynchronizeVisualProperties(visual_properties);

  // Setting empty physical backing size should not send the ack.
  visual_properties.new_size = gfx::Size(10, 10);
  OnSynchronizeVisualProperties(visual_properties);

  // Setting the bounds to a "real" rect should send the ack.
  render_thread_->sink().ClearMessages();
  viz::ParentLocalSurfaceIdAllocator local_surface_id_allocator;
  local_surface_id_allocator.GenerateId();
  gfx::Size size(100, 100);
  visual_properties.local_surface_id_allocation =
      local_surface_id_allocator.GetCurrentLocalSurfaceIdAllocation();
  visual_properties.new_size = size;
  visual_properties.compositor_viewport_pixel_rect = gfx::Rect(size);
  OnSynchronizeVisualProperties(visual_properties);

  // Clear the flag.
  widget()->DidCommitCompositorFrame();
  widget()->DidCommitAndDrawCompositorFrame();

  // Setting the same size again should not send the ack.
  OnSynchronizeVisualProperties(visual_properties);

  // Resetting the rect to empty should not send the ack.
  visual_properties.new_size = gfx::Size();
  visual_properties.compositor_viewport_pixel_rect = gfx::Rect();
  visual_properties.local_surface_id_allocation = base::nullopt;
  OnSynchronizeVisualProperties(visual_properties);

  // Changing the screen info should not send the ack.
  visual_properties.screen_info.orientation_angle = 90;
  OnSynchronizeVisualProperties(visual_properties);

  visual_properties.screen_info.orientation_type =
      SCREEN_ORIENTATION_VALUES_PORTRAIT_PRIMARY;
  OnSynchronizeVisualProperties(visual_properties);
}

class RenderWidgetInitialSizeTest : public RenderWidgetTest {
 protected:
  VisualProperties InitialVisualProperties() override {
    VisualProperties initial_visual_properties;
    initial_visual_properties.new_size = initial_size_;
    initial_visual_properties.compositor_viewport_pixel_rect =
        gfx::Rect(initial_size_);
    initial_visual_properties.local_surface_id_allocation =
        local_surface_id_allocator_.GetCurrentLocalSurfaceIdAllocation();
    return initial_visual_properties;
  }

  gfx::Size initial_size_ = gfx::Size(200, 100);
  viz::ParentLocalSurfaceIdAllocator local_surface_id_allocator_;
};

TEST_F(RenderWidgetTest, HitTestAPI) {
  LoadHTML(
      "<body style='padding: 0px; margin: 0px'>"
      "<div style='background: green; padding: 100px; margin: 0px;'>"
      "<iframe style='width: 200px; height: 100px;'"
      "srcdoc='<body style=\"margin: 0px; height: 100px; width: 200px;\">"
      "</body>'></iframe><div></body>");
  gfx::PointF point;
  viz::FrameSinkId main_frame_sink_id =
      widget()->GetFrameSinkIdAtPoint(gfx::PointF(10, 10), &point);
  EXPECT_EQ(static_cast<uint32_t>(widget()->routing_id()),
            main_frame_sink_id.sink_id());
  EXPECT_EQ(static_cast<uint32_t>(RenderThreadImpl::Get()->GetClientId()),
            main_frame_sink_id.client_id());
  EXPECT_EQ(gfx::PointF(10, 10), point);

  // Targeting a child frame should also return the FrameSinkId for the main
  // widget.
  viz::FrameSinkId frame_sink_id =
      widget()->GetFrameSinkIdAtPoint(gfx::PointF(150, 150), &point);
  EXPECT_EQ(static_cast<uint32_t>(widget()->routing_id()),
            frame_sink_id.sink_id());
  EXPECT_EQ(main_frame_sink_id.client_id(), frame_sink_id.client_id());
  EXPECT_EQ(gfx::PointF(150, 150), point);
}

TEST_F(RenderWidgetTest, GetCompositionRangeValidComposition) {
  LoadHTML(
      "<div contenteditable>EDITABLE</div>"
      "<script> document.querySelector('div').focus(); </script>");
  blink::WebVector<blink::WebImeTextSpan> empty_ime_text_spans;
  DCHECK(widget()->GetInputMethodController());
  widget()->GetInputMethodController()->SetComposition(
      "hello", empty_ime_text_spans, blink::WebRange(), 3, 3);
  gfx::Range range;
  GetCompositionRange(&range);
  EXPECT_TRUE(range.IsValid());
  EXPECT_EQ(0U, range.start());
  EXPECT_EQ(5U, range.end());
}

TEST_F(RenderWidgetTest, GetCompositionRangeForSelection) {
  LoadHTML(
      "<div>NOT EDITABLE</div>"
      "<script> document.execCommand('selectAll'); </script>");
  gfx::Range range;
  GetCompositionRange(&range);
  // Selection range should not be treated as composition range.
  EXPECT_FALSE(range.IsValid());
}

TEST_F(RenderWidgetTest, GetCompositionRangeInvalid) {
  LoadHTML("<div>NOT EDITABLE</div>");
  gfx::Range range;
  GetCompositionRange(&range);
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
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, GetTextInputType());

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
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, GetTextInputType());

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
  auto* layer_tree_host = widget()->layer_tree_view()->layer_tree_host();
  EXPECT_FALSE(layer_tree_host->is_external_pinch_gesture_active_for_testing());
  content::VisualProperties visual_properties;

  // Sync visual properties on a mainframe RenderWidget.
  visual_properties.is_pinch_gesture_active = true;
  {
    WidgetMsg_UpdateVisualProperties msg(widget()->routing_id(),
                                         visual_properties);
    widget()->OnMessageReceived(msg);
  }
  // We do not expect the |is_pinch_gesture_active| value to propagate to the
  // LayerTreeHost for the main-frame. Since GesturePinch events are handled
  // directly by the layer tree for the main frame, it already knows whether or
  // not a pinch gesture is active, and so we shouldn't propagate this
  // information to the layer tree for a main-frame's widget.
  EXPECT_FALSE(layer_tree_host->is_external_pinch_gesture_active_for_testing());
}

}  // namespace content
