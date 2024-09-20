/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/inspector/inspector_overlay_agent.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "build/build_config.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/picture_layer.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/resources/grit/inspector_overlay_resources_map.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/script_evaluation_result.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_inspector_overlay_host.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/events/web_input_event_conversion.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/frame_overlay.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/root_frame_viewport.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/inspect_tools.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/inspector_css_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_overlay_host.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/data_resource_helper.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/inspector_protocol/crdtp/json.h"
#include "ui/accessibility/ax_mode.h"
#include "v8/include/v8.h"

using crdtp::SpanFrom;
using crdtp::json::ConvertCBORToJSON;

namespace blink {

using protocol::Maybe;

namespace {

bool ParseQuad(std::unique_ptr<protocol::Array<double>> quad_array,
               gfx::QuadF* quad) {
  const size_t kCoordinatesInQuad = 8;
  if (!quad_array || quad_array->size() != kCoordinatesInQuad) {
    return false;
  }
  quad->set_p1(gfx::PointF((*quad_array)[0], (*quad_array)[1]));
  quad->set_p2(gfx::PointF((*quad_array)[2], (*quad_array)[3]));
  quad->set_p3(gfx::PointF((*quad_array)[4], (*quad_array)[5]));
  quad->set_p4(gfx::PointF((*quad_array)[6], (*quad_array)[7]));
  return true;
}

v8::MaybeLocal<v8::Value> GetV8Property(v8::Local<v8::Context> context,
                                        v8::Local<v8::Value> object,
                                        const String& name) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::Local<v8::String> name_str = V8String(isolate, name);
  v8::Local<v8::Object> object_obj;
  if (!object->ToObject(context).ToLocal(&object_obj)) {
    return v8::MaybeLocal<v8::Value>();
  }
  return object_obj->Get(context, name_str);
}

Color ParseColor(protocol::DOM::RGBA* rgba) {
  if (!rgba) {
    return Color::kTransparent;
  }

  int r = rgba->getR();
  int g = rgba->getG();
  int b = rgba->getB();
  if (!rgba->hasA()) {
    return Color(r, g, b);
  }

  double a = rgba->getA(1);
  // Clamp alpha to the [0..1] range.
  if (a < 0) {
    a = 0;
  } else if (a > 1) {
    a = 1;
  }

  return Color(r, g, b, static_cast<int>(a * 255));
}

}  // namespace

// OverlayNames ----------------------------------------------------------------
const char* OverlayNames::OVERLAY_HIGHLIGHT = "highlight";
const char* OverlayNames::OVERLAY_PERSISTENT = "persistent";
const char* OverlayNames::OVERLAY_SOURCE_ORDER = "sourceOrder";
const char* OverlayNames::OVERLAY_DISTANCES = "distances";
const char* OverlayNames::OVERLAY_VIEWPORT_SIZE = "viewportSize";
const char* OverlayNames::OVERLAY_SCREENSHOT = "screenshot";
const char* OverlayNames::OVERLAY_PAUSED = "paused";
const char* OverlayNames::OVERLAY_WINDOW_CONTROLS_OVERLAY =
    "windowControlsOverlay";

// InspectTool -----------------------------------------------------------------
bool InspectTool::HandleInputEvent(LocalFrameView* frame_view,
                                   const WebInputEvent& input_event,
                                   bool* swallow_next_mouse_up) {
  if (input_event.GetType() == WebInputEvent::Type::kGestureTap) {
    // We only have a use for gesture tap.
    WebGestureEvent transformed_event = TransformWebGestureEvent(
        frame_view, static_cast<const WebGestureEvent&>(input_event));
    return HandleGestureTapEvent(transformed_event);
  }

  if (WebInputEvent::IsMouseEventType(input_event.GetType())) {
    WebMouseEvent transformed_event = TransformWebMouseEvent(
        frame_view, static_cast<const WebMouseEvent&>(input_event));
    return HandleMouseEvent(transformed_event, swallow_next_mouse_up);
  }

  if (WebInputEvent::IsPointerEventType(input_event.GetType())) {
    WebPointerEvent transformed_event = TransformWebPointerEvent(
        frame_view, static_cast<const WebPointerEvent&>(input_event));
    return HandlePointerEvent(transformed_event);
  }

  if (WebInputEvent::IsKeyboardEventType(input_event.GetType())) {
    return HandleKeyboardEvent(
        static_cast<const WebKeyboardEvent&>(input_event));
  }

  return false;
}

bool InspectTool::HandleMouseEvent(const WebMouseEvent& mouse_event,
                                   bool* swallow_next_mouse_up) {
  if (mouse_event.GetType() == WebInputEvent::Type::kMouseMove) {
    return HandleMouseMove(mouse_event);
  }

  if (mouse_event.GetType() == WebInputEvent::Type::kMouseDown) {
    return HandleMouseDown(mouse_event, swallow_next_mouse_up);
  }

  if (mouse_event.GetType() == WebInputEvent::Type::kMouseUp) {
    return HandleMouseUp(mouse_event);
  }

  return false;
}

bool InspectTool::HandleMouseDown(const WebMouseEvent&,
                                  bool* swallow_next_mouse_up) {
  return false;
}

bool InspectTool::HandleMouseUp(const WebMouseEvent&) {
  return false;
}

bool InspectTool::HandleMouseMove(const WebMouseEvent&) {
  return false;
}

bool InspectTool::HandleGestureTapEvent(const WebGestureEvent&) {
  return false;
}

bool InspectTool::HandlePointerEvent(const WebPointerEvent&) {
  return false;
}

bool InspectTool::HandleKeyboardEvent(const WebKeyboardEvent&) {
  return false;
}

bool InspectTool::ForwardEventsToOverlay() {
  return true;
}

bool InspectTool::SupportsPersistentOverlays() {
  return false;
}

bool InspectTool::HideOnMouseMove() {
  return false;
}

bool InspectTool::HideOnHideHighlight() {
  return false;
}

void InspectTool::Trace(Visitor* visitor) const {
  visitor->Trace(overlay_);
}

// Hinge -----------------------------------------------------------------------

Hinge::Hinge(gfx::QuadF quad,
             Color content_color,
             Color outline_color,
             InspectorOverlayAgent* overlay)
    : quad_(quad),
      content_color_(content_color),
      outline_color_(outline_color),
      overlay_(overlay) {}

String Hinge::GetOverlayName() {
  // TODO (soxia): In the future, we should make the hinge working properly
  // with tools using different resources.
  return OverlayNames::OVERLAY_HIGHLIGHT;
}

void Hinge::Trace(Visitor* visitor) const {
  visitor->Trace(overlay_);
}

void Hinge::Draw(float scale) {
  // scaling is applied at the drawHighlight code.
  InspectorHighlight highlight(1.f);
  highlight.AppendQuad(quad_, content_color_, outline_color_);
  overlay_->EvaluateInOverlay("drawHighlight", highlight.AsProtocolValue());
}

// InspectorOverlayAgent -------------------------------------------------------

class InspectorOverlayAgent::InspectorPageOverlayDelegate final
    : public FrameOverlay::Delegate,
      public cc::ContentLayerClient {
 public:
  explicit InspectorPageOverlayDelegate(InspectorOverlayAgent& overlay)
      : overlay_(&overlay) {
    layer_ = cc::PictureLayer::Create(this);
    layer_->SetIsDrawable(true);
    layer_->SetHitTestable(false);
  }
  ~InspectorPageOverlayDelegate() override {
    if (layer_) {
      layer_->ClearClient();
    }
  }

  void PaintFrameOverlay(const FrameOverlay& frame_overlay,
                         GraphicsContext& graphics_context,
                         const gfx::Size& size) const override {
    if (!overlay_->IsVisible()) {
      return;
    }

    CHECK_EQ(layer_->client(), this);

    overlay_->PaintOverlayPage();

    // The emulation scale factor is baked in the contents of the overlay layer,
    // so the size of the layer also needs to be scaled.
    layer_->SetBounds(
        gfx::ScaleToCeiledSize(size, overlay_->EmulationScaleFactor()));
    DEFINE_STATIC_DISPLAY_ITEM_CLIENT(client, "InspectorOverlay");
    // The overlay layer needs to be in the root property tree state (instead of
    // the default FrameOverlay state which is under the emulation scale
    // transform node) because the emulation scale is baked in the layer.
    auto property_tree_state = PropertyTreeState::Root();
    RecordForeignLayer(graphics_context, *client,
                       DisplayItem::kForeignLayerDevToolsOverlay, layer_,
                       gfx::Point(), &property_tree_state);
  }

  void Invalidate() override {
    overlay_->GetFrame()->View()->SetVisualViewportOrOverlayNeedsRepaint();
    if (layer_) {
      layer_->SetNeedsDisplay();
    }
  }

  const cc::Layer* GetLayer() const { return layer_.get(); }

 private:
  // cc::ContentLayerClient implementation
  bool FillsBoundsCompletely() const override { return false; }

  scoped_refptr<cc::DisplayItemList> PaintContentsToDisplayList() override {
    auto display_list = base::MakeRefCounted<cc::DisplayItemList>();
    display_list->StartPaint();
    display_list->push<cc::DrawRecordOp>(
        overlay_->OverlayMainFrame()->View()->GetPaintRecord());
    display_list->EndPaintOfUnpaired(gfx::Rect(layer_->bounds()));
    display_list->Finalize();
    return display_list;
  }

  Persistent<InspectorOverlayAgent> overlay_;
  scoped_refptr<cc::PictureLayer> layer_;
};

class InspectorOverlayAgent::InspectorOverlayChromeClient final
    : public EmptyChromeClient {
 public:
  InspectorOverlayChromeClient(ChromeClient& client,
                               InspectorOverlayAgent& overlay)
      : client_(&client), overlay_(&overlay) {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(client_);
    visitor->Trace(overlay_);
    EmptyChromeClient::Trace(visitor);
  }

  void SetCursor(const ui::Cursor& cursor, LocalFrame* local_root) override {
    client_->SetCursorOverridden(false);
    client_->SetCursor(cursor, overlay_->GetFrame());
    client_->SetCursorOverridden(true);
  }

  void UpdateTooltipUnderCursor(LocalFrame& frame,
                                const String& tooltip,
                                TextDirection direction) override {
    DCHECK_EQ(&frame, overlay_->OverlayMainFrame());
    client_->UpdateTooltipUnderCursor(*overlay_->GetFrame(), tooltip,
                                      direction);
  }

 private:
  Member<ChromeClient> client_;
  Member<InspectorOverlayAgent> overlay_;
};

InspectorOverlayAgent::InspectorOverlayAgent(
    WebLocalFrameImpl* frame_impl,
    InspectedFrames* inspected_frames,
    v8_inspector::V8InspectorSession* v8_session,
    InspectorDOMAgent* dom_agent)
    : frame_impl_(frame_impl),
      inspected_frames_(inspected_frames),
      resize_timer_active_(false),
      resize_timer_(
          frame_impl->GetFrame()->GetTaskRunner(TaskType::kInternalInspector),
          this,
          &InspectorOverlayAgent::OnResizeTimer),
      disposed_(false),
      v8_session_(v8_session),
      dom_agent_(dom_agent),
      swallow_next_mouse_up_(false),
      backend_node_id_to_inspect_(0),
      enabled_(&agent_state_, false),
      show_ad_highlights_(&agent_state_, false),
      show_debug_borders_(&agent_state_, false),
      show_fps_counter_(&agent_state_, false),
      show_paint_rects_(&agent_state_, false),
      show_layout_shift_regions_(&agent_state_, false),
      show_scroll_bottleneck_rects_(&agent_state_, false),
      show_hit_test_borders_(&agent_state_, false),
      show_web_vitals_(&agent_state_, false),
      show_size_on_resize_(&agent_state_, false),
      paused_in_debugger_message_(&agent_state_, String()),
      inspect_mode_(&agent_state_, protocol::Overlay::InspectModeEnum::None),
      inspect_mode_protocol_config_(&agent_state_, std::vector<uint8_t>()) {
  DCHECK(dom_agent);

  frame_impl_->GetFrame()->GetProbeSink()->AddInspectorOverlayAgent(this);

  if (GetFrame()->GetWidgetForLocalRoot()) {
    original_layer_tree_debug_state_ =
        std::make_unique<cc::LayerTreeDebugState>(
            *GetFrame()->GetWidgetForLocalRoot()->GetLayerTreeDebugState());
  }
}

InspectorOverlayAgent::~InspectorOverlayAgent() {
  DCHECK(!overlay_page_);
  DCHECK(!inspect_tool_);
  DCHECK(!hinge_);
  DCHECK(!persistent_tool_);
  DCHECK(!frame_overlay_);
}

void InspectorOverlayAgent::Trace(Visitor* visitor) const {
  visitor->Trace(frame_impl_);
  visitor->Trace(inspected_frames_);
  visitor->Trace(overlay_page_);
  visitor->Trace(overlay_chrome_client_);
  visitor->Trace(overlay_host_);
  visitor->Trace(resize_timer_);
  visitor->Trace(dom_agent_);
  visitor->Trace(frame_overlay_);
  visitor->Trace(inspect_tool_);
  visitor->Trace(persistent_tool_);
  visitor->Trace(hinge_);
  visitor->Trace(document_to_ax_context_);
  InspectorBaseAgent::Trace(visitor);
}

void InspectorOverlayAgent::Restore() {
  if (enabled_.Get()) {
    enable();
  }
  setShowAdHighlights(show_ad_highlights_.Get());
  setShowDebugBorders(show_debug_borders_.Get());
  setShowFPSCounter(show_fps_counter_.Get());
  setShowPaintRects(show_paint_rects_.Get());
  setShowLayoutShiftRegions(show_layout_shift_regions_.Get());
  setShowScrollBottleneckRects(show_scroll_bottleneck_rects_.Get());
  setShowHitTestBorders(show_hit_test_borders_.Get());
  setShowViewportSizeOnResize(show_size_on_resize_.Get());
  setShowWebVitals(show_web_vitals_.Get());
  PickTheRightTool();
}

void InspectorOverlayAgent::Dispose() {
  InspectorBaseAgent::Dispose();
  disposed_ = true;

  frame_impl_->GetFrame()->GetProbeSink()->RemoveInspectorOverlayAgent(this);
}

protocol::Response InspectorOverlayAgent::enable() {
  if (!dom_agent_->Enabled()) {
    return protocol::Response::ServerError("DOM should be enabled first");
  }
  enabled_.Set(true);
  if (backend_node_id_to_inspect_) {
    GetFrontend()->inspectNodeRequested(
        static_cast<int>(backend_node_id_to_inspect_));
  }
  backend_node_id_to_inspect_ = 0;
  SetNeedsUnbufferedInput(true);
  return protocol::Response::Success();
}

bool InspectorOverlayAgent::HasAXContext(Node* node) {
  return document_to_ax_context_.Contains(&node->GetDocument());
}

void InspectorOverlayAgent::EnsureAXContext(Node* node) {
  EnsureAXContext(node->GetDocument());
}

void InspectorOverlayAgent::EnsureAXContext(Document& document) {
  if (!document_to_ax_context_.Contains(&document)) {
    auto context = std::make_unique<AXContext>(document, ui::kAXModeComplete);
    document_to_ax_context_.Set(&document, std::move(context));
  }
}

protocol::Response InspectorOverlayAgent::disable() {
  enabled_.Clear();
  setShowAdHighlights(false);
  setShowViewportSizeOnResize(false);
  paused_in_debugger_message_.Clear();
  inspect_mode_.Set(protocol::Overlay::InspectModeEnum::None);
  inspect_mode_protocol_config_.Set(std::vector<uint8_t>());

  if (FrameWidgetInitialized()) {
    GetFrame()->GetWidgetForLocalRoot()->SetLayerTreeDebugState(
        *original_layer_tree_debug_state_);
  }

  if (overlay_page_) {
    overlay_page_->WillBeDestroyed();
    overlay_page_.Clear();
    overlay_chrome_client_.Clear();
    overlay_host_->ClearDelegate();
    overlay_host_.Clear();
  }
  resize_timer_.Stop();
  resize_timer_active_ = false;

  if (frame_overlay_) {
    frame_overlay_.Release()->Destroy();
  }

  persistent_tool_ = nullptr;
  hinge_ = nullptr;
  PickTheRightTool();
  SetNeedsUnbufferedInput(false);
  document_to_ax_context_.clear();
  return protocol::Response::Success();
}

protocol::Response InspectorOverlayAgent::setShowAdHighlights(bool show) {
  show_ad_highlights_.Set(show);
  frame_impl_->ViewImpl()->GetPage()->GetSettings().SetHighlightAds(show);
  return protocol::Response::Success();
}

protocol::Response InspectorOverlayAgent::setShowDebugBorders(bool show) {
  show_debug_borders_.Set(show);
  if (show) {
    protocol::Response response = CompositingEnabled();
    if (!response.IsSuccess()) {
      return response;
    }
  }
  if (FrameWidgetInitialized()) {
    FrameWidget* widget = GetFrame()->GetWidgetForLocalRoot();
    cc::LayerTreeDebugState debug_state = *widget->GetLayerTreeDebugState();
    if (show) {
      debug_state.show_debug_borders.set();
    } else {
      debug_state.show_debug_borders.reset();
    }
    widget->SetLayerTreeDebugState(debug_state);
  }
  return protocol::Response::Success();
}

protocol::Response InspectorOverlayAgent::setShowFPSCounter(bool show) {
  show_fps_counter_.Set(show);
  if (show) {
    protocol::Response response = CompositingEnabled();
    if (!response.IsSuccess()) {
      return response;
    }
  }
  if (FrameWidgetInitialized()) {
    FrameWidget* widget = GetFrame()->GetWidgetForLocalRoot();
    cc::LayerTreeDebugState debug_state = *widget->GetLayerTreeDebugState();
    debug_state.show_fps_counter = show;
    widget->SetLayerTreeDebugState(debug_state);
  }
  return protocol::Response::Success();
}

protocol::Response InspectorOverlayAgent::setShowPaintRects(bool show) {
  show_paint_rects_.Set(show);
  if (show) {
    protocol::Response response = CompositingEnabled();
    if (!response.IsSuccess()) {
      return response;
    }
  }
  if (FrameWidgetInitialized()) {
    FrameWidget* widget = GetFrame()->GetWidgetForLocalRoot();
    cc::LayerTreeDebugState debug_state = *widget->GetLayerTreeDebugState();
    debug_state.show_paint_rects = show;
    widget->SetLayerTreeDebugState(debug_state);
  }
  return protocol::Response::Success();
}

protocol::Response InspectorOverlayAgent::setShowLayoutShiftRegions(bool show) {
  show_layout_shift_regions_.Set(show);
  if (show) {
    protocol::Response response = CompositingEnabled();
    if (!response.IsSuccess()) {
      return response;
    }
  }
  if (FrameWidgetInitialized()) {
    FrameWidget* widget = GetFrame()->GetWidgetForLocalRoot();
    cc::LayerTreeDebugState debug_state = *widget->GetLayerTreeDebugState();
    debug_state.show_layout_shift_regions = show;
    widget->SetLayerTreeDebugState(debug_state);
  }
  return protocol::Response::Success();
}

protocol::Response InspectorOverlayAgent::setShowScrollBottleneckRects(
    bool show) {
  show_scroll_bottleneck_rects_.Set(show);
  if (show) {
    protocol::Response response = CompositingEnabled();
    if (!response.IsSuccess()) {
      return response;
    }
  }
  if (FrameWidgetInitialized()) {
    FrameWidget* widget = GetFrame()->GetWidgetForLocalRoot();
    cc::LayerTreeDebugState debug_state = *widget->GetLayerTreeDebugState();
    debug_state.show_touch_event_handler_rects = show;
    debug_state.show_wheel_event_handler_rects = show;
    debug_state.show_main_thread_scroll_hit_test_rects = show;
    debug_state.show_main_thread_scroll_repaint_rects = show;
    debug_state.show_raster_inducing_scroll_rects = show;
    widget->SetLayerTreeDebugState(debug_state);
  }
  return protocol::Response::Success();
}

protocol::Response InspectorOverlayAgent::setShowHitTestBorders(bool show) {
  // This CDP command has been deprecated. Don't do anything and return success.
  return protocol::Response::Success();
}

protocol::Response InspectorOverlayAgent::setShowViewportSizeOnResize(
    bool show) {
  show_size_on_resize_.Set(show);
  return protocol::Response::Success();
}

protocol::Response InspectorOverlayAgent::setShowWebVitals(bool show) {
  return protocol::Response::Success();
}

protocol::Response InspectorOverlayAgent::setShowWindowControlsOverlay(
    protocol::Maybe<protocol::Overlay::WindowControlsOverlayConfig>
        wco_config) {
  // Hide WCO when called without a configuration.
  if (!wco_config.has_value()) {
    SetInspectTool(nullptr);
    return protocol::Response::Success();
  }

  std::unique_ptr<protocol::DictionaryValue> result =
      protocol::DictionaryValue::create();

  protocol::Overlay::WindowControlsOverlayConfig& config = wco_config.value();

  result->setBoolean("showCSS", config.getShowCSS());
  result->setString("selectedPlatform", config.getSelectedPlatform());
  result->setString("themeColor", config.getThemeColor());

  return SetInspectTool(MakeGarbageCollected<WindowControlsOverlayTool>(
      this, GetFrontend(), std::move(result)));
}

protocol::Response InspectorOverlayAgent::setPausedInDebuggerMessage(
    Maybe<String> message) {
  paused_in_debugger_message_.Set(message.value_or(String()));
  PickTheRightTool();
  return protocol::Response::Success();
}

protocol::Response InspectorOverlayAgent::highlightRect(
    int x,
    int y,
    int width,
    int height,
    Maybe<protocol::DOM::RGBA> color,
    Maybe<protocol::DOM::RGBA> outline_color) {
  std::unique_ptr<gfx::QuadF> quad =
      std::make_unique<gfx::QuadF>(gfx::RectF(x, y, width, height));
  return SetInspectTool(MakeGarbageCollected<QuadHighlightTool>(
      this, GetFrontend(), std::move(quad),
      ParseColor(color.has_value() ? &color.value() : nullptr),
      ParseColor(outline_color.has_value() ? &outline_color.value()
                                           : nullptr)));
}

protocol::Response InspectorOverlayAgent::highlightQuad(
    std::unique_ptr<protocol::Array<double>> quad_array,
    Maybe<protocol::DOM::RGBA> color,
    Maybe<protocol::DOM::RGBA> outline_color) {
  std::unique_ptr<gfx::QuadF> quad = std::make_unique<gfx::QuadF>();
  if (!ParseQuad(std::move(quad_array), quad.get())) {
    return protocol::Response::ServerError("Invalid Quad format");
  }
  return SetInspectTool(MakeGarbageCollected<QuadHighlightTool>(
      this, GetFrontend(), std::move(quad),
      ParseColor(color.has_value() ? &color.value() : nullptr),
      ParseColor(outline_color.has_value() ? &outline_color.value()
                                           : nullptr)));
}

protocol::Response InspectorOverlayAgent::setShowHinge(
    protocol::Maybe<protocol::Overlay::HingeConfig> tool_config) {
  // Hide the hinge when called without a configuration.
  if (!tool_config.has_value()) {
    hinge_ = nullptr;
    if (!inspect_tool_) {
      DisableFrameOverlay();
    }
    ScheduleUpdate();
    return protocol::Response::Success();
  }

  // Create a hinge
  protocol::Overlay::HingeConfig& config = tool_config.value();
  protocol::DOM::Rect* rect = config.getRect();
  int x = rect->getX();
  int y = rect->getY();
  int width = rect->getWidth();
  int height = rect->getHeight();
  if (x < 0 || y < 0 || width < 0 || height < 0) {
    return protocol::Response::InvalidParams("Invalid hinge rectangle.");
  }

  // Use default color if a content color is not provided.
  Color content_color = config.hasContentColor()
                            ? ParseColor(config.getContentColor(nullptr))
                            : Color(38, 38, 38);
  // outlineColor uses a kTransparent default from ParseColor if not provided.
  Color outline_color = ParseColor(config.getOutlineColor(nullptr));

  DCHECK(frame_impl_->GetFrameView() && GetFrame());

  gfx::QuadF quad(gfx::RectF(x, y, width, height));
  hinge_ =
      MakeGarbageCollected<Hinge>(quad, content_color, outline_color, this);

  LoadOverlayPageResource();
  EvaluateInOverlay("setOverlay", hinge_->GetOverlayName());
  EnsureEnableFrameOverlay();

  ScheduleUpdate();

  return protocol::Response::Success();
}

protocol::Response InspectorOverlayAgent::highlightNode(
    std::unique_ptr<protocol::Overlay::HighlightConfig>
        highlight_inspector_object,
    Maybe<int> node_id,
    Maybe<int> backend_node_id,
    Maybe<String> object_id,
    Maybe<String> selector_list) {
  Node* node = nullptr;
  protocol::Response response =
      dom_agent_->AssertNode(node_id, backend_node_id, object_id, node);
  if (!response.IsSuccess()) {
    return response;
  }

  if (node->GetDocument().Lifecycle().GetState() <=
      DocumentLifecycle::LifecycleState::kInactive) {
    return protocol::Response::InvalidRequest(
        "The node's document is not active");
  }

  std::unique_ptr<InspectorHighlightConfig> highlight_config;
  response = HighlightConfigFromInspectorObject(
      std::move(highlight_inspector_object), &highlight_config);
  if (!response.IsSuccess()) {
    return response;
  }

  return SetInspectTool(MakeGarbageCollected<NodeHighlightTool>(
      this, GetFrontend(), node, selector_list.value_or(String()),
      std::move(highlight_config)));
}

protocol::Response InspectorOverlayAgent::setShowGridOverlays(
    std::unique_ptr<protocol::Array<protocol::Overlay::GridNodeHighlightConfig>>
        grid_node_highlight_configs) {
  if (!persistent_tool_) {
    persistent_tool_ =
        MakeGarbageCollected<PersistentTool>(this, GetFrontend());
  }

  HeapHashMap<WeakMember<Node>, std::unique_ptr<InspectorGridHighlightConfig>>
      configs;
  for (std::unique_ptr<protocol::Overlay::GridNodeHighlightConfig>& config :
       *grid_node_highlight_configs) {
    Node* node = nullptr;
    protocol::Response response =
        dom_agent_->AssertNode(config->getNodeId(), node);
    if (!response.IsSuccess()) {
      return response;
    }
    configs.insert(node, InspectorOverlayAgent::ToGridHighlightConfig(
                             config->getGridHighlightConfig()));
  }

  persistent_tool_->SetGridConfigs(std::move(configs));

  PickTheRightTool();

  return protocol::Response::Success();
}

protocol::Response InspectorOverlayAgent::setShowFlexOverlays(
    std::unique_ptr<protocol::Array<protocol::Overlay::FlexNodeHighlightConfig>>
        flex_node_highlight_configs) {
  if (!persistent_tool_) {
    persistent_tool_ =
        MakeGarbageCollected<PersistentTool>(this, GetFrontend());
  }

  HeapHashMap<WeakMember<Node>,
              std::unique_ptr<InspectorFlexContainerHighlightConfig>>
      configs;

  for (std::unique_ptr<protocol::Overlay::FlexNodeHighlightConfig>& config :
       *flex_node_highlight_configs) {
    Node* node = nullptr;
    protocol::Response response =
        dom_agent_->AssertNode(config->getNodeId(), node);
    if (!response.IsSuccess()) {
      return response;
    }
    configs.insert(node, InspectorOverlayAgent::ToFlexContainerHighlightConfig(
                             config->getFlexContainerHighlightConfig()));
  }

  persistent_tool_->SetFlexContainerConfigs(std::move(configs));

  PickTheRightTool();

  return protocol::Response::Success();
}

protocol::Response InspectorOverlayAgent::setShowScrollSnapOverlays(
    std::unique_ptr<
        protocol::Array<protocol::Overlay::ScrollSnapHighlightConfig>>
        scroll_snap_highlight_configs) {
  if (!persistent_tool_) {
    persistent_tool_ =
        MakeGarbageCollected<PersistentTool>(this, GetFrontend());
  }

  HeapHashMap<WeakMember<Node>,
              std::unique_ptr<InspectorScrollSnapContainerHighlightConfig>>
      configs;

  for (std::unique_ptr<protocol::Overlay::ScrollSnapHighlightConfig>& config :
       *scroll_snap_highlight_configs) {
    Node* node = nullptr;
    protocol::Response response =
        dom_agent_->AssertNode(config->getNodeId(), node);
    if (!response.IsSuccess()) {
      return response;
    }
    configs.insert(node,
                   InspectorOverlayAgent::ToScrollSnapContainerHighlightConfig(
                       config->getScrollSnapContainerHighlightConfig()));
  }

  persistent_tool_->SetScrollSnapConfigs(std::move(configs));

  PickTheRightTool();

  return protocol::Response::Success();
}

protocol::Response InspectorOverlayAgent::setShowContainerQueryOverlays(
    std::unique_ptr<
        protocol::Array<protocol::Overlay::ContainerQueryHighlightConfig>>
        container_query_highlight_configs) {
  if (!persistent_tool_) {
    persistent_tool_ =
        MakeGarbageCollected<PersistentTool>(this, GetFrontend());
  }

  HeapHashMap<WeakMember<Node>,
              std::unique_ptr<InspectorContainerQueryContainerHighlightConfig>>
      configs;

  for (std::unique_ptr<protocol::Overlay::ContainerQueryHighlightConfig>&
           config : *container_query_highlight_configs) {
    Node* node = nullptr;
    protocol::Response response =
        dom_agent_->AssertNode(config->getNodeId(), node);
    if (!response.IsSuccess()) {
      return response;
    }
    configs.insert(
        node, InspectorOverlayAgent::ToContainerQueryContainerHighlightConfig(
                  config->getContainerQueryContainerHighlightConfig()));
  }

  persistent_tool_->SetContainerQueryConfigs(std::move(configs));

  PickTheRightTool();

  return protocol::Response::Success();
}

protocol::Response InspectorOverlayAgent::setShowIsolatedElements(
    std::unique_ptr<
        protocol::Array<protocol::Overlay::IsolatedElementHighlightConfig>>
        isolated_element_highlight_configs) {
  if (!persistent_tool_) {
    persistent_tool_ =
        MakeGarbageCollected<PersistentTool>(this, GetFrontend());
  }

  HeapHashMap<WeakMember<Element>,
              std::unique_ptr<InspectorIsolationModeHighlightConfig>>
      configs;

  int idx = 0;
  for (std::unique_ptr<protocol::Overlay::IsolatedElementHighlightConfig>&
           config : *isolated_element_highlight_configs) {
    Element* element = nullptr;
    // Isolation mode can only be triggered on elements
    protocol::Response response =
        dom_agent_->AssertElement(config->getNodeId(), element);
    if (!response.IsSuccess()) {
      return response;
    }
    configs.insert(element,
                   InspectorOverlayAgent::ToIsolationModeHighlightConfig(
                       config->getIsolationModeHighlightConfig(), idx));
    idx++;
  }

  persistent_tool_->SetIsolatedElementConfigs(std::move(configs));

  PickTheRightTool();

  return protocol::Response::Success();
}

protocol::Response InspectorOverlayAgent::highlightSourceOrder(
    std::unique_ptr<protocol::Overlay::SourceOrderConfig>
        source_order_inspector_object,
    Maybe<int> node_id,
    Maybe<int> backend_node_id,
    Maybe<String> object_id) {
  Node* node = nullptr;
  protocol::Response response =
      dom_agent_->AssertNode(node_id, backend_node_id, object_id, node);
  if (!response.IsSuccess()) {
    return response;
  }

  InspectorSourceOrderConfig config = SourceOrderConfigFromInspectorObject(
      std::move(source_order_inspector_object));
  std::unique_ptr<InspectorSourceOrderConfig> source_order_config =
      std::make_unique<InspectorSourceOrderConfig>(config);

  return SetInspectTool(MakeGarbageCollected<SourceOrderTool>(
      this, GetFrontend(), node, std::move(source_order_config)));
}

protocol::Response InspectorOverlayAgent::highlightFrame(
    const String& frame_id,
    Maybe<protocol::DOM::RGBA> color,
    Maybe<protocol::DOM::RGBA> outline_color) {
  LocalFrame* frame =
      IdentifiersFactory::FrameById(inspected_frames_, frame_id);
  // FIXME: Inspector doesn't currently work cross process.
  if (!frame) {
    return protocol::Response::ServerError("Invalid frame id");
  }
  if (!frame->DeprecatedLocalOwner()) {
    PickTheRightTool();
    return protocol::Response::Success();
  }

  std::unique_ptr<InspectorHighlightConfig> highlight_config =
      std::make_unique<InspectorHighlightConfig>();
  highlight_config->show_info = true;  // Always show tooltips for frames.
  highlight_config->content =
      ParseColor(color.has_value() ? &color.value() : nullptr);
  highlight_config->content_outline =
      ParseColor(outline_color.has_value() ? &outline_color.value() : nullptr);

  return SetInspectTool(MakeGarbageCollected<NodeHighlightTool>(
      this, GetFrontend(), frame->DeprecatedLocalOwner(), String(),
      std::move(highlight_config)));
}

protocol::Response InspectorOverlayAgent::hideHighlight() {
  if (inspect_tool_ && inspect_tool_->HideOnHideHighlight()) {
    PickTheRightTool();
  }

  return protocol::Response::Success();
}

protocol::Response InspectorOverlayAgent::getHighlightObjectForTest(
    int node_id,
    Maybe<bool> include_distance,
    Maybe<bool> include_style,
    Maybe<String> colorFormat,
    Maybe<bool> show_accessibility_info,
    std::unique_ptr<protocol::DictionaryValue>* result) {
  Node* node = nullptr;
  protocol::Response response = dom_agent_->AssertNode(node_id, node);
  if (!response.IsSuccess()) {
    return response;
  }

  auto config = std::make_unique<InspectorHighlightConfig>(
      InspectorHighlight::DefaultConfig());
  config->show_styles = include_style.value_or(false);
  config->show_accessibility_info = show_accessibility_info.value_or(true);
  String format = colorFormat.value_or("hex");
  namespace ColorFormatEnum = protocol::Overlay::ColorFormatEnum;
  if (format == ColorFormatEnum::Hsl) {
    config->color_format = ColorFormat::kHsl;
  } else if (format == ColorFormatEnum::Hwb) {
    config->color_format = ColorFormat::kHwb;
  } else if (format == ColorFormatEnum::Rgb) {
    config->color_format = ColorFormat::kRgb;
  } else {
    config->color_format = ColorFormat::kHex;
  }
  NodeHighlightTool* tool = MakeGarbageCollected<NodeHighlightTool>(
      this, GetFrontend(), node, "" /* selector_list */, std::move(config));
  node->GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kInspector);
  *result = tool->GetNodeInspectorHighlightAsJson(
      true /* append_element_info */, include_distance.value_or(false));
  return protocol::Response::Success();
}

protocol::Response InspectorOverlayAgent::getGridHighlightObjectsForTest(
    std::unique_ptr<protocol::Array<int>> node_ids,
    std::unique_ptr<protocol::DictionaryValue>* highlights) {
  PersistentTool* persistent_tool =
      MakeGarbageCollected<PersistentTool>(this, GetFrontend());

  HeapHashMap<WeakMember<Node>, std::unique_ptr<InspectorGridHighlightConfig>>
      configs;
  for (const int node_id : *node_ids) {
    Node* node = nullptr;
    protocol::Response response = dom_agent_->AssertNode(node_id, node);
    if (!response.IsSuccess()) {
      return response;
    }
    configs.insert(node, std::make_unique<InspectorGridHighlightConfig>(
                             InspectorHighlight::DefaultGridConfig()));
  }
  persistent_tool->SetGridConfigs(std::move(configs));
  *highlights = persistent_tool->GetGridInspectorHighlightsAsJson();
  return protocol::Response::Success();
}

protocol::Response InspectorOverlayAgent::getSourceOrderHighlightObjectForTest(
    int node_id,
    std::unique_ptr<protocol::DictionaryValue>* result) {
  Node* node = nullptr;
  protocol::Response response = dom_agent_->AssertNode(node_id, node);
  if (!response.IsSuccess()) {
    return response;
  }

  auto config = std::make_unique<InspectorSourceOrderConfig>(
      InspectorSourceOrderHighlight::DefaultConfig());

  SourceOrderTool* tool = MakeGarbageCollected<SourceOrderTool>(
      this, GetFrontend(), node, std::move(config));
  *result = tool->GetNodeInspectorSourceOrderHighlightAsJson();
  return protocol::Response::Success();
}

void InspectorOverlayAgent::UpdatePrePaint() {
  if (frame_overlay_) {
    frame_overlay_->UpdatePrePaint();
  }
}

void InspectorOverlayAgent::PaintOverlay(GraphicsContext& context) {
  if (frame_overlay_) {
    frame_overlay_->Paint(context);
  }
}

bool InspectorOverlayAgent::IsInspectorLayer(const cc::Layer* layer) const {
  if (!frame_overlay_) {
    return false;
  }
  return layer == static_cast<const InspectorPageOverlayDelegate*>(
                      frame_overlay_->GetDelegate())
                      ->GetLayer();
}

LocalFrame* InspectorOverlayAgent::GetFrame() const {
  return frame_impl_->GetFrame();
}

void InspectorOverlayAgent::DispatchBufferedTouchEvents() {
  if (!inspect_tool_) {
    return;
  }
  OverlayMainFrame()->GetEventHandler().DispatchBufferedTouchEvents();
}

void InspectorOverlayAgent::SetPageIsScrolling(bool is_scrolling) {
  is_page_scrolling_ = is_scrolling;
}

WebInputEventResult InspectorOverlayAgent::HandleInputEvent(
    const WebInputEvent& input_event) {
  if (!enabled_.Get()) {
    return WebInputEventResult::kNotHandled;
  }

  if (input_event.GetType() == WebInputEvent::Type::kMouseUp &&
      swallow_next_mouse_up_) {
    swallow_next_mouse_up_ = false;
    return WebInputEventResult::kHandledSuppressed;
  }

  LocalFrame* frame = GetFrame();
  if (!frame || !frame->View() || !frame->ContentLayoutObject() ||
      !inspect_tool_) {
    return WebInputEventResult::kNotHandled;
  }

  bool handled = inspect_tool_->HandleInputEvent(
      frame_impl_->GetFrameView(), input_event, &swallow_next_mouse_up_);

  if (handled) {
    ScheduleUpdate();
    return WebInputEventResult::kHandledSuppressed;
  }

  if (inspect_tool_->ForwardEventsToOverlay()) {
    WebInputEventResult result = HandleInputEventInOverlay(input_event);
    if (result != WebInputEventResult::kNotHandled) {
      ScheduleUpdate();
      return result;
    }
  }

  // Exit tool upon unhandled Esc.
  if (input_event.GetType() == WebInputEvent::Type::kRawKeyDown) {
    const WebKeyboardEvent& keyboard_event =
        static_cast<const WebKeyboardEvent&>(input_event);
    if (keyboard_event.windows_key_code == VKEY_ESCAPE) {
      GetFrontend()->inspectModeCanceled();
      return WebInputEventResult::kNotHandled;
    }
  }

  if (input_event.GetType() == WebInputEvent::Type::kMouseMove &&
      inspect_tool_->HideOnMouseMove()) {
    PickTheRightTool();
  }

  return WebInputEventResult::kNotHandled;
}

WebInputEventResult InspectorOverlayAgent::HandleInputEventInOverlay(
    const WebInputEvent& input_event) {
  if (input_event.GetType() == WebInputEvent::Type::kGestureTap) {
    return OverlayMainFrame()->GetEventHandler().HandleGestureEvent(
        static_cast<const WebGestureEvent&>(input_event));
  }

  if (WebInputEvent::IsMouseEventType(input_event.GetType())) {
    WebMouseEvent mouse_event = static_cast<const WebMouseEvent&>(input_event);
    if (mouse_event.GetType() == WebInputEvent::Type::kMouseMove) {
      return OverlayMainFrame()->GetEventHandler().HandleMouseMoveEvent(
          mouse_event, {}, {});
    }
    if (mouse_event.GetType() == WebInputEvent::Type::kMouseDown) {
      return OverlayMainFrame()->GetEventHandler().HandleMousePressEvent(
          mouse_event);
    }
    if (mouse_event.GetType() == WebInputEvent::Type::kMouseUp) {
      return OverlayMainFrame()->GetEventHandler().HandleMouseReleaseEvent(
          mouse_event);
    }
    return WebInputEventResult::kNotHandled;
  }

  if (WebInputEvent::IsPointerEventType(input_event.GetType())) {
    return OverlayMainFrame()->GetEventHandler().HandlePointerEvent(
        static_cast<const WebPointerEvent&>(input_event),
        Vector<WebPointerEvent>(), Vector<WebPointerEvent>());
  }

  if (WebInputEvent::IsKeyboardEventType(input_event.GetType())) {
    return OverlayMainFrame()->GetEventHandler().KeyEvent(
        static_cast<const WebKeyboardEvent&>(input_event));
  }

  if (input_event.GetType() == WebInputEvent::Type::kMouseWheel) {
    return OverlayMainFrame()->GetEventHandler().HandleWheelEvent(
        static_cast<const WebMouseWheelEvent&>(input_event));
  }

  return WebInputEventResult::kNotHandled;
}

void InspectorOverlayAgent::ScheduleUpdate() {
  if (IsVisible()) {
    GetFrame()->GetPage()->GetChromeClient().ScheduleAnimation(
        GetFrame()->View());
  }
}

void InspectorOverlayAgent::PaintOverlayPage() {
  DCHECK(overlay_page_);

  LocalFrameView* view = frame_impl_->GetFrameView();
  LocalFrame* frame = GetFrame();
  if (!view || !frame) {
    return;
  }

  LocalFrame* overlay_frame = OverlayMainFrame();
  blink::VisualViewport& visual_viewport =
      frame->GetPage()->GetVisualViewport();
  // The emulation scale factor is backed in the overlay frame.
  gfx::Size viewport_size =
      gfx::ScaleToCeiledSize(visual_viewport.Size(), EmulationScaleFactor());
  overlay_frame->SetLayoutZoomFactor(WindowToViewportScale());
  overlay_frame->View()->Resize(viewport_size);
  OverlayMainFrame()->View()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kInspector);

  Reset(viewport_size, frame->View()->ViewportSizeForMediaQueries());

  float scale = WindowToViewportScale();

  if (inspect_tool_) {
    // Skip drawing persistent_tool_ on page scroll.
    if (!(inspect_tool_ == persistent_tool_ && is_page_scrolling_)) {
      inspect_tool_->Draw(scale);
    }
    if (persistent_tool_ && inspect_tool_->SupportsPersistentOverlays() &&
        !is_page_scrolling_) {
      persistent_tool_->Draw(scale);
    }
  }

  if (hinge_) {
    hinge_->Draw(scale);
  }

  EvaluateInOverlay("drawingFinished", "");

  OverlayMainFrame()->View()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kInspector);
}

float InspectorOverlayAgent::EmulationScaleFactor() const {
  return GetFrame()
      ->GetPage()
      ->GetChromeClient()
      .InputEventsScaleForEmulation();
}

void InspectorOverlayAgent::DidInitializeFrameWidget() {
  if (original_layer_tree_debug_state_) {
    return;
  }

  original_layer_tree_debug_state_ = std::make_unique<cc::LayerTreeDebugState>(
      *GetFrame()->GetWidgetForLocalRoot()->GetLayerTreeDebugState());
  Restore();
}

bool InspectorOverlayAgent::FrameWidgetInitialized() const {
  return !!original_layer_tree_debug_state_;
}

static std::unique_ptr<protocol::DictionaryValue> BuildObjectForSize(
    const gfx::Size& size) {
  std::unique_ptr<protocol::DictionaryValue> result =
      protocol::DictionaryValue::create();
  result->setInteger("width", size.width());
  result->setInteger("height", size.height());
  return result;
}

static std::unique_ptr<protocol::DictionaryValue> BuildObjectForSize(
    const gfx::SizeF& size) {
  std::unique_ptr<protocol::DictionaryValue> result =
      protocol::DictionaryValue::create();
  result->setDouble("width", size.width());
  result->setDouble("height", size.height());
  return result;
}

float InspectorOverlayAgent::WindowToViewportScale() const {
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return 1.0f;
  }
  return frame->GetPage()->GetChromeClient().WindowToViewportScalar(frame,
                                                                    1.0f);
}

void InspectorOverlayAgent::LoadOverlayPageResource() {
  if (overlay_page_) {
    return;
  }

  ScriptForbiddenScope::AllowUserAgentScript allow_script;

  DCHECK(!overlay_chrome_client_);
  overlay_chrome_client_ = MakeGarbageCollected<InspectorOverlayChromeClient>(
      GetFrame()->GetPage()->GetChromeClient(), *this);
  overlay_page_ = Page::CreateNonOrdinary(
      *overlay_chrome_client_,
      *GetFrame()->GetFrameScheduler()->GetAgentGroupScheduler(),
      &GetFrame()->GetPage()->GetColorProviderColorMaps());
  overlay_host_ = MakeGarbageCollected<InspectorOverlayHost>(this);

  Settings& settings = GetFrame()->GetPage()->GetSettings();
  Settings& overlay_settings = overlay_page_->GetSettings();

  overlay_settings.GetGenericFontFamilySettings().UpdateStandard(
      settings.GetGenericFontFamilySettings().Standard());
  overlay_settings.GetGenericFontFamilySettings().UpdateFixed(
      settings.GetGenericFontFamilySettings().Fixed());
  overlay_settings.GetGenericFontFamilySettings().UpdateSerif(
      settings.GetGenericFontFamilySettings().Serif());
  overlay_settings.GetGenericFontFamilySettings().UpdateSansSerif(
      settings.GetGenericFontFamilySettings().SansSerif());
  overlay_settings.GetGenericFontFamilySettings().UpdateCursive(
      settings.GetGenericFontFamilySettings().Cursive());
  overlay_settings.GetGenericFontFamilySettings().UpdateFantasy(
      settings.GetGenericFontFamilySettings().Fantasy());
  overlay_settings.GetGenericFontFamilySettings().UpdateMath(
      settings.GetGenericFontFamilySettings().Math());
  overlay_settings.SetMinimumFontSize(settings.GetMinimumFontSize());
  overlay_settings.SetMinimumLogicalFontSize(
      settings.GetMinimumLogicalFontSize());
  overlay_settings.SetScriptEnabled(true);
  overlay_settings.SetPluginsEnabled(false);
  overlay_settings.SetLoadsImagesAutomatically(true);

  DEFINE_STATIC_LOCAL(Persistent<LocalFrameClient>, dummy_local_frame_client,
                      (MakeGarbageCollected<EmptyLocalFrameClient>()));
  auto* frame = MakeGarbageCollected<LocalFrame>(
      dummy_local_frame_client, *overlay_page_, nullptr, nullptr, nullptr,
      FrameInsertType::kInsertInConstructor, LocalFrameToken(), nullptr,
      nullptr, mojo::NullRemote());
  frame->SetView(MakeGarbageCollected<LocalFrameView>(*frame));
  frame->Init(/*opener=*/nullptr, DocumentToken(), /*policy_container=*/nullptr,
              StorageKey(), /*document_ukm_source_id=*/ukm::kInvalidSourceId,
              /*creator_base_url=*/KURL());
  frame->View()->SetCanHaveScrollbars(false);
  frame->View()->SetBaseBackgroundColor(Color::kTransparent);

  SegmentedBuffer data;

  data.Append("<script>", static_cast<size_t>(8));
  data.Append(UncompressResourceAsBinary(IDR_INSPECT_TOOL_MAIN_JS));
  data.Append("</script>", static_cast<size_t>(9));

  frame->ForceSynchronousDocumentInstall(AtomicString("text/html"),
                                         std::move(data));

  v8::Isolate* isolate = ToIsolate(frame);
  ScriptState* script_state = ToScriptStateForMainWorld(frame);
  DCHECK(script_state);
  ScriptState::Scope scope(script_state);
  v8::MicrotasksScope microtasks_scope(
      isolate, ToMicrotaskQueue(script_state),
      v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Local<v8::Value> overlay_host_obj =
      ToV8Traits<InspectorOverlayHost>::ToV8(script_state, overlay_host_.Get());
  DCHECK(!overlay_host_obj.IsEmpty());
  script_state->GetContext()
      ->Global()
      ->Set(script_state->GetContext(),
            V8AtomicString(isolate, "InspectorOverlayHost"), overlay_host_obj)
      .ToChecked();

#if BUILDFLAG(IS_WIN)
  EvaluateInOverlay("setPlatform", "windows");
#elif BUILDFLAG(IS_MAC)
  EvaluateInOverlay("setPlatform", "mac");
#elif BUILDFLAG(IS_POSIX)
  EvaluateInOverlay("setPlatform", "linux");
#else
  EvaluateInOverlay("setPlatform", "other");
#endif
}

LocalFrame* InspectorOverlayAgent::OverlayMainFrame() {
  DCHECK(overlay_page_);
  return To<LocalFrame>(overlay_page_->MainFrame());
}

void InspectorOverlayAgent::Reset(
    const gfx::Size& viewport_size,
    const gfx::SizeF& viewport_size_for_media_queries) {
  std::unique_ptr<protocol::DictionaryValue> reset_data =
      protocol::DictionaryValue::create();
  reset_data->setDouble("deviceScaleFactor", WindowToViewportScale());
  reset_data->setDouble("emulationScaleFactor", EmulationScaleFactor());
  reset_data->setDouble("pageScaleFactor",
                        GetFrame()->GetPage()->GetVisualViewport().Scale());

  float physical_to_dips =
      1.f / GetFrame()->GetPage()->GetChromeClient().WindowToViewportScalar(
                GetFrame(), 1.f);
  gfx::Size viewport_size_in_dips =
      gfx::ScaleToFlooredSize(viewport_size, physical_to_dips);

  reset_data->setObject("viewportSize",
                        BuildObjectForSize(viewport_size_in_dips));
  reset_data->setObject("viewportSizeForMediaQueries",
                        BuildObjectForSize(viewport_size_for_media_queries));

  // The zoom factor in the overlay frame already has been multiplied by the
  // window to viewport scale (aka device scale factor), so cancel it.
  reset_data->setDouble("pageZoomFactor", GetFrame()->LayoutZoomFactor() /
                                              WindowToViewportScale());

  // TODO(szager): These values have been zero since root layer scrolling
  // landed. Probably they should be derived from
  // LocalFrameView::LayoutViewport(); but I have no idea who the consumers
  // of these values are, so I'm leaving them zero pending investigation.
  reset_data->setInteger("scrollX", 0);
  reset_data->setInteger("scrollY", 0);
  EvaluateInOverlay("reset", std::move(reset_data));
}

void InspectorOverlayAgent::EvaluateInOverlay(const String& method,
                                              const String& argument) {
  ScriptForbiddenScope::AllowUserAgentScript allow_script;
  v8::HandleScope handle_scope(ToIsolate(OverlayMainFrame()));

  LocalFrame* local_frame = To<LocalFrame>(OverlayMainFrame());
  ScriptState* script_state = ToScriptStateForMainWorld(local_frame);
  DCHECK(script_state);

  v8::Local<v8::Context> context = script_state->GetContext();
  v8::Context::Scope context_scope(context);

  v8::LocalVector<v8::Value> args(context->GetIsolate());
  int args_length = 2;
  v8::Local<v8::Array> params(
      v8::Array::New(context->GetIsolate(), args_length));
  v8::Local<v8::Value> local_method(V8String(context->GetIsolate(), method));
  v8::Local<v8::Value> local_argument(
      V8String(context->GetIsolate(), argument));
  params->CreateDataProperty(context, 0, local_method).Check();
  params->CreateDataProperty(context, 1, local_argument).Check();
  args.push_back(params);

  v8::Local<v8::Value> v8_method;
  if (!GetV8Property(context, context->Global(), "dispatch")
           .ToLocal(&v8_method) ||
      v8_method->IsUndefined()) {
    return;
  }

  local_frame->DomWindow()->GetScriptController().EvaluateMethodInMainWorld(
      v8::Local<v8::Function>::Cast(v8_method), context->Global(),
      static_cast<int>(args.size()), args.data());
}

void InspectorOverlayAgent::EvaluateInOverlay(
    const String& method,
    std::unique_ptr<protocol::Value> argument) {
  ScriptForbiddenScope::AllowUserAgentScript allow_script;
  std::unique_ptr<protocol::ListValue> command = protocol::ListValue::create();
  command->pushValue(protocol::StringValue::create(method));
  command->pushValue(std::move(argument));
  std::vector<uint8_t> json;
  ConvertCBORToJSON(SpanFrom(command->Serialize()), &json);
  ClassicScript::CreateUnspecifiedScript(
      "dispatch(" +
          String(reinterpret_cast<const char*>(json.data()), json.size()) + ")",
      ScriptSourceLocationType::kInspector)
      ->RunScript(To<LocalFrame>(OverlayMainFrame())->DomWindow(),
                  ExecuteScriptPolicy::kExecuteScriptWhenScriptsDisabled);
}

String InspectorOverlayAgent::EvaluateInOverlayForTest(const String& script) {
  ScriptForbiddenScope::AllowUserAgentScript allow_script;
  v8::Isolate* isolate = ToIsolate(OverlayMainFrame());
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Value> string =
      ClassicScript::CreateUnspecifiedScript(
          script, ScriptSourceLocationType::kInspector)
          ->RunScriptAndReturnValue(
              To<LocalFrame>(OverlayMainFrame())->DomWindow(),
              ExecuteScriptPolicy::kExecuteScriptWhenScriptsDisabled)
          .GetSuccessValueOrEmpty();
  return ToCoreStringWithUndefinedOrNullCheck(isolate, string);
}

void InspectorOverlayAgent::OnResizeTimer(TimerBase*) {
  if (resize_timer_active_) {
    // Restore the original tool.
    PickTheRightTool();
    return;
  }

  // Show the resize tool.
  SetInspectTool(MakeGarbageCollected<ShowViewSizeTool>(this, GetFrontend()));
  resize_timer_active_ = true;
  resize_timer_.Stop();
  resize_timer_.StartOneShot(base::Seconds(1), FROM_HERE);
}

void InspectorOverlayAgent::Dispatch(const ScriptValue& message,
                                     ExceptionState& exception_state) {
  inspect_tool_->Dispatch(message, exception_state);
}

void InspectorOverlayAgent::PageLayoutInvalidated(bool resized) {
  if (resized && show_size_on_resize_.Get()) {
    resize_timer_active_ = false;
    // Handle the resize in the next cycle to decouple overlay page rebuild from
    // the main page layout to avoid document lifecycle issues caused by
    // EventLoop::PerformCheckpoint() called when we rebuild the overlay page.
    resize_timer_.Stop();
    resize_timer_.StartOneShot(base::Seconds(0), FROM_HERE);
    return;
  }
  ScheduleUpdate();
}

protocol::Response InspectorOverlayAgent::CompositingEnabled() {
  bool main_frame = frame_impl_->ViewImpl() && !frame_impl_->Parent();
  if (!main_frame || !frame_impl_->ViewImpl()
                          ->GetPage()
                          ->GetSettings()
                          .GetAcceleratedCompositingEnabled()) {
    return protocol::Response::ServerError("Compositing mode is not supported");
  }
  return protocol::Response::Success();
}

bool InspectorOverlayAgent::InSomeInspectMode() {
  return inspect_mode_.Get() != protocol::Overlay::InspectModeEnum::None;
}

void InspectorOverlayAgent::Inspect(Node* inspected_node) {
  if (!inspected_node) {
    return;
  }

  Node* node = inspected_node;
  while (node && !node->IsElementNode() && !node->IsDocumentNode() &&
         !node->IsDocumentFragment()) {
    node = node->ParentOrShadowHostNode();
  }
  if (!node) {
    return;
  }

  DOMNodeId backend_node_id = node->GetDomNodeId();
  if (!enabled_.Get()) {
    backend_node_id_to_inspect_ = backend_node_id;
    return;
  }

  GetFrontend()->inspectNodeRequested(IdentifiersFactory::IntIdForNode(node));
}

protocol::Response InspectorOverlayAgent::setInspectMode(
    const String& mode,
    Maybe<protocol::Overlay::HighlightConfig> highlight_inspector_object) {
  if (mode != protocol::Overlay::InspectModeEnum::None &&
      mode != protocol::Overlay::InspectModeEnum::SearchForNode &&
      mode != protocol::Overlay::InspectModeEnum::SearchForUAShadowDOM &&
      mode != protocol::Overlay::InspectModeEnum::CaptureAreaScreenshot &&
      mode != protocol::Overlay::InspectModeEnum::ShowDistances) {
    return protocol::Response::ServerError(
        String("Unknown mode \"" + mode + "\" was provided.").Utf8());
  }

  std::vector<uint8_t> serialized_config;
  if (highlight_inspector_object.has_value()) {
    highlight_inspector_object.value().AppendSerialized(&serialized_config);
  }
  std::unique_ptr<InspectorHighlightConfig> config;
  protocol::Response response = HighlightConfigFromInspectorObject(
      std::move(highlight_inspector_object), &config);
  if (!response.IsSuccess()) {
    return response;
  }
  inspect_mode_.Set(mode);
  inspect_mode_protocol_config_.Set(serialized_config);
  PickTheRightTool();
  return protocol::Response::Success();
}

void InspectorOverlayAgent::PickTheRightTool() {
  InspectTool* inspect_tool = nullptr;

  if (persistent_tool_ && persistent_tool_->IsEmpty()) {
    persistent_tool_ = nullptr;
  }

  String inspect_mode = inspect_mode_.Get();
  if (inspect_mode == protocol::Overlay::InspectModeEnum::SearchForNode ||
      inspect_mode ==
          protocol::Overlay::InspectModeEnum::SearchForUAShadowDOM) {
    inspect_tool = MakeGarbageCollected<SearchingForNodeTool>(
        this, GetFrontend(), dom_agent_,
        inspect_mode ==
            protocol::Overlay::InspectModeEnum::SearchForUAShadowDOM,
        inspect_mode_protocol_config_.Get());
  } else if (inspect_mode ==
             protocol::Overlay::InspectModeEnum::CaptureAreaScreenshot) {
    inspect_tool = MakeGarbageCollected<ScreenshotTool>(this, GetFrontend());
  } else if (inspect_mode ==
             protocol::Overlay::InspectModeEnum::ShowDistances) {
    inspect_tool =
        MakeGarbageCollected<NearbyDistanceTool>(this, GetFrontend());
  } else if (!paused_in_debugger_message_.Get().IsNull()) {
    inspect_tool = MakeGarbageCollected<PausedInDebuggerTool>(
        this, GetFrontend(), v8_session_, paused_in_debugger_message_.Get());
  } else if (persistent_tool_) {
    inspect_tool = persistent_tool_;
  }

  SetInspectTool(inspect_tool);
}

void InspectorOverlayAgent::DisableFrameOverlay() {
  if (IsVisible() || !frame_overlay_) {
    return;
  }

  frame_overlay_.Release()->Destroy();
  auto& client = GetFrame()->GetPage()->GetChromeClient();
  client.SetCursorOverridden(false);
  client.SetCursor(PointerCursor(), GetFrame());

  if (auto* frame_view = frame_impl_->GetFrameView()) {
    frame_view->SetPaintArtifactCompositorNeedsUpdate();
  }
}

void InspectorOverlayAgent::EnsureEnableFrameOverlay() {
  if (frame_overlay_) {
    return;
  }

  frame_overlay_ = MakeGarbageCollected<FrameOverlay>(
      GetFrame(), std::make_unique<InspectorPageOverlayDelegate>(*this));
}

void InspectorOverlayAgent::ClearInspectTool() {
  inspect_tool_ = nullptr;
  if (!hinge_) {
    DisableFrameOverlay();
  }
}

protocol::Response InspectorOverlayAgent::SetInspectTool(
    InspectTool* inspect_tool) {
  ClearInspectTool();

  if (!inspect_tool) {
    return protocol::Response::Success();
  }

  if (!enabled_.Get()) {
    return protocol::Response::InvalidRequest(
        "Overlay must be enabled before a tool can be shown");
  }

  LocalFrameView* view = frame_impl_->GetFrameView();
  LocalFrame* frame = GetFrame();
  if (!view || !frame) {
    return protocol::Response::InternalError();
  }

  inspect_tool_ = inspect_tool;
  // If the tool supports persistent overlays, the resources of the persistent
  // tool will be included into the JS resource.
  LoadOverlayPageResource();
  EvaluateInOverlay("setOverlay", inspect_tool->GetOverlayName());
  EnsureEnableFrameOverlay();
  EnsureAXContext(frame->GetDocument());
  ScheduleUpdate();
  return protocol::Response::Success();
}

InspectorSourceOrderConfig
InspectorOverlayAgent::SourceOrderConfigFromInspectorObject(
    std::unique_ptr<protocol::Overlay::SourceOrderConfig>
        source_order_inspector_object) {
  InspectorSourceOrderConfig source_order_config = InspectorSourceOrderConfig();
  source_order_config.parent_outline_color =
      ParseColor(source_order_inspector_object->getParentOutlineColor());
  source_order_config.child_outline_color =
      ParseColor(source_order_inspector_object->getChildOutlineColor());

  return source_order_config;
}

protocol::Response InspectorOverlayAgent::HighlightConfigFromInspectorObject(
    Maybe<protocol::Overlay::HighlightConfig> highlight_inspector_object,
    std::unique_ptr<InspectorHighlightConfig>* out_config) {
  if (!highlight_inspector_object.has_value()) {
    return protocol::Response::ServerError(
        "Internal error: highlight configuration parameter is missing");
  }

  protocol::Overlay::HighlightConfig& config =
      highlight_inspector_object.value();

  String format = config.getColorFormat("hex");

  namespace ColorFormatEnum = protocol::Overlay::ColorFormatEnum;
  if (format != ColorFormatEnum::Rgb && format != ColorFormatEnum::Hex &&
      format != ColorFormatEnum::Hsl && format != ColorFormatEnum::Hwb) {
    return protocol::Response::InvalidParams("Unknown color format");
  }

  *out_config = InspectorOverlayAgent::ToHighlightConfig(&config);
  return protocol::Response::Success();
}

// static
std::unique_ptr<InspectorGridHighlightConfig>
InspectorOverlayAgent::ToGridHighlightConfig(
    protocol::Overlay::GridHighlightConfig* config) {
  if (!config) {
    return nullptr;
  }
  std::unique_ptr<InspectorGridHighlightConfig> highlight_config =
      std::make_unique<InspectorGridHighlightConfig>();
  highlight_config->show_positive_line_numbers =
      config->getShowPositiveLineNumbers(false);
  highlight_config->show_negative_line_numbers =
      config->getShowNegativeLineNumbers(false);
  highlight_config->show_area_names = config->getShowAreaNames(false);
  highlight_config->show_line_names = config->getShowLineNames(false);
  highlight_config->show_grid_extension_lines =
      config->getShowGridExtensionLines(false);
  highlight_config->grid_border_dash = config->getGridBorderDash(false);

  // cellBorderDash is deprecated. We only use it if defined and none of the new
  // properties are.
  bool hasLegacyBorderDash = !config->hasRowLineDash() &&
                             !config->hasColumnLineDash() &&
                             config->hasCellBorderDash();
  highlight_config->row_line_dash = hasLegacyBorderDash
                                        ? config->getCellBorderDash(false)
                                        : config->getRowLineDash(false);
  highlight_config->column_line_dash = hasLegacyBorderDash
                                           ? config->getCellBorderDash(false)
                                           : config->getColumnLineDash(false);

  highlight_config->show_track_sizes = config->getShowTrackSizes(false);
  highlight_config->grid_color =
      ParseColor(config->getGridBorderColor(nullptr));

  // cellBorderColor is deprecated. We only use it if defined and none of the
  // new properties are.
  bool hasLegacyBorderColors = !config->hasRowLineColor() &&
                               !config->hasColumnLineColor() &&
                               config->hasCellBorderColor();
  highlight_config->row_line_color =
      hasLegacyBorderColors ? ParseColor(config->getCellBorderColor(nullptr))
                            : ParseColor(config->getRowLineColor(nullptr));
  highlight_config->column_line_color =
      hasLegacyBorderColors ? ParseColor(config->getCellBorderColor(nullptr))
                            : ParseColor(config->getColumnLineColor(nullptr));

  highlight_config->row_gap_color = ParseColor(config->getRowGapColor(nullptr));
  highlight_config->column_gap_color =
      ParseColor(config->getColumnGapColor(nullptr));
  highlight_config->row_hatch_color =
      ParseColor(config->getRowHatchColor(nullptr));
  highlight_config->column_hatch_color =
      ParseColor(config->getColumnHatchColor(nullptr));
  highlight_config->area_border_color =
      ParseColor(config->getAreaBorderColor(nullptr));
  highlight_config->grid_background_color =
      ParseColor(config->getGridBackgroundColor(nullptr));
  return highlight_config;
}

// static
std::unique_ptr<InspectorFlexContainerHighlightConfig>
InspectorOverlayAgent::ToFlexContainerHighlightConfig(
    protocol::Overlay::FlexContainerHighlightConfig* config) {
  if (!config) {
    return nullptr;
  }
  std::unique_ptr<InspectorFlexContainerHighlightConfig> highlight_config =
      std::make_unique<InspectorFlexContainerHighlightConfig>();
  highlight_config->container_border =
      InspectorOverlayAgent::ToLineStyle(config->getContainerBorder(nullptr));
  highlight_config->line_separator =
      InspectorOverlayAgent::ToLineStyle(config->getLineSeparator(nullptr));
  highlight_config->item_separator =
      InspectorOverlayAgent::ToLineStyle(config->getItemSeparator(nullptr));

  highlight_config->main_distributed_space = InspectorOverlayAgent::ToBoxStyle(
      config->getMainDistributedSpace(nullptr));
  highlight_config->cross_distributed_space = InspectorOverlayAgent::ToBoxStyle(
      config->getCrossDistributedSpace(nullptr));
  highlight_config->row_gap_space =
      InspectorOverlayAgent::ToBoxStyle(config->getRowGapSpace(nullptr));
  highlight_config->column_gap_space =
      InspectorOverlayAgent::ToBoxStyle(config->getColumnGapSpace(nullptr));
  highlight_config->cross_alignment =
      InspectorOverlayAgent::ToLineStyle(config->getCrossAlignment(nullptr));

  return highlight_config;
}

// static
std::unique_ptr<InspectorScrollSnapContainerHighlightConfig>
InspectorOverlayAgent::ToScrollSnapContainerHighlightConfig(
    protocol::Overlay::ScrollSnapContainerHighlightConfig* config) {
  if (!config) {
    return nullptr;
  }
  std::unique_ptr<InspectorScrollSnapContainerHighlightConfig>
      highlight_config =
          std::make_unique<InspectorScrollSnapContainerHighlightConfig>();
  highlight_config->snapport_border =
      InspectorOverlayAgent::ToLineStyle(config->getSnapportBorder(nullptr));
  highlight_config->snap_area_border =
      InspectorOverlayAgent::ToLineStyle(config->getSnapAreaBorder(nullptr));

  highlight_config->scroll_margin_color =
      ParseColor(config->getScrollMarginColor(nullptr));
  highlight_config->scroll_padding_color =
      ParseColor(config->getScrollPaddingColor(nullptr));

  return highlight_config;
}

// static
std::unique_ptr<InspectorContainerQueryContainerHighlightConfig>
InspectorOverlayAgent::ToContainerQueryContainerHighlightConfig(
    protocol::Overlay::ContainerQueryContainerHighlightConfig* config) {
  if (!config) {
    return nullptr;
  }
  std::unique_ptr<InspectorContainerQueryContainerHighlightConfig>
      highlight_config =
          std::make_unique<InspectorContainerQueryContainerHighlightConfig>();
  highlight_config->container_border =
      InspectorOverlayAgent::ToLineStyle(config->getContainerBorder(nullptr));
  highlight_config->descendant_border =
      InspectorOverlayAgent::ToLineStyle(config->getDescendantBorder(nullptr));

  return highlight_config;
}

// static
std::unique_ptr<InspectorFlexItemHighlightConfig>
InspectorOverlayAgent::ToFlexItemHighlightConfig(
    protocol::Overlay::FlexItemHighlightConfig* config) {
  if (!config) {
    return nullptr;
  }
  std::unique_ptr<InspectorFlexItemHighlightConfig> highlight_config =
      std::make_unique<InspectorFlexItemHighlightConfig>();

  highlight_config->base_size_box =
      InspectorOverlayAgent::ToBoxStyle(config->getBaseSizeBox(nullptr));
  highlight_config->base_size_border =
      InspectorOverlayAgent::ToLineStyle(config->getBaseSizeBorder(nullptr));
  highlight_config->flexibility_arrow =
      InspectorOverlayAgent::ToLineStyle(config->getFlexibilityArrow(nullptr));

  return highlight_config;
}

// static
std::unique_ptr<InspectorIsolationModeHighlightConfig>
InspectorOverlayAgent::ToIsolationModeHighlightConfig(
    protocol::Overlay::IsolationModeHighlightConfig* config,
    int idx) {
  if (!config) {
    return nullptr;
  }
  std::unique_ptr<InspectorIsolationModeHighlightConfig> highlight_config =
      std::make_unique<InspectorIsolationModeHighlightConfig>();
  highlight_config->resizer_color =
      ParseColor(config->getResizerColor(nullptr));
  highlight_config->resizer_handle_color =
      ParseColor(config->getResizerHandleColor(nullptr));
  highlight_config->mask_color = ParseColor(config->getMaskColor(nullptr));
  highlight_config->highlight_index = idx;

  return highlight_config;
}

// static
std::optional<LineStyle> InspectorOverlayAgent::ToLineStyle(
    protocol::Overlay::LineStyle* config) {
  if (!config) {
    return std::nullopt;
  }
  std::optional<LineStyle> line_style = LineStyle();
  line_style->color = ParseColor(config->getColor(nullptr));
  line_style->pattern = config->getPattern("solid");

  return line_style;
}

// static
std::optional<BoxStyle> InspectorOverlayAgent::ToBoxStyle(
    protocol::Overlay::BoxStyle* config) {
  if (!config) {
    return std::nullopt;
  }
  std::optional<BoxStyle> box_style = BoxStyle();
  box_style->fill_color = ParseColor(config->getFillColor(nullptr));
  box_style->hatch_color = ParseColor(config->getHatchColor(nullptr));

  return box_style;
}

ContrastAlgorithm GetContrastAlgorithm(const String& contrast_algorithm) {
  namespace ContrastAlgorithmEnum = protocol::Overlay::ContrastAlgorithmEnum;
  if (contrast_algorithm == ContrastAlgorithmEnum::Aaa) {
    return ContrastAlgorithm::kAaa;
  } else if (contrast_algorithm == ContrastAlgorithmEnum::Apca) {
    return ContrastAlgorithm::kApca;
  } else {
    return ContrastAlgorithm::kAa;
  }
}

// static
std::unique_ptr<InspectorHighlightConfig>
InspectorOverlayAgent::ToHighlightConfig(
    protocol::Overlay::HighlightConfig* config) {
  std::unique_ptr<InspectorHighlightConfig> highlight_config =
      std::make_unique<InspectorHighlightConfig>();
  highlight_config->show_info = config->getShowInfo(false);
  highlight_config->show_accessibility_info =
      config->getShowAccessibilityInfo(true);
  highlight_config->show_styles = config->getShowStyles(false);
  highlight_config->show_rulers = config->getShowRulers(false);
  highlight_config->show_extension_lines = config->getShowExtensionLines(false);
  highlight_config->content = ParseColor(config->getContentColor(nullptr));
  highlight_config->padding = ParseColor(config->getPaddingColor(nullptr));
  highlight_config->border = ParseColor(config->getBorderColor(nullptr));
  highlight_config->margin = ParseColor(config->getMarginColor(nullptr));
  highlight_config->event_target =
      ParseColor(config->getEventTargetColor(nullptr));
  highlight_config->shape = ParseColor(config->getShapeColor(nullptr));
  highlight_config->shape_margin =
      ParseColor(config->getShapeMarginColor(nullptr));
  highlight_config->css_grid = ParseColor(config->getCssGridColor(nullptr));

  namespace ColorFormatEnum = protocol::Overlay::ColorFormatEnum;

  String format = config->getColorFormat("hex");

  if (format == ColorFormatEnum::Hsl) {
    highlight_config->color_format = ColorFormat::kHsl;
  } else if (format == ColorFormatEnum::Hwb) {
    highlight_config->color_format = ColorFormat::kHwb;
  } else if (format == ColorFormatEnum::Rgb) {
    highlight_config->color_format = ColorFormat::kRgb;
  } else {
    highlight_config->color_format = ColorFormat::kHex;
  }

  namespace ContrastAlgorithmEnum = protocol::Overlay::ContrastAlgorithmEnum;
  highlight_config->contrast_algorithm = GetContrastAlgorithm(
      config->getContrastAlgorithm(ContrastAlgorithmEnum::Aa));

  highlight_config->grid_highlight_config =
      InspectorOverlayAgent::ToGridHighlightConfig(
          config->getGridHighlightConfig(nullptr));

  highlight_config->flex_container_highlight_config =
      InspectorOverlayAgent::ToFlexContainerHighlightConfig(
          config->getFlexContainerHighlightConfig(nullptr));

  highlight_config->flex_item_highlight_config =
      InspectorOverlayAgent::ToFlexItemHighlightConfig(
          config->getFlexItemHighlightConfig(nullptr));

  highlight_config->container_query_container_highlight_config =
      InspectorOverlayAgent::ToContainerQueryContainerHighlightConfig(
          config->getContainerQueryContainerHighlightConfig(nullptr));

  return highlight_config;
}

void InspectorOverlayAgent::SetNeedsUnbufferedInput(bool unbuffered) {
  LocalFrame* frame = GetFrame();
  if (frame) {
    frame->GetPage()->GetChromeClient().SetNeedsUnbufferedInputForDebugger(
        frame, unbuffered);
  }
}

}  // namespace blink
