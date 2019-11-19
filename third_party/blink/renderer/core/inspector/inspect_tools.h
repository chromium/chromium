// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECT_TOOLS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECT_TOOLS_H_

#include <vector>
#include <v8-inspector.h>
#include "base/macros.h"
#include "third_party/blink/renderer/core/inspector/inspector_overlay_agent.h"

namespace blink {

class WebMouseEvent;
class WebPointerEvent;

// -----------------------------------------------------------------------------

class SearchingForNodeTool : public InspectTool {
 public:
  SearchingForNodeTool(InspectorDOMAgent* dom_agent,
                       bool ua_shadow,
                       const std::vector<uint8_t>& highlight_config);

 private:
  bool HandleInputEvent(LocalFrameView* frame_view,
                        const WebInputEvent& input_event,
                        bool* swallow_next_mouse_up) override;
  bool HandleMouseDown(const WebMouseEvent& event,
                       bool* swallow_next_mouse_up) override;
  bool HandleMouseMove(const WebMouseEvent& event) override;
  bool HandleGestureTapEvent(const WebGestureEvent&) override;
  bool HandlePointerEvent(const WebPointerEvent&) override;
  void Draw(float scale) override;
  void NodeHighlightRequested(Node*);
  void Trace(blink::Visitor* visitor) override;

  Member<InspectorDOMAgent> dom_agent_;
  bool ua_shadow_;
  bool is_locked_ancestor_ = false;
  Member<Node> hovered_node_;
  Member<Node> event_target_node_;
  std::unique_ptr<InspectorHighlightConfig> highlight_config_;
  InspectorHighlightContrastInfo contrast_info_;
  bool omit_tooltip_ = false;
  DISALLOW_COPY_AND_ASSIGN(SearchingForNodeTool);
};

// -----------------------------------------------------------------------------

class QuadHighlightTool : public InspectTool {
 public:
  QuadHighlightTool(std::unique_ptr<FloatQuad> quad,
                    Color color,
                    Color outline_color);

 private:
  bool ForwardEventsToOverlay() override;
  bool HideOnHideHighlight() override;
  void Draw(float scale) override;
  std::unique_ptr<FloatQuad> quad_;
  Color color_;
  Color outline_color_;
  DISALLOW_COPY_AND_ASSIGN(QuadHighlightTool);
};

// -----------------------------------------------------------------------------

class NodeHighlightTool : public InspectTool {
 public:
  NodeHighlightTool(Member<Node> node,
                    String selector_list,
                    std::unique_ptr<InspectorHighlightConfig> highlight_config);

 private:
  bool ForwardEventsToOverlay() override;
  bool HideOnHideHighlight() override;
  void Draw(float scale) override;
  void DrawNode();
  void DrawMatchingSelector();
  void Trace(blink::Visitor* visitor) override;

  bool is_locked_ancestor_ = false;
  Member<Node> node_;
  String selector_list_;
  std::unique_ptr<InspectorHighlightConfig> highlight_config_;
  InspectorHighlightContrastInfo contrast_info_;
  DISALLOW_COPY_AND_ASSIGN(NodeHighlightTool);
};

// -----------------------------------------------------------------------------

class NearbyDistanceTool : public InspectTool {
 public:
  NearbyDistanceTool() = default;

 private:
  int GetDataResourceId() override;
  bool HandleMouseDown(const WebMouseEvent& event,
                       bool* swallow_next_mouse_up) override;
  bool HandleMouseMove(const WebMouseEvent& event) override;
  bool HandleMouseUp(const WebMouseEvent& event) override;
  void Draw(float scale) override;
  void Trace(blink::Visitor* visitor) override;

  Member<Node> hovered_node_;
  DISALLOW_COPY_AND_ASSIGN(NearbyDistanceTool);
};

// -----------------------------------------------------------------------------

class ShowViewSizeTool : public InspectTool {
 public:
  ShowViewSizeTool() = default;

 private:
  bool ForwardEventsToOverlay() override;
  int GetDataResourceId() override;
  void Draw(float scale) override;
  DISALLOW_COPY_AND_ASSIGN(ShowViewSizeTool);
};

// -----------------------------------------------------------------------------

class ScreenshotTool : public InspectTool {
 public:
  ScreenshotTool() = default;

 private:
  int GetDataResourceId() override;
  void DoInit() override;
  void Dispatch(const String& message) override;

  DISALLOW_COPY_AND_ASSIGN(ScreenshotTool);
};

// -----------------------------------------------------------------------------

class PausedInDebuggerTool : public InspectTool {
 public:
  PausedInDebuggerTool(v8_inspector::V8InspectorSession* v8_session,
                       const String& message)
      : v8_session_(v8_session), message_(message) {}

 private:
  int GetDataResourceId() override;
  void Draw(float scale) override;
  void Dispatch(const String& message) override;
  v8_inspector::V8InspectorSession* v8_session_;
  String message_;
  DISALLOW_COPY_AND_ASSIGN(PausedInDebuggerTool);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECT_TOOLS_H_
