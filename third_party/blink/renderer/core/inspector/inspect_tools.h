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
  SearchingForNodeTool(InspectorOverlayAgent* overlay,
                       OverlayFrontend* frontend,
                       InspectorDOMAgent* dom_agent,
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
  void Trace(Visitor* visitor) const override;
  bool SupportsPersistentOverlays() override;

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
  QuadHighlightTool(InspectorOverlayAgent* overlay,
                    OverlayFrontend* frontend,
                    std::unique_ptr<FloatQuad> quad,
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
  NodeHighlightTool(InspectorOverlayAgent* overlay,
                    OverlayFrontend* frontend,
                    Member<Node> node,
                    String selector_list,
                    std::unique_ptr<InspectorHighlightConfig> highlight_config);

  std::unique_ptr<protocol::DictionaryValue> GetNodeInspectorHighlightAsJson(
      bool append_element_info,
      bool append_distance_info) const;

 private:
  bool ForwardEventsToOverlay() override;
  bool SupportsPersistentOverlays() override;
  bool HideOnMouseMove() override;
  bool HideOnHideHighlight() override;
  void Draw(float scale) override;
  void DrawNode();
  void DrawMatchingSelector();
  void Trace(Visitor* visitor) const override;

  bool is_locked_ancestor_ = false;
  Member<Node> node_;
  String selector_list_;
  std::unique_ptr<InspectorHighlightConfig> highlight_config_;
  InspectorHighlightContrastInfo contrast_info_;
  DISALLOW_COPY_AND_ASSIGN(NodeHighlightTool);
};

// -----------------------------------------------------------------------------

class SourceOrderTool : public InspectTool {
 public:
  SourceOrderTool(
      InspectorOverlayAgent* overlay,
      OverlayFrontend* frontend,
      Node* node,
      std::unique_ptr<InspectorSourceOrderConfig> source_order_config);
  std::unique_ptr<protocol::DictionaryValue>
  GetNodeInspectorSourceOrderHighlightAsJson() const;

 private:
  bool HideOnHideHighlight() override;
  bool HideOnMouseMove() override;
  int GetDataResourceId() override;
  void Draw(float scale) override;
  void DrawNode(Node* node, int source_order_position);
  void DrawParentNode();
  void Trace(Visitor* visitor) const override;

  Member<Node> node_;
  std::unique_ptr<InspectorSourceOrderConfig> source_order_config_;
  DISALLOW_COPY_AND_ASSIGN(SourceOrderTool);
};

// -----------------------------------------------------------------------------

class GridHighlightTool : public InspectTool {
  using InspectTool::InspectTool;

 public:
  void Draw(float scale) override;
  void AddGridConfig(
      Node* node,
      std::unique_ptr<InspectorGridHighlightConfig> grid_highlight_config);

  std::unique_ptr<protocol::DictionaryValue> GetGridInspectorHighlightsAsJson()
      const;

 private:
  int GetDataResourceId() override;
  bool ForwardEventsToOverlay() override;
  bool HideOnMouseMove() override;
  bool HideOnHideHighlight() override;

  Vector<std::pair<Member<Node>, std::unique_ptr<InspectorGridHighlightConfig>>>
      grid_node_highlights_;
  DISALLOW_COPY_AND_ASSIGN(GridHighlightTool);
};

// -----------------------------------------------------------------------------

class NearbyDistanceTool : public InspectTool {
  using InspectTool::InspectTool;

 private:
  int GetDataResourceId() override;
  bool HandleMouseDown(const WebMouseEvent& event,
                       bool* swallow_next_mouse_up) override;
  bool HandleMouseMove(const WebMouseEvent& event) override;
  bool HandleMouseUp(const WebMouseEvent& event) override;
  void Draw(float scale) override;
  void Trace(Visitor* visitor) const override;

  Member<Node> hovered_node_;
  DISALLOW_COPY_AND_ASSIGN(NearbyDistanceTool);
};

// -----------------------------------------------------------------------------

class ShowViewSizeTool : public InspectTool {
  using InspectTool::InspectTool;

 private:
  bool ForwardEventsToOverlay() override;
  int GetDataResourceId() override;
  void Draw(float scale) override;
  DISALLOW_COPY_AND_ASSIGN(ShowViewSizeTool);
};

// -----------------------------------------------------------------------------

class ScreenshotTool : public InspectTool {
 public:
  ScreenshotTool(InspectorOverlayAgent* overlay, OverlayFrontend* frontend);

 private:
  int GetDataResourceId() override;
  void Dispatch(const String& message) override;

  DISALLOW_COPY_AND_ASSIGN(ScreenshotTool);
};

// -----------------------------------------------------------------------------

class PausedInDebuggerTool : public InspectTool {
 public:
  PausedInDebuggerTool(InspectorOverlayAgent* overlay,
                       OverlayFrontend* frontend,
                       v8_inspector::V8InspectorSession* v8_session,
                       const String& message)
      : InspectTool(overlay, frontend),
        v8_session_(v8_session),
        message_(message) {}

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
