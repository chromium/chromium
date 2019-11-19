// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/input/event_handling_util.h"

#include "third_party/blink/public/platform/web_mouse_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {
namespace event_handling_util {

HitTestResult HitTestResultInFrame(
    LocalFrame* frame,
    const HitTestLocation& location,
    HitTestRequest::HitTestRequestType hit_type) {
  DCHECK(!location.IsRectBasedTest());
  HitTestResult result(HitTestRequest(hit_type), location);

  if (!frame || !frame->ContentLayoutObject())
    return result;
  if (LocalFrameView* frame_view = frame->View()) {
    PhysicalRect rect(PhysicalOffset(), PhysicalSize(frame_view->Size()));
    if (!location.Intersects(rect))
      return result;
  }
  frame->ContentLayoutObject()->HitTest(location, result);
  return result;
}

WebInputEventResult MergeEventResult(WebInputEventResult result_a,
                                     WebInputEventResult result_b) {
  // The ordering of the enumeration is specific. There are times that
  // multiple events fire and we need to combine them into a single
  // result code. The enumeration is based on the level of consumption that
  // is most significant. The enumeration is ordered with smaller specified
  // numbers first. Examples of merged results are:
  // (HandledApplication, HandledSystem) -> HandledSystem
  // (NotHandled, HandledApplication) -> HandledApplication
  static_assert(static_cast<int>(WebInputEventResult::kNotHandled) == 0,
                "WebInputEventResult not ordered");
  static_assert(static_cast<int>(WebInputEventResult::kHandledSuppressed) <
                    static_cast<int>(WebInputEventResult::kHandledApplication),
                "WebInputEventResult not ordered");
  static_assert(static_cast<int>(WebInputEventResult::kHandledApplication) <
                    static_cast<int>(WebInputEventResult::kHandledSystem),
                "WebInputEventResult not ordered");
  return static_cast<WebInputEventResult>(
      max(static_cast<int>(result_a), static_cast<int>(result_b)));
}

WebInputEventResult ToWebInputEventResult(DispatchEventResult result) {
  switch (result) {
    case DispatchEventResult::kNotCanceled:
      return WebInputEventResult::kNotHandled;
    case DispatchEventResult::kCanceledByEventHandler:
      return WebInputEventResult::kHandledApplication;
    case DispatchEventResult::kCanceledByDefaultEventHandler:
      return WebInputEventResult::kHandledSystem;
    case DispatchEventResult::kCanceledBeforeDispatch:
      return WebInputEventResult::kHandledSuppressed;
    default:
      NOTREACHED();
      return WebInputEventResult::kHandledSystem;
  }
}

PaintLayer* LayerForNode(Node* node) {
  if (!node)
    return nullptr;

  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object)
    return nullptr;

  PaintLayer* layer = layout_object->EnclosingLayer();
  if (!layer)
    return nullptr;

  return layer;
}

bool IsInDocument(EventTarget* n) {
  return n && n->ToNode() && n->ToNode()->isConnected();
}

ScrollableArea* AssociatedScrollableArea(const PaintLayer* layer) {
  if (PaintLayerScrollableArea* scrollable_area = layer->GetScrollableArea()) {
    if (scrollable_area->ScrollsOverflow())
      return scrollable_area;
  }

  return nullptr;
}

ContainerNode* ParentForClickEventInteractiveElementSensitive(
    const Node& node) {
  // IE doesn't dispatch click events for mousedown/mouseup events across form
  // controls.
  if (node.IsHTMLElement() && ToHTMLElement(node).IsInteractiveContent())
    return nullptr;

  return FlatTreeTraversal::Parent(node);
}

ContainerNode* ParentForClickEvent(const Node& node) {
  return FlatTreeTraversal::Parent(node);
}

PhysicalOffset ContentPointFromRootFrame(
    LocalFrame* frame,
    const FloatPoint& point_in_root_frame) {
  LocalFrameView* view = frame->View();
  // FIXME: Is it really OK to use the wrong coordinates here when view is 0?
  // Historically the code would just crash; this is clearly no worse than that.
  return PhysicalOffset::FromFloatPointRound(
      view ? view->ConvertFromRootFrame(point_in_root_frame)
           : point_in_root_frame);
}

MouseEventWithHitTestResults PerformMouseEventHitTest(
    LocalFrame* frame,
    const HitTestRequest& request,
    const WebMouseEvent& mev) {
  DCHECK(frame);
  DCHECK(frame->GetDocument());

  return frame->GetDocument()->PerformMouseEventHitTest(
      request, ContentPointFromRootFrame(frame, mev.PositionInRootFrame()),
      mev);
}

bool ShouldDiscardEventTargetingFrame(const WebInputEvent& event,
                                      const LocalFrame& frame) {
  if (!RuntimeEnabledFeatures::DiscardInputToMovingIframesEnabled())
    return false;

  // There are two different mechanisms for tracking whether an iframe has moved
  // recently, for OOPIF and in-process iframes. For OOPIF's, frame movement is
  // tracked in the browser process using hit test data, and it's propagated
  // in event.GetModifiers(). For in-process iframes, frame movement is tracked
  // during lifecycle updates, in FrameView::UpdateViewportIntersection, and
  // propagated via FrameView::RectInParentIsStable.
  bool should_discard = false;
  if (frame.NeedsOcclusionTracking() && frame.IsCrossOriginSubframe()) {
    should_discard =
        (event.GetModifiers() & WebInputEvent::kTargetFrameMovedRecently) ||
        !frame.View()->RectInParentIsStable(event.TimeStamp());
  }
  if (should_discard) {
    UseCounter::Count(frame.GetDocument(),
                      WebFeature::kDiscardInputEventToMovingIframe);
  }
  return should_discard;
}

LocalFrame* SubframeForTargetNode(Node* node, bool* is_remote_frame) {
  if (!node)
    return nullptr;

  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object || !layout_object->IsLayoutEmbeddedContent())
    return nullptr;

  FrameView* frame_view =
      ToLayoutEmbeddedContent(layout_object)->ChildFrameView();
  if (!frame_view)
    return nullptr;
  auto* local_frame_view = DynamicTo<LocalFrameView>(frame_view);
  if (!local_frame_view) {
    if (is_remote_frame)
      *is_remote_frame = true;
    return nullptr;
  }

  return &local_frame_view->GetFrame();
}

LocalFrame* GetTargetSubframe(
    const MouseEventWithHitTestResults& hit_test_result,
    Node* capturing_node,
    bool* is_remote_frame) {
  if (!RuntimeEnabledFeatures::UnifiedPointerCaptureInBlinkEnabled() &&
      capturing_node) {
    return event_handling_util::SubframeForTargetNode(capturing_node,
                                                      is_remote_frame);
  }

  if (!hit_test_result.IsOverEmbeddedContentView())
    return nullptr;

  return SubframeForTargetNode(hit_test_result.InnerNode(), is_remote_frame);
}

}  // namespace event_handling_util
}  // namespace blink
