// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspect_tools.h"

#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_pointer_event.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/public/resources/grit/inspector_overlay_resources_map.h"
#include "third_party/blink/renderer/core/css/css_color_value.h"
#include "third_party/blink/renderer/core/css/css_computed_style_declaration.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/root_frame_viewport.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/inspector/inspector_css_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_agent.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/cursors.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/inspector_protocol/crdtp/json.h"

namespace blink {

namespace {

InspectorHighlightContrastInfo FetchContrast(Node* node) {
  InspectorHighlightContrastInfo result;
  auto* element = DynamicTo<Element>(node);
  if (!element)
    return result;

  Vector<Color> bgcolors;
  String font_size;
  String font_weight;
  InspectorCSSAgent::GetBackgroundColors(element, &bgcolors, &font_size,
                                         &font_weight);
  if (bgcolors.size() == 1) {
    result.font_size = font_size;
    result.font_weight = font_weight;
    result.background_color = bgcolors[0];
  }
  return result;
}

Node* HoveredNodeForPoint(LocalFrame* frame,
                          const IntPoint& point_in_root_frame,
                          bool ignore_pointer_events_none) {
  HitTestRequest::HitTestRequestType hit_type =
      HitTestRequest::kMove | HitTestRequest::kReadOnly |
      HitTestRequest::kAllowChildFrameContent;
  if (ignore_pointer_events_none)
    hit_type |= HitTestRequest::kIgnorePointerEventsNone;
  HitTestRequest request(hit_type);
  HitTestLocation location(
      frame->View()->ConvertFromRootFrame(point_in_root_frame));
  HitTestResult result(request, location);
  frame->ContentLayoutObject()->HitTest(location, result);
  Node* node = result.InnerPossiblyPseudoNode();
  while (node && node->getNodeType() == Node::kTextNode)
    node = node->parentNode();
  return node;
}

Node* HoveredNodeForEvent(LocalFrame* frame,
                          const WebGestureEvent& event,
                          bool ignore_pointer_events_none) {
  return HoveredNodeForPoint(
      frame, RoundedIntPoint(FloatPoint(event.PositionInRootFrame())),
      ignore_pointer_events_none);
}

Node* HoveredNodeForEvent(LocalFrame* frame,
                          const WebMouseEvent& event,
                          bool ignore_pointer_events_none) {
  return HoveredNodeForPoint(
      frame, RoundedIntPoint(FloatPoint(event.PositionInRootFrame())),
      ignore_pointer_events_none);
}

Node* HoveredNodeForEvent(LocalFrame* frame,
                          const WebPointerEvent& event,
                          bool ignore_pointer_events_none) {
  WebPointerEvent transformed_point = event.WebPointerEventInRootFrame();
  return HoveredNodeForPoint(
      frame, RoundedIntPoint(FloatPoint(transformed_point.PositionInWidget())),
      ignore_pointer_events_none);
}

}  // namespace

// SearchingForNodeTool --------------------------------------------------------

SearchingForNodeTool::SearchingForNodeTool(InspectorOverlayAgent* overlay,
                                           OverlayFrontend* frontend,
                                           InspectorDOMAgent* dom_agent,
                                           bool ua_shadow,
                                           const std::vector<uint8_t>& config)
    : InspectTool(overlay, frontend),
      dom_agent_(dom_agent),
      ua_shadow_(ua_shadow) {
  auto parsed_config = protocol::Overlay::HighlightConfig::FromBinary(
      config.data(), config.size());
  if (parsed_config) {
    highlight_config_ =
        InspectorOverlayAgent::ToHighlightConfig(parsed_config.get());
  }
}

String SearchingForNodeTool::GetOverlayName() {
  return OverlayNames::OVERLAY_HIGHLIGHT;
}

void SearchingForNodeTool::Trace(Visitor* visitor) const {
  InspectTool::Trace(visitor);
  visitor->Trace(dom_agent_);
  visitor->Trace(hovered_node_);
  visitor->Trace(event_target_node_);
}

void SearchingForNodeTool::Draw(float scale) {
  if (!hovered_node_)
    return;

  Node* node = hovered_node_.Get();

  bool append_element_info = (node->IsElementNode() || node->IsTextNode()) &&
                             !omit_tooltip_ && highlight_config_->show_info &&
                             node->GetLayoutObject() &&
                             node->GetDocument().GetFrame();
  overlay_->EnsureAXContext(node);
  InspectorHighlight highlight(node, *highlight_config_, contrast_info_,
                               append_element_info, false, is_locked_ancestor_);
  if (event_target_node_) {
    highlight.AppendEventTargetQuads(event_target_node_.Get(),
                                     *highlight_config_);
  }
  overlay_->EvaluateInOverlay("drawHighlight", highlight.AsProtocolValue());
}

bool SearchingForNodeTool::SupportsPersistentOverlays() {
  return true;
}

bool SearchingForNodeTool::HandleInputEvent(LocalFrameView* frame_view,
                                            const WebInputEvent& input_event,
                                            bool* swallow_next_mouse_up) {
  if (input_event.GetType() == WebInputEvent::Type::kGestureScrollBegin ||
      input_event.GetType() == WebInputEvent::Type::kGestureScrollUpdate) {
    hovered_node_.Clear();
    event_target_node_.Clear();
    overlay_->ScheduleUpdate();
    return false;
  }
  return InspectTool::HandleInputEvent(frame_view, input_event,
                                       swallow_next_mouse_up);
}

bool SearchingForNodeTool::HandleMouseMove(const WebMouseEvent& event) {
  LocalFrame* frame = overlay_->GetFrame();
  if (!frame || !frame->View() || !frame->ContentLayoutObject())
    return false;
  Node* node = HoveredNodeForEvent(
      frame, event, event.GetModifiers() & WebInputEvent::kShiftKey);

  // Do not highlight within user agent shadow root unless requested.
  if (!ua_shadow_) {
    ShadowRoot* shadow_root = InspectorDOMAgent::UserAgentShadowRoot(node);
    if (shadow_root)
      node = &shadow_root->host();
  }

  // Shadow roots don't have boxes - use host element instead.
  if (node && node->IsShadowRoot())
    node = node->ParentOrShadowHostNode();

  if (!node)
    return true;

  // If |node| is in a display locked subtree, highlight the highest locked
  // element instead.
  if (Node* locked_ancestor =
          DisplayLockUtilities::HighestLockedExclusiveAncestor(*node)) {
    node = locked_ancestor;
    is_locked_ancestor_ = true;
  } else {
    is_locked_ancestor_ = false;
  }

  if (auto* frame_owner = DynamicTo<HTMLFrameOwnerElement>(node)) {
    if (!IsA<LocalFrame>(frame_owner->ContentFrame())) {
      // Do not consume event so that remote frame can handle it.
      overlay_->hideHighlight();
      hovered_node_.Clear();
      return false;
    }
  }

  // Store values for the highlight.
  hovered_node_ = node;
  event_target_node_ = (event.GetModifiers() & WebInputEvent::kShiftKey)
                           ? HoveredNodeForEvent(frame, event, false)
                           : nullptr;
  if (event_target_node_ == hovered_node_)
    event_target_node_ = nullptr;
  omit_tooltip_ = event.GetModifiers() &
                  (WebInputEvent::kControlKey | WebInputEvent::kMetaKey);

  contrast_info_ = FetchContrast(node);
  NodeHighlightRequested(node);
  return true;
}

bool SearchingForNodeTool::HandleMouseDown(const WebMouseEvent& event,
                                           bool* swallow_next_mouse_up) {
  if (hovered_node_) {
    *swallow_next_mouse_up = true;
    overlay_->Inspect(hovered_node_.Get());
    hovered_node_.Clear();
    return true;
  }
  return false;
}

bool SearchingForNodeTool::HandleGestureTapEvent(const WebGestureEvent& event) {
  Node* node = HoveredNodeForEvent(overlay_->GetFrame(), event, false);
  if (node) {
    overlay_->Inspect(node);
    return true;
  }
  return false;
}

bool SearchingForNodeTool::HandlePointerEvent(const WebPointerEvent& event) {
  // Trigger Inspect only when a pointer device is pressed down.
  if (event.GetType() != WebInputEvent::Type::kPointerDown)
    return false;
  Node* node = HoveredNodeForEvent(overlay_->GetFrame(), event, false);
  if (node) {
    overlay_->Inspect(node);
    return true;
  }
  return false;
}

void SearchingForNodeTool::NodeHighlightRequested(Node* node) {
  while (node && !node->IsElementNode() && !node->IsDocumentNode() &&
         !node->IsDocumentFragment())
    node = node->ParentOrShadowHostNode();

  if (!node)
    return;

  int node_id = dom_agent_->PushNodePathToFrontend(node);
  if (node_id)
    frontend_->nodeHighlightRequested(node_id);
}

// QuadHighlightTool -----------------------------------------------------------

QuadHighlightTool::QuadHighlightTool(InspectorOverlayAgent* overlay,
                                     OverlayFrontend* frontend,
                                     std::unique_ptr<FloatQuad> quad,
                                     Color color,
                                     Color outline_color)
    : InspectTool(overlay, frontend),
      quad_(std::move(quad)),
      color_(color),
      outline_color_(outline_color) {}

String QuadHighlightTool::GetOverlayName() {
  return OverlayNames::OVERLAY_HIGHLIGHT;
}

bool QuadHighlightTool::ForwardEventsToOverlay() {
  return false;
}

bool QuadHighlightTool::HideOnHideHighlight() {
  return true;
}

void QuadHighlightTool::Draw(float scale) {
  InspectorHighlight highlight(scale);
  highlight.AppendQuad(*quad_, color_, outline_color_);
  overlay_->EvaluateInOverlay("drawHighlight", highlight.AsProtocolValue());
}

// NodeHighlightTool -----------------------------------------------------------

NodeHighlightTool::NodeHighlightTool(
    InspectorOverlayAgent* overlay,
    OverlayFrontend* frontend,
    Member<Node> node,
    String selector_list,
    std::unique_ptr<InspectorHighlightConfig> highlight_config)
    : InspectTool(overlay, frontend),
      selector_list_(selector_list),
      highlight_config_(std::move(highlight_config)) {
  if (Node* locked_ancestor =
          DisplayLockUtilities::HighestLockedExclusiveAncestor(*node)) {
    is_locked_ancestor_ = true;
    node_ = locked_ancestor;
  } else {
    node_ = node;
  }
  contrast_info_ = FetchContrast(node_);
}

String NodeHighlightTool::GetOverlayName() {
  return OverlayNames::OVERLAY_HIGHLIGHT;
}

bool NodeHighlightTool::ForwardEventsToOverlay() {
  return false;
}

bool NodeHighlightTool::SupportsPersistentOverlays() {
  return true;
}

bool NodeHighlightTool::HideOnHideHighlight() {
  return true;
}

bool NodeHighlightTool::HideOnMouseMove() {
  return true;
}

void NodeHighlightTool::Draw(float scale) {
  DrawNode();
  DrawMatchingSelector();
}

void NodeHighlightTool::DrawNode() {
  bool append_element_info = (node_->IsElementNode() || node_->IsTextNode()) &&
                             highlight_config_->show_info &&
                             node_->GetLayoutObject() &&
                             node_->GetDocument().GetFrame();
  overlay_->EvaluateInOverlay(
      "drawHighlight",
      GetNodeInspectorHighlightAsJson(append_element_info,
                                      false /* append_distance_info */));
}

void NodeHighlightTool::DrawMatchingSelector() {
  if (selector_list_.IsEmpty() || !node_)
    return;
  DummyExceptionStateForTesting exception_state;
  ContainerNode* query_base = node_->ContainingShadowRoot();
  if (!query_base)
    query_base = node_->ownerDocument();

  overlay_->EnsureAXContext(query_base);

  StaticElementList* elements = query_base->QuerySelectorAll(
      AtomicString(selector_list_), exception_state);
  if (exception_state.HadException())
    return;

  for (unsigned i = 0; i < elements->length(); ++i) {
    Element* element = elements->item(i);
    // Skip elements in locked subtrees.
    if (DisplayLockUtilities::NearestLockedExclusiveAncestor(*element))
      continue;
    InspectorHighlight highlight(element, *highlight_config_, contrast_info_,
                                 false /* append_element_info */,
                                 false /* append_distance_info */,
                                 false /* is_locked_ancestor */);
    overlay_->EvaluateInOverlay("drawHighlight", highlight.AsProtocolValue());
  }
}

void NodeHighlightTool::Trace(Visitor* visitor) const {
  InspectTool::Trace(visitor);
  visitor->Trace(node_);
}

std::unique_ptr<protocol::DictionaryValue>
NodeHighlightTool::GetNodeInspectorHighlightAsJson(
    bool append_element_info,
    bool append_distance_info) const {
  overlay_->EnsureAXContext(node_.Get());
  InspectorHighlight highlight(node_.Get(), *highlight_config_, contrast_info_,
                               append_element_info, append_distance_info,
                               is_locked_ancestor_);
  return highlight.AsProtocolValue();
}

// GridHighlightTool -----------------------------------------------------------
String PersistentTool::GetOverlayName() {
  return OverlayNames::OVERLAY_PERSISTENT;
}

bool PersistentTool::IsEmpty() {
  return !grid_node_highlights_.size() && !flex_container_configs_.size();
}

void PersistentTool::SetGridConfigs(
    Vector<std::pair<Member<Node>,
                     std::unique_ptr<InspectorGridHighlightConfig>>> configs) {
  grid_node_highlights_ = std::move(configs);
}

void PersistentTool::SetFlexContainerConfigs(
    Vector<std::pair<Member<Node>,
                     std::unique_ptr<InspectorFlexContainerHighlightConfig>>>
        configs) {
  flex_container_configs_ = std::move(configs);
}

bool PersistentTool::ForwardEventsToOverlay() {
  return false;
}

bool PersistentTool::HideOnHideHighlight() {
  return false;
}

bool PersistentTool::HideOnMouseMove() {
  return false;
}

void PersistentTool::Draw(float scale) {
  for (auto& entry : grid_node_highlights_) {
    std::unique_ptr<protocol::Value> highlight =
        InspectorGridHighlight(entry.first.Get(), *(entry.second));
    if (!highlight)
      continue;
    overlay_->EvaluateInOverlay("drawGridHighlight", std::move(highlight));
  }
  for (auto& entry : flex_container_configs_) {
    std::unique_ptr<protocol::Value> highlight =
        InspectorFlexContainerHighlight(entry.first.Get(), *(entry.second));
    if (!highlight)
      continue;
    overlay_->EvaluateInOverlay("drawFlexContainerHighlight",
                                std::move(highlight));
  }
}

std::unique_ptr<protocol::DictionaryValue>
PersistentTool::GetGridInspectorHighlightsAsJson() const {
  std::unique_ptr<protocol::ListValue> highlights =
      protocol::ListValue::create();
  for (auto& entry : grid_node_highlights_) {
    std::unique_ptr<protocol::Value> highlight =
        InspectorGridHighlight(entry.first.Get(), *(entry.second));
    if (!highlight)
      continue;
    highlights->pushValue(std::move(highlight));
  }
  std::unique_ptr<protocol::DictionaryValue> result =
      protocol::DictionaryValue::create();
  if (highlights->size() > 0) {
    result->setValue("gridHighlights", std::move(highlights));
  }
  return result;
}

// SourceOrderTool -----------------------------------------------------------

SourceOrderTool::SourceOrderTool(
    InspectorOverlayAgent* overlay,
    OverlayFrontend* frontend,
    Node* node,
    std::unique_ptr<InspectorSourceOrderConfig> source_order_config)
    : InspectTool(overlay, frontend),
      source_order_config_(std::move(source_order_config)) {
  if (Node* locked_ancestor =
          DisplayLockUtilities::HighestLockedExclusiveAncestor(*node)) {
    node_ = locked_ancestor;
  } else {
    node_ = node;
  }
}

String SourceOrderTool::GetOverlayName() {
  return OverlayNames::OVERLAY_SOURCE_ORDER;
}

void SourceOrderTool::Draw(float scale) {
  DrawParentNode();

  // Draw child outlines and labels.
  int position_number = 1;
  for (Node& child_node : NodeTraversal::ChildrenOf(*node_)) {
    // Don't draw if it's not an element or is not the direct child of the
    // parent node.
    if (!child_node.IsElementNode())
      continue;
    // Don't draw if it's not rendered/would be ignored by a screen reader.
    if (child_node.GetComputedStyle()) {
      bool display_none =
          child_node.GetComputedStyle()->Display() == EDisplay::kNone;
      bool visibility_hidden =
          child_node.GetComputedStyle()->Visibility() == EVisibility::kHidden;
      if (display_none || visibility_hidden)
        continue;
    }
    DrawNode(&child_node, position_number);
    position_number++;
  }
}

void SourceOrderTool::DrawNode(Node* node, int source_order_position) {
  InspectorSourceOrderHighlight highlight(
      node, source_order_config_->child_outline_color, source_order_position);
  overlay_->EvaluateInOverlay("drawSourceOrder", highlight.AsProtocolValue());
}

void SourceOrderTool::DrawParentNode() {
  InspectorSourceOrderHighlight highlight(
      node_.Get(), source_order_config_->parent_outline_color, 0);
  overlay_->EvaluateInOverlay("drawSourceOrder", highlight.AsProtocolValue());
}

bool SourceOrderTool::HideOnHideHighlight() {
  return true;
}

bool SourceOrderTool::HideOnMouseMove() {
  return false;
}

std::unique_ptr<protocol::DictionaryValue>
SourceOrderTool::GetNodeInspectorSourceOrderHighlightAsJson() const {
  InspectorSourceOrderHighlight highlight(
      node_.Get(), source_order_config_->parent_outline_color, 0);
  return highlight.AsProtocolValue();
}

void SourceOrderTool::Trace(Visitor* visitor) const {
  InspectTool::Trace(visitor);
  visitor->Trace(node_);
}

// NearbyDistanceTool ----------------------------------------------------------

String NearbyDistanceTool::GetOverlayName() {
  return OverlayNames::OVERLAY_DISTANCES;
}

bool NearbyDistanceTool::HandleMouseDown(const WebMouseEvent& event,
                                         bool* swallow_next_mouse_up) {
  return true;
}

bool NearbyDistanceTool::HandleMouseMove(const WebMouseEvent& event) {
  Node* node = HoveredNodeForEvent(overlay_->GetFrame(), event, true);

  // Do not highlight within user agent shadow root
  ShadowRoot* shadow_root = InspectorDOMAgent::UserAgentShadowRoot(node);
  if (shadow_root)
    node = &shadow_root->host();

  // Shadow roots don't have boxes - use host element instead.
  if (node && node->IsShadowRoot())
    node = node->ParentOrShadowHostNode();

  if (!node)
    return true;

  if (auto* frame_owner = DynamicTo<HTMLFrameOwnerElement>(node)) {
    if (!IsA<LocalFrame>(frame_owner->ContentFrame())) {
      // Do not consume event so that remote frame can handle it.
      overlay_->hideHighlight();
      hovered_node_.Clear();
      return false;
    }
  }

  // If |node| is in a display locked subtree, highlight the highest locked
  // element instead.
  if (Node* locked_ancestor =
          DisplayLockUtilities::HighestLockedExclusiveAncestor(*node))
    node = locked_ancestor;

  // Store values for the highlight.
  hovered_node_ = node;
  return true;
}

bool NearbyDistanceTool::HandleMouseUp(const WebMouseEvent& event) {
  return true;
}

void NearbyDistanceTool::Draw(float scale) {
  Node* node = hovered_node_.Get();
  if (!node)
    return;
  overlay_->EnsureAXContext(node);
  InspectorHighlight highlight(
      node, InspectorHighlight::DefaultConfig(),
      InspectorHighlightContrastInfo(), false /* append_element_info */,
      true /* append_distance_info */, false /* is_locked_ancestor */);
  overlay_->EvaluateInOverlay("drawDistances", highlight.AsProtocolValue());
}

void NearbyDistanceTool::Trace(Visitor* visitor) const {
  InspectTool::Trace(visitor);
  visitor->Trace(hovered_node_);
}

// ShowViewSizeTool ------------------------------------------------------------

void ShowViewSizeTool::Draw(float scale) {
  overlay_->EvaluateInOverlay("drawViewSize", "");
}

String ShowViewSizeTool::GetOverlayName() {
  return OverlayNames::OVERLAY_VIEWPORT_SIZE;
}

bool ShowViewSizeTool::ForwardEventsToOverlay() {
  return false;
}

// ScreenshotTool --------------------------------------------------------------

ScreenshotTool::ScreenshotTool(InspectorOverlayAgent* overlay,
                               OverlayFrontend* frontend)
    : InspectTool(overlay, frontend) {
  auto& client = overlay_->GetFrame()->GetPage()->GetChromeClient();
  client.SetCursorOverridden(false);
  client.SetCursor(CrossCursor(), overlay_->GetFrame());
  client.SetCursorOverridden(true);
}

String ScreenshotTool::GetOverlayName() {
  return OverlayNames::OVERLAY_SCREENSHOT;
}

void ScreenshotTool::Dispatch(const String& message) {
  if (message.IsEmpty())
    return;
  std::vector<uint8_t> cbor;
  if (message.Is8Bit()) {
    crdtp::json::ConvertJSONToCBOR(
        crdtp::span<uint8_t>(message.Characters8(), message.length()), &cbor);
  } else {
    crdtp::json::ConvertJSONToCBOR(
        crdtp::span<uint16_t>(
            reinterpret_cast<const uint16_t*>(message.Characters16()),
            message.length()),
        &cbor);
  }
  std::unique_ptr<protocol::DOM::Rect> box =
      protocol::DOM::Rect::FromBinary(cbor.data(), cbor.size());
  if (!box)
    return;
  float scale = 1.0f;
  // Capture values in the CSS pixels.
  IntPoint p1(box->getX(), box->getY());
  IntPoint p2(box->getX() + box->getWidth(), box->getY() + box->getHeight());

  if (LocalFrame* frame = overlay_->GetFrame()) {
    float emulation_scale = overlay_->GetFrame()
                                ->GetPage()
                                ->GetChromeClient()
                                .InputEventsScaleForEmulation();
    // Convert from overlay terms into the absolute.
    p1.Scale(1 / emulation_scale, 1 / emulation_scale);
    p2.Scale(1 / emulation_scale, 1 / emulation_scale);

    // Scroll offset in the viewport is in the device pixels, convert before
    // calling ViewportToRootFrame.
    float dip_to_dp = overlay_->WindowToViewportScale();
    p1.Scale(dip_to_dp, dip_to_dp);
    p2.Scale(dip_to_dp, dip_to_dp);

    const VisualViewport& visual_viewport =
        frame->GetPage()->GetVisualViewport();
    p1 = visual_viewport.ViewportToRootFrame(p1);
    p2 = visual_viewport.ViewportToRootFrame(p2);

    scale = frame->GetPage()->PageScaleFactor();
    if (const RootFrameViewport* root_frame_viewport =
            frame->View()->GetRootFrameViewport()) {
      IntSize scroll_offset = FlooredIntSize(
          root_frame_viewport->LayoutViewport().GetScrollOffset());
      // Accunt for the layout scroll (different from viewport scroll offset).
      p1 += scroll_offset;
      p2 += scroll_offset;
    }
  }

  // Go back to dip for the protocol.
  float dp_to_dip = 1.f / overlay_->WindowToViewportScale();
  p1.Scale(dp_to_dip, dp_to_dip);
  p2.Scale(dp_to_dip, dp_to_dip);

  // Points are in device independent pixels (dip) now.
  IntRect rect =
      UnionRectEvenIfEmpty(IntRect(p1, IntSize()), IntRect(p2, IntSize()));
  frontend_->screenshotRequested(protocol::Page::Viewport::create()
                                     .setX(rect.X())
                                     .setY(rect.Y())
                                     .setWidth(rect.Width())
                                     .setHeight(rect.Height())
                                     .setScale(scale)
                                     .build());
}

// PausedInDebuggerTool --------------------------------------------------------

String PausedInDebuggerTool::GetOverlayName() {
  return OverlayNames::OVERLAY_PAUSED;
}

void PausedInDebuggerTool::Draw(float scale) {
  overlay_->EvaluateInOverlay("drawPausedInDebuggerMessage", message_);
}

void PausedInDebuggerTool::Dispatch(const String& message) {
  if (message == "resume")
    v8_session_->resume();
  else if (message == "stepOver")
    v8_session_->stepOver();
}

}  // namespace blink
