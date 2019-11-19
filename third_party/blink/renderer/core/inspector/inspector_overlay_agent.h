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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_OVERLAY_AGENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_OVERLAY_AGENT_H_

#include <v8-inspector.h>
#include <memory>
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/inspector/inspector_base_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_highlight.h"
#include "third_party/blink/renderer/core/inspector/inspector_overlay_host.h"
#include "third_party/blink/renderer/core/inspector/protocol/Overlay.h"
#include "third_party/blink/renderer/platform/geometry/float_quad.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace cc {
class Layer;
}

namespace blink {

class FrameOverlay;
class GraphicsContext;
class InspectedFrames;
class InspectorDOMAgent;
class LocalFrame;
class LocalFrameView;
class Node;
class Page;
class WebGestureEvent;
class WebKeyboardEvent;
class WebMouseEvent;
class WebLocalFrameImpl;
class WebPointerEvent;

class InspectorOverlayAgent;

using OverlayFrontend = protocol::Overlay::Metainfo::FrontendClass;

class CORE_EXPORT InspectTool : public GarbageCollected<InspectTool> {
 public:
  virtual ~InspectTool() = default;
  void Init(InspectorOverlayAgent* overlay, OverlayFrontend* frontend);
  virtual int GetDataResourceId();
  virtual bool HandleInputEvent(LocalFrameView* frame_view,
                                const WebInputEvent& input_event,
                                bool* swallow_next_mouse_up);
  virtual bool HandleMouseEvent(const WebMouseEvent&,
                                bool* swallow_next_mouse_up);
  virtual bool HandleMouseDown(const WebMouseEvent&,
                               bool* swallow_next_mouse_up);
  virtual bool HandleMouseUp(const WebMouseEvent&);
  virtual bool HandleMouseMove(const WebMouseEvent&);
  virtual bool HandleGestureTapEvent(const WebGestureEvent&);
  virtual bool HandlePointerEvent(const WebPointerEvent&);
  virtual bool HandleKeyboardEvent(const WebKeyboardEvent&);
  virtual bool ForwardEventsToOverlay();
  virtual void Draw(float scale) {}
  virtual void Dispatch(const String& message) {}
  virtual void Trace(blink::Visitor* visitor);
  virtual void Dispose() {}
  virtual bool HideOnHideHighlight();

 protected:
  virtual void DoInit() {}
  Member<InspectorOverlayAgent> overlay_;
  OverlayFrontend* frontend_ = nullptr;
};

class CORE_EXPORT InspectorOverlayAgent final
    : public InspectorBaseAgent<protocol::Overlay::Metainfo>,
      public InspectorOverlayHost::Delegate {
  USING_GARBAGE_COLLECTED_MIXIN(InspectorOverlayAgent);

 public:
  static std::unique_ptr<InspectorHighlightConfig> ToHighlightConfig(
      protocol::Overlay::HighlightConfig*);
  InspectorOverlayAgent(WebLocalFrameImpl*,
                        InspectedFrames*,
                        v8_inspector::V8InspectorSession*,
                        InspectorDOMAgent*);
  ~InspectorOverlayAgent() override;
  void Trace(blink::Visitor*) override;

  // protocol::Dispatcher::OverlayCommandHandler implementation.
  protocol::Response enable() override;
  protocol::Response disable() override;
  protocol::Response setShowAdHighlights(bool) override;
  protocol::Response setShowPaintRects(bool) override;
  protocol::Response setShowLayoutShiftRegions(bool) override;
  protocol::Response setShowDebugBorders(bool) override;
  protocol::Response setShowFPSCounter(bool) override;
  protocol::Response setShowScrollBottleneckRects(bool) override;
  protocol::Response setShowHitTestBorders(bool) override;
  protocol::Response setShowViewportSizeOnResize(bool) override;
  protocol::Response setPausedInDebuggerMessage(
      protocol::Maybe<String>) override;
  protocol::Response setInspectMode(
      const String& mode,
      protocol::Maybe<protocol::Overlay::HighlightConfig>) override;
  protocol::Response highlightRect(
      int x,
      int y,
      int width,
      int height,
      protocol::Maybe<protocol::DOM::RGBA> color,
      protocol::Maybe<protocol::DOM::RGBA> outline_color) override;
  protocol::Response highlightQuad(
      std::unique_ptr<protocol::Array<double>> quad,
      protocol::Maybe<protocol::DOM::RGBA> color,
      protocol::Maybe<protocol::DOM::RGBA> outline_color) override;
  protocol::Response highlightNode(
      std::unique_ptr<protocol::Overlay::HighlightConfig>,
      protocol::Maybe<int> node_id,
      protocol::Maybe<int> backend_node_id,
      protocol::Maybe<String> object_id,
      protocol::Maybe<String> selector_list) override;
  protocol::Response hideHighlight() override;
  protocol::Response highlightFrame(
      const String& frame_id,
      protocol::Maybe<protocol::DOM::RGBA> content_color,
      protocol::Maybe<protocol::DOM::RGBA> content_outline_color) override;
  protocol::Response getHighlightObjectForTest(
      int node_id,
      protocol::Maybe<bool> include_distance,
      protocol::Maybe<bool> include_style,
      std::unique_ptr<protocol::DictionaryValue>* highlight) override;

  // InspectorBaseAgent overrides.
  void Restore() override;
  void Dispose() override;

  void Inspect(Node*);
  void DispatchBufferedTouchEvents();
  WebInputEventResult HandleInputEvent(const WebInputEvent&);
  WebInputEventResult HandleInputEventInOverlay(const WebInputEvent&);
  void PageLayoutInvalidated(bool resized);
  void EvaluateInOverlay(const String& method, const String& argument);
  void EvaluateInOverlay(const String& method,
                         std::unique_ptr<protocol::Value> argument);
  String EvaluateInOverlayForTest(const String&);

  void UpdatePrePaint();
  // For CompositeAfterPaint.
  void PaintOverlay(GraphicsContext&);

  bool IsInspectorLayer(const cc::Layer*) const;

  LocalFrame* GetFrame() const;
  float WindowToViewportScale() const;
  void ScheduleUpdate();

 private:
  class InspectorOverlayChromeClient;
  class InspectorPageOverlayDelegate;

  // InspectorOverlayHost::Delegate implementation.
  void Dispatch(const String& message) override;

  bool IsEmpty();

  void EnsureOverlayPageCreated();
  LocalFrame* OverlayMainFrame();
  void Reset(const IntSize& viewport_size);
  void OnResizeTimer(TimerBase*);
  void PaintOverlayPage();

  protocol::Response CompositingEnabled();

  bool InSomeInspectMode();

  void SetNeedsUnbufferedInput(bool unbuffered);
  void PickTheRightTool();
  void SetInspectTool(InspectTool* inspect_tool);
  void LoadFrameForTool();
  protocol::Response HighlightConfigFromInspectorObject(
      protocol::Maybe<protocol::Overlay::HighlightConfig>
          highlight_inspector_object,
      std::unique_ptr<InspectorHighlightConfig>*);

  Member<WebLocalFrameImpl> frame_impl_;
  Member<InspectedFrames> inspected_frames_;
  Member<Page> overlay_page_;
  int frame_resource_name_;
  Member<InspectorOverlayChromeClient> overlay_chrome_client_;
  Member<InspectorOverlayHost> overlay_host_;
  bool resize_timer_active_;
  TaskRunnerTimer<InspectorOverlayAgent> resize_timer_;
  bool disposed_;
  v8_inspector::V8InspectorSession* v8_session_;
  Member<InspectorDOMAgent> dom_agent_;
  std::unique_ptr<FrameOverlay> frame_overlay_;
  Member<InspectTool> inspect_tool_;
  bool swallow_next_mouse_up_;
  bool swallow_next_escape_up_;
  DOMNodeId backend_node_id_to_inspect_;
  InspectorAgentState::Boolean enabled_;
  InspectorAgentState::Boolean show_ad_highlights_;
  InspectorAgentState::Boolean show_debug_borders_;
  InspectorAgentState::Boolean show_fps_counter_;
  InspectorAgentState::Boolean show_paint_rects_;
  InspectorAgentState::Boolean show_layout_shift_regions_;
  InspectorAgentState::Boolean show_scroll_bottleneck_rects_;
  InspectorAgentState::Boolean show_hit_test_borders_;
  InspectorAgentState::Boolean show_size_on_resize_;
  InspectorAgentState::String paused_in_debugger_message_;
  InspectorAgentState::String inspect_mode_;
  InspectorAgentState::Bytes inspect_mode_protocol_config_;
  DISALLOW_COPY_AND_ASSIGN(InspectorOverlayAgent);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_OVERLAY_AGENT_H_
