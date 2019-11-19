// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INPUT_EVENT_HANDLING_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INPUT_EVENT_HANDLING_UTIL_H_

#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/page/event_with_hit_test_results.h"

namespace blink {

class ContainerNode;
class EventTarget;
class LocalFrame;
class ScrollableArea;
class PaintLayer;
enum class DispatchEventResult;

namespace event_handling_util {

HitTestResult HitTestResultInFrame(
    LocalFrame*,
    const HitTestLocation&,
    HitTestRequest::HitTestRequestType hit_type = HitTestRequest::kReadOnly |
                                                  HitTestRequest::kActive);

WebInputEventResult MergeEventResult(WebInputEventResult result_a,
                                     WebInputEventResult result_b);
WebInputEventResult ToWebInputEventResult(DispatchEventResult);

PaintLayer* LayerForNode(Node*);
ScrollableArea* AssociatedScrollableArea(const PaintLayer*);

bool IsInDocument(EventTarget*);

ContainerNode* ParentForClickEvent(const Node&);
ContainerNode* ParentForClickEventInteractiveElementSensitive(const Node&);

PhysicalOffset ContentPointFromRootFrame(LocalFrame*,
                                         const FloatPoint& point_in_root_frame);

MouseEventWithHitTestResults PerformMouseEventHitTest(LocalFrame*,
                                                      const HitTestRequest&,
                                                      const WebMouseEvent&);

LocalFrame* GetTargetSubframe(const MouseEventWithHitTestResults&,
                              Node* capturing_node = nullptr,
                              bool* is_remote_frame = nullptr);

LocalFrame* SubframeForTargetNode(Node*, bool* is_remote_frame = nullptr);

// Intervention: if an input event lands on a cross-origin iframe that has
// moved or resized recently (recent==500ms), and which contains an
// IntersectionObserver that is tracking visibility, then the event is quietly
// discarded.
bool ShouldDiscardEventTargetingFrame(const WebInputEvent& event,
                                      const LocalFrame& frame);

class PointerEventTarget {
  DISALLOW_NEW();

 public:
  void Trace(blink::Visitor* visitor) {
    visitor->Trace(target_element);
    visitor->Trace(target_frame);
  }

  Member<Element> target_element;
  Member<LocalFrame> target_frame;
  String region;
};

}  // namespace event_handling_util

}  // namespace blink

WTF_ALLOW_INIT_WITH_MEM_FUNCTIONS(
    blink::event_handling_util::PointerEventTarget)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INPUT_EVENT_HANDLING_UTIL_H_
