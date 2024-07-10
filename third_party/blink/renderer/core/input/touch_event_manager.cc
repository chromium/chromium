// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/input/touch_event_manager.h"

#include <memory>

#include "base/ranges/algorithm.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_touch_event.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/events/touch_event.h"
#include "third_party/blink/renderer/core/frame/event_handler_registry.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/input/event_handling_util.h"
#include "third_party/blink/renderer/core/input/touch_action_util.h"
#include "third_party/blink/renderer/core/layout/hit_test_canvas_result.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

namespace {

// Returns true if there are event listeners of |handler_class| on |touch_node|
// or any of its ancestors inside the document (including DOMWindow).
bool HasEventHandlerInAncestorPath(
    Node* touch_node,
    EventHandlerRegistry::EventHandlerClass handler_class) {
  Document& document = touch_node->GetDocument();
  const EventTargetSet* event_target_set =
      document.GetFrame()->GetEventHandlerRegistry().EventHandlerTargets(
          handler_class);

  if (event_target_set->Contains(document.domWindow()))
    return true;

  for (Node& ancestor : NodeTraversal::InclusiveAncestorsOf(*touch_node)) {
    if (event_target_set->Contains(&ancestor))
      return true;
  }

  return false;
}

bool HasTouchHandlers(const EventHandlerRegistry& registry) {
  return registry.HasEventHandlers(
             EventHandlerRegistry::kTouchStartOrMoveEventBlocking) ||
         registry.HasEventHandlers(
             EventHandlerRegistry::kTouchStartOrMoveEventBlockingLowLatency) ||
         registry.HasEventHandlers(
             EventHandlerRegistry::kTouchStartOrMoveEventPassive) ||
         registry.HasEventHandlers(
             EventHandlerRegistry::kTouchEndOrCancelEventBlocking) ||
         registry.HasEventHandlers(
             EventHandlerRegistry::kTouchEndOrCancelEventPassive);
}

const AtomicString& TouchEventNameForPointerEventType(
    WebInputEvent::Type type) {
  switch (type) {
    case WebInputEvent::Type::kPointerUp:
      return event_type_names::kTouchend;
    case WebInputEvent::Type::kPointerCancel:
      return event_type_names::kTouchcancel;
    case WebInputEvent::Type::kPointerDown:
      return event_type_names::kTouchstart;
    case WebInputEvent::Type::kPointerMove:
      return event_type_names::kTouchmove;
    default:
      NOTREACHED_IN_MIGRATION();
      return g_empty_atom;
  }
}

WebTouchPoint::State TouchPointStateFromPointerEventType(
    WebInputEvent::Type type,
    bool stale) {
  if (stale)
    return WebTouchPoint::State::kStateStationary;
  switch (type) {
    case WebInputEvent::Type::kPointerUp:
      return WebTouchPoint::State::kStateReleased;
    case WebInputEvent::Type::kPointerCancel:
      return WebTouchPoint::State::kStateCancelled;
    case WebInputEvent::Type::kPointerDown:
      return WebTouchPoint::State::kStatePressed;
    case WebInputEvent::Type::kPointerMove:
      return WebTouchPoint::State::kStateMoved;
    default:
      NOTREACHED_IN_MIGRATION();
      return WebTouchPoint::State::kStateUndefined;
  }
}

WebTouchPoint CreateWebTouchPointFromWebPointerEvent(
    const WebPointerEvent& web_pointer_event,
    bool stale) {
  WebTouchPoint web_touch_point(web_pointer_event);
  web_touch_point.state =
      TouchPointStateFromPointerEventType(web_pointer_event.GetType(), stale);
  web_touch_point.radius_x = web_pointer_event.width / 2.f;
  web_touch_point.radius_y = web_pointer_event.height / 2.f;
  web_touch_point.rotation_angle = web_pointer_event.rotation_angle;
  return web_touch_point;
}

void SetWebTouchEventAttributesFromWebPointerEvent(
    WebTouchEvent* web_touch_event,
    const WebPointerEvent& web_pointer_event) {
  web_touch_event->dispatch_type = web_pointer_event.dispatch_type;
  web_touch_event->touch_start_or_first_touch_move =
      web_pointer_event.touch_start_or_first_touch_move;
  web_touch_event->moved_beyond_slop_region =
      web_pointer_event.moved_beyond_slop_region;
  web_touch_event->SetFrameScale(web_pointer_event.FrameScale());
  web_touch_event->SetFrameTranslate(web_pointer_event.FrameTranslate());
  web_touch_event->SetTimeStamp(web_pointer_event.TimeStamp());
  web_touch_event->SetModifiers(web_pointer_event.GetModifiers());
}

// Defining this class type local to
// DispatchTouchEventFromAccumulatdTouchPoints() and annotating
// it with STACK_ALLOCATED(), runs into MSVC(VS 2013)'s C4822 warning
// that the local class doesn't provide a local definition for 'operator new'.
// Which it intentionally doesn't and shouldn't.
//
// Work around such toolchain bugginess by lifting out the type, thereby
// taking it out of C4822's reach.
class ChangedTouches final {
  STACK_ALLOCATED();

 public:
  // The touches corresponding to the particular change state this struct
  // instance represents.
  TouchList* touches_ = nullptr;

  using EventTargetSet = HeapHashSet<Member<EventTarget>>;
  // Set of targets involved in m_touches.
  EventTargetSet targets_;
};

}  // namespace

TouchEventManager::TouchEventManager(LocalFrame& frame) : frame_(frame) {
  Clear();
}

void TouchEventManager::Clear() {
  touch_sequence_document_.Clear();
  touch_attribute_map_.clear();
  last_coalesced_touch_event_ = WebTouchEvent();
  suppressing_touchmoves_within_slop_ = false;
  current_touch_action_ = TouchAction::kAuto;
}

void TouchEventManager::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  visitor->Trace(touch_sequence_document_);
  visitor->Trace(touch_attribute_map_);
}

Touch* TouchEventManager::CreateDomTouch(
    const TouchEventManager::TouchPointAttributes* point_attr,
    bool* known_target) {
  Node* touch_node = point_attr->target_;
  *known_target = false;

  LocalFrame* target_frame = nullptr;
  if (touch_node) {
    Document& doc = touch_node->GetDocument();
    // If the target node has moved to a new document while it was being
    // touched, we can't send events to the new document because that could
    // leak nodes from one document to another. See http://crbug.com/394339.
    if (&doc == touch_sequence_document_.Get()) {
      target_frame = doc.GetFrame();
      *known_target = true;
    }
  }
  if (!(*known_target)) {
    // If we don't have a target registered for the point it means we've
    // missed our opportunity to do a hit test for it (due to some
    // optimization that prevented blink from ever seeing the
    // touchstart), or that the touch started outside the active touch
    // sequence document. We should still include the touch in the
    // Touches list reported to the application (eg. so it can
    // differentiate between a one and two finger gesture), but we won't
    // actually dispatch any events for it. Set the target to the
    // Document so that there's some valid node here. Perhaps this
    // should really be LocalDOMWindow, but in all other cases the target of
    // a Touch is a Node so using the window could be a breaking change.
    // Since we know there was no handler invoked, the specific target
    // should be completely irrelevant to the application.
    touch_node = touch_sequence_document_;
    target_frame = touch_sequence_document_->GetFrame();
  }
  DCHECK(target_frame);

  WebPointerEvent transformed_event =
      point_attr->event_.WebPointerEventInRootFrame();
  float scale_factor = 1.0f / target_frame->LayoutZoomFactor();

  gfx::PointF document_point =
      gfx::ScalePoint(target_frame->View()->RootFrameToDocument(
                          transformed_event.PositionInWidget()),
                      scale_factor);
  gfx::SizeF adjusted_radius = gfx::ScaleSize(
      gfx::SizeF(transformed_event.width / 2.f, transformed_event.height / 2.f),
      scale_factor);

  return MakeGarbageCollected<Touch>(
      target_frame, touch_node, point_attr->event_.id,
      transformed_event.PositionInScreen(), document_point, adjusted_radius,
      transformed_event.rotation_angle, transformed_event.force);
}

WebCoalescedInputEvent TouchEventManager::GenerateWebCoalescedInputEvent() {
  DCHECK(!touch_attribute_map_.empty());

  auto event = std::make_unique<WebTouchEvent>();

  const auto& first_touch_pointer_event =
      touch_attribute_map_.begin()->value->event_;

  SetWebTouchEventAttributesFromWebPointerEvent(event.get(),
                                                first_touch_pointer_event);
  SetWebTouchEventAttributesFromWebPointerEvent(&last_coalesced_touch_event_,
                                                first_touch_pointer_event);
  WebInputEvent::Type touch_event_type = WebInputEvent::Type::kTouchMove;
  Vector<WebPointerEvent> all_coalesced_events;
  Vector<int> available_ids;
  WTF::CopyKeysToVector(touch_attribute_map_, available_ids);
  std::sort(available_ids.begin(), available_ids.end());
  for (const int& touch_point_id : available_ids) {
    auto* const touch_point_attribute = touch_attribute_map_.at(touch_point_id);
    const WebPointerEvent& touch_pointer_event = touch_point_attribute->event_;
    event->touches[event->touches_length++] =
        CreateWebTouchPointFromWebPointerEvent(touch_pointer_event,
                                               touch_point_attribute->stale_);
    if (!touch_point_attribute->stale_) {
      event->SetTimeStamp(std::max(event->TimeStamp(),
                                   touch_point_attribute->event_.TimeStamp()));
    }

    // Only change the touch event type from move. So if we have two pointers
    // in up and down state we just set the touch event type to the first one
    // we see.
    // TODO(crbug.com/732842): Note that event sender API allows sending any
    // mix of input and as long as we don't crash or anything we should be good
    // for now.
    if (touch_event_type == WebInputEvent::Type::kTouchMove) {
      if (touch_pointer_event.GetType() == WebInputEvent::Type::kPointerDown)
        touch_event_type = WebInputEvent::Type::kTouchStart;
      else if (touch_pointer_event.GetType() ==
               WebInputEvent::Type::kPointerCancel)
        touch_event_type = WebInputEvent::Type::kTouchCancel;
      else if (touch_pointer_event.GetType() == WebInputEvent::Type::kPointerUp)
        touch_event_type = WebInputEvent::Type::kTouchEnd;
    }

    for (const WebPointerEvent& coalesced_event :
         touch_point_attribute->coalesced_events_) {
      all_coalesced_events.push_back(coalesced_event);
    }
  }
  event->SetType(touch_event_type);
  last_coalesced_touch_event_.SetType(touch_event_type);

  // Create all coalesced touch events based on pointerevents
  struct {
    bool operator()(const WebPointerEvent& a, const WebPointerEvent& b) {
      return a.TimeStamp() < b.TimeStamp();
    }
  } timestamp_based_event_comparison;
  std::sort(all_coalesced_events.begin(), all_coalesced_events.end(),
            timestamp_based_event_comparison);
  WebCoalescedInputEvent result(std::move(event), {}, {}, ui::LatencyInfo());
  for (const auto& web_pointer_event : all_coalesced_events) {
    if (web_pointer_event.GetType() == WebInputEvent::Type::kPointerDown) {
      // TODO(crbug.com/732842): Technically we should never receive the
      // pointerdown twice for the same touch point. But event sender API allows
      // that. So we should handle it gracefully.
      WebTouchPoint web_touch_point(web_pointer_event);
      bool found_existing_id = false;
      for (unsigned i = 0; i < last_coalesced_touch_event_.touches_length;
           ++i) {
        if (last_coalesced_touch_event_.touches[i].id == web_pointer_event.id) {
          last_coalesced_touch_event_.touches[i] =
              CreateWebTouchPointFromWebPointerEvent(web_pointer_event, false);
          last_coalesced_touch_event_.SetTimeStamp(
              web_pointer_event.TimeStamp());
          found_existing_id = true;
          break;
        }
      }
      // If the pointerdown point didn't exist add a new point to the array.
      if (!found_existing_id) {
        last_coalesced_touch_event_
            .touches[last_coalesced_touch_event_.touches_length++] =
            CreateWebTouchPointFromWebPointerEvent(web_pointer_event, false);
      }
      struct {
        bool operator()(const WebTouchPoint& a, const WebTouchPoint& b) {
          return a.id < b.id;
        }
      } id_based_event_comparison;
      base::ranges::sort(base::span(last_coalesced_touch_event_.touches)
                             .first(last_coalesced_touch_event_.touches_length),
                         id_based_event_comparison);
      result.AddCoalescedEvent(last_coalesced_touch_event_);
    } else {
      for (unsigned i = 0; i < last_coalesced_touch_event_.touches_length;
           ++i) {
        if (last_coalesced_touch_event_.touches[i].id == web_pointer_event.id) {
          last_coalesced_touch_event_.touches[i] =
              CreateWebTouchPointFromWebPointerEvent(web_pointer_event, false);
          last_coalesced_touch_event_.SetTimeStamp(
              web_pointer_event.TimeStamp());
          result.AddCoalescedEvent(last_coalesced_touch_event_);

          // Remove up and canceled points.
          unsigned result_size = 0;
          for (unsigned j = 0; j < last_coalesced_touch_event_.touches_length;
               j++) {
            if (last_coalesced_touch_event_.touches[j].state !=
                    WebTouchPoint::State::kStateCancelled &&
                last_coalesced_touch_event_.touches[j].state !=
                    WebTouchPoint::State::kStateReleased) {
              last_coalesced_touch_event_.touches[result_size++] =
                  last_coalesced_touch_event_.touches[j];
            }
          }
          last_coalesced_touch_event_.touches_length = result_size;
          break;
        }
      }
    }
  }

  return result;
}

WebInputEventResult
TouchEventManager::DispatchTouchEventFromAccumulatdTouchPoints() {
  // Build up the lists to use for the |touches|, |targetTouches| and
  // |changedTouches| attributes in the JS event. See
  // http://www.w3.org/TR/touch-events/#touchevent-interface for how these
  // lists fit together.

  bool new_touch_point_since_last_dispatch = false;
  bool any_touch_canceled_or_ended = false;
  bool all_touch_points_pressed = true;

  for (const auto& attr : touch_attribute_map_.Values()) {
    if (!attr->stale_)
      new_touch_point_since_last_dispatch = true;
    if (attr->event_.GetType() == WebInputEvent::Type::kPointerUp ||
        attr->event_.GetType() == WebInputEvent::Type::kPointerCancel)
      any_touch_canceled_or_ended = true;
    if (attr->event_.GetType() != WebInputEvent::Type::kPointerDown)
      all_touch_points_pressed = false;
  }

  if (!new_touch_point_since_last_dispatch)
    return WebInputEventResult::kNotHandled;

  if (any_touch_canceled_or_ended || touch_attribute_map_.size() > 1)
    suppressing_touchmoves_within_slop_ = false;

  if (suppressing_touchmoves_within_slop_) {
    // There is exactly one touch point here otherwise
    // |suppressing_touchmoves_within_slop_| would have been false.
    DCHECK_EQ(1U, touch_attribute_map_.size());
    const auto& touch_point_attribute = touch_attribute_map_.begin()->value;
    if (touch_point_attribute->event_.GetType() ==
        WebInputEvent::Type::kPointerMove) {
      if (!touch_point_attribute->event_.moved_beyond_slop_region)
        return WebInputEventResult::kHandledSuppressed;
      suppressing_touchmoves_within_slop_ = false;
    }
  }

  // Holds the complete set of touches on the screen.
  TouchList* touches = TouchList::Create();

  // A different view on the 'touches' list above, filtered and grouped by
  // event target. Used for the |targetTouches| list in the JS event.
  using TargetTouchesHeapMap =
      HeapHashMap<Member<EventTarget>, Member<TouchList>>;
  TargetTouchesHeapMap touches_by_target;

  // Array of touches per state, used to assemble the |changedTouches| list.
  ChangedTouches
      changed_touches[static_cast<int>(WebInputEvent::Type::kPointerTypeLast) -
                      static_cast<int>(WebInputEvent::Type::kPointerTypeFirst) +
                      1];

  Vector<int> available_ids;
  for (const auto& id : touch_attribute_map_.Keys())
    available_ids.push_back(id);
  std::sort(available_ids.begin(), available_ids.end());
  for (const int& touch_point_id : available_ids) {
    auto* const touch_point_attribute = touch_attribute_map_.at(touch_point_id);
    WebInputEvent::Type event_type = touch_point_attribute->event_.GetType();
    bool known_target;

    Touch* touch = CreateDomTouch(touch_point_attribute, &known_target);
    EventTarget* touch_target = touch->target();

    // Ensure this target's touch list exists, even if it ends up empty, so
    // it can always be passed to TouchEvent::Create below.
    TargetTouchesHeapMap::iterator target_touches_iterator =
        touches_by_target.find(touch_target);
    if (target_touches_iterator == touches_by_target.end()) {
      touches_by_target.Set(touch_target, TouchList::Create());
      target_touches_iterator = touches_by_target.find(touch_target);
    }

    // |touches| and |targetTouches| should only contain information about
    // touches still on the screen, so if this point is released or
    // cancelled it will only appear in the |changedTouches| list.
    if (event_type != WebInputEvent::Type::kPointerUp &&
        event_type != WebInputEvent::Type::kPointerCancel) {
      touches->Append(touch);
      target_touches_iterator->value->Append(touch);
    }

    // Now build up the correct list for |changedTouches|.
    // Note that  any touches that are in the TouchStationary state (e.g. if
    // the user had several points touched but did not move them all) should
    // never be in the |changedTouches| list so we do not handle them
    // explicitly here. See https://bugs.webkit.org/show_bug.cgi?id=37609
    // for further discussion about the TouchStationary state.
    if (!touch_point_attribute->stale_ && known_target) {
      size_t event_type_idx =
          static_cast<int>(event_type) -
          static_cast<int>(WebInputEvent::Type::kPointerTypeFirst);
      if (!changed_touches[event_type_idx].touches_)
        changed_touches[event_type_idx].touches_ = TouchList::Create();
      changed_touches[event_type_idx].touches_->Append(touch);
      changed_touches[event_type_idx].targets_.insert(touch_target);
    }
  }

  WebInputEventResult event_result = WebInputEventResult::kNotHandled;

  // First we construct the webcoalescedinputevent containing all the coalesced
  // touch event.
  WebCoalescedInputEvent coalesced_event = GenerateWebCoalescedInputEvent();

  // Now iterate through the |changedTouches| list and |m_targets| within it,
  // sending TouchEvents to the targets as required.
  for (unsigned action =
           static_cast<int>(WebInputEvent::Type::kPointerTypeFirst);
       action <= static_cast<int>(WebInputEvent::Type::kPointerTypeLast);
       ++action) {
    size_t action_idx =
        action - static_cast<int>(WebInputEvent::Type::kPointerTypeFirst);
    if (!changed_touches[action_idx].touches_)
      continue;

    const AtomicString& event_name(TouchEventNameForPointerEventType(
        static_cast<WebInputEvent::Type>(action)));

    for (const auto& event_target : changed_touches[action_idx].targets_) {
      EventTarget* touch_event_target = event_target;
      TouchEvent* touch_event = TouchEvent::Create(
          coalesced_event, touches, touches_by_target.at(touch_event_target),
          changed_touches[action_idx].touches_, event_name,
          touch_event_target->ToNode()->GetDocument().domWindow(),
          current_touch_action_);

      DispatchEventResult dom_dispatch_result =
          touch_event_target->DispatchEvent(*touch_event);

      event_result = event_handling_util::MergeEventResult(
          event_result,
          event_handling_util::ToWebInputEventResult(dom_dispatch_result));
    }
  }

  if (should_enforce_vertical_scroll_)
    event_result = EnsureVerticalScrollIsPossible(event_result);

  // Suppress following touchmoves within the slop region if the touchstart is
  // not consumed.
  if (all_touch_points_pressed &&
      event_result == WebInputEventResult::kNotHandled) {
    suppressing_touchmoves_within_slop_ = true;
  }

  return event_result;
}

Node* TouchEventManager::GetTouchPointerNode(
    const WebPointerEvent& event,
    const event_handling_util::PointerEventTarget& pointer_event_target) {
  DCHECK(event.GetType() == WebInputEvent::Type::kPointerDown);

  Node* touch_pointer_node = pointer_event_target.target_element;

  if (touch_sequence_document_ &&
      (!touch_pointer_node ||
       &touch_pointer_node->GetDocument() != touch_sequence_document_)) {
    if (!touch_sequence_document_->GetFrame())
      return nullptr;

    HitTestLocation location(PhysicalOffset::FromPointFRound(
        touch_sequence_document_->GetFrame()->View()->ConvertFromRootFrame(
            event.PositionInWidget())));
    HitTestRequest::HitTestRequestType hit_type = HitTestRequest::kTouchEvent |
                                                  HitTestRequest::kReadOnly |
                                                  HitTestRequest::kActive;
    HitTestResult result = event_handling_util::HitTestResultInFrame(
        touch_sequence_document_->GetFrame(), location, hit_type);
    Node* node = result.InnerNode();
    if (!node)
      return nullptr;
    // Touch events should not go to text nodes.
    if (node->IsTextNode())
      node = FlatTreeTraversal::Parent(*node);
    touch_pointer_node = node;
  }

  return touch_pointer_node;
}

void TouchEventManager::UpdateTouchAttributeMapsForPointerDown(
    const WebPointerEvent& event,
    Node* touch_node,
    TouchAction effective_touch_action) {
  DCHECK(event.GetType() == WebInputEvent::Type::kPointerDown);
  DCHECK(touch_node);

  // Ideally we'd DCHECK(!touch_attribute_map_.Contains(event.id))
  // since we shouldn't get a touchstart for a touch that's already
  // down. However EventSender allows this to be violated and there's
  // some tests that take advantage of it. There may also be edge
  // cases in the browser where this happens.
  // See http://crbug.com/345372.
  touch_attribute_map_.Set(event.id,
                           MakeGarbageCollected<TouchPointAttributes>(event));

  if (!touch_sequence_document_) {
    // Keep track of which document should receive all touch events
    // in the active sequence. This must be a single document to
    // ensure we don't leak Nodes between documents.
    touch_sequence_document_ = &(touch_node->GetDocument());
    DCHECK(touch_sequence_document_->GetFrame()->View());
  }

  TouchPointAttributes* attributes = touch_attribute_map_.at(event.id);
  attributes->target_ = touch_node;

  should_enforce_vertical_scroll_ =
      touch_sequence_document_->IsVerticalScrollEnforced();
  if (should_enforce_vertical_scroll_ &&
      HasEventHandlerInAncestorPath(
          touch_node, EventHandlerRegistry::kTouchStartOrMoveEventBlocking)) {
    delayed_effective_touch_action_ =
        delayed_effective_touch_action_.value_or(TouchAction::kAuto) &
        effective_touch_action;
  }
  if (!delayed_effective_touch_action_) {
    frame_->GetPage()->GetChromeClient().SetTouchAction(frame_,
                                                        effective_touch_action);
  }
  // Combine the current touch action sequence with the touch action
  // for the current finger press.
  current_touch_action_ &= effective_touch_action;
}

void TouchEventManager::HandleTouchPoint(
    const WebPointerEvent& event,
    const Vector<WebPointerEvent>& coalesced_events,
    const event_handling_util::PointerEventTarget& pointer_event_target) {
  DCHECK_GE(event.GetType(), WebInputEvent::Type::kPointerTypeFirst);
  DCHECK_LE(event.GetType(), WebInputEvent::Type::kPointerTypeLast);
  DCHECK_NE(event.GetType(), WebInputEvent::Type::kPointerCausedUaAction);

  if (touch_attribute_map_.empty()) {
    // Ideally we'd DCHECK(!m_touchSequenceDocument) here since we should
    // have cleared the active document when we saw the last release. But we
    // have some tests that violate this, ClusterFuzz could trigger it, and
    // there may be cases where the browser doesn't reliably release all
    // touches. http://crbug.com/345372 tracks this.
    AllTouchesReleasedCleanup();
  }

  DCHECK(frame_->View());
  if (touch_sequence_document_ &&
      (!touch_sequence_document_->GetFrame() ||
       !touch_sequence_document_->GetFrame()->View())) {
    // If the active touch document has no frame or view, it's probably being
    // destroyed so we can't dispatch events.
    // Update the points so they get removed in flush when they are released.
    if (touch_attribute_map_.Contains(event.id)) {
      TouchPointAttributes* attributes = touch_attribute_map_.at(event.id);
      attributes->event_ = event;
    }
    return;
  }

  // We might not receive the down action for a touch point. In that case we
  // would have never added them to |touch_attribute_map_| or hit-tested
  // them. For those just keep them in the map with a null target. Later they
  // will be targeted at the |touch_sequence_document_|.
  if (!touch_attribute_map_.Contains(event.id)) {
    touch_attribute_map_.insert(
        event.id, MakeGarbageCollected<TouchPointAttributes>(event));
  }

  TouchPointAttributes* attributes = touch_attribute_map_.at(event.id);
  attributes->event_ = event;
  attributes->coalesced_events_ = coalesced_events;
  attributes->stale_ = false;
}

WebInputEventResult TouchEventManager::FlushEvents() {
  WebInputEventResult result = WebInputEventResult::kNotHandled;

  // If there's no document receiving touch events, or no handlers on the
  // document set to receive the events, then we can skip all the rest of
  // sending the event.
  if (touch_sequence_document_ && touch_sequence_document_->GetPage() &&
      HasTouchHandlers(
          touch_sequence_document_->GetFrame()->GetEventHandlerRegistry()) &&
      touch_sequence_document_->GetFrame()->View()) {
    result = DispatchTouchEventFromAccumulatdTouchPoints();
  }

  // Cleanup the |touch_attribute_map_| map from released and canceled
  // touch points.
  Vector<int> released_canceled_points;
  for (auto& attributes : touch_attribute_map_.Values()) {
    if (attributes->event_.GetType() == WebInputEvent::Type::kPointerUp ||
        attributes->event_.GetType() == WebInputEvent::Type::kPointerCancel) {
      released_canceled_points.push_back(attributes->event_.id);
    } else {
      attributes->stale_ = true;
      attributes->event_.movement_x = 0;
      attributes->event_.movement_y = 0;
      attributes->coalesced_events_.clear();
    }
  }
  touch_attribute_map_.RemoveAll(released_canceled_points);

  if (touch_attribute_map_.empty()) {
    AllTouchesReleasedCleanup();
  }

  return result;
}

void TouchEventManager::AllTouchesReleasedCleanup() {
  touch_sequence_document_.Clear();
  current_touch_action_ = TouchAction::kAuto;
  last_coalesced_touch_event_ = WebTouchEvent();
  // Ideally, we should have DCHECK(!delayed_effective_touch_action_) but we do
  // we do actually get here from HandleTouchPoint(). Supposedly, if there has
  // been a |touch_sequence_document_| and nothing in the |touch_attribute_map_|
  // we still get here and if |touch_sequence_document| was of the type which
  // cannot block scroll, then the flag is certainly set
  // (https://crbug.com/345372).
  delayed_effective_touch_action_ = std::nullopt;
  should_enforce_vertical_scroll_ = false;
}

bool TouchEventManager::IsAnyTouchActive() const {
  return !touch_attribute_map_.empty();
}

Element* TouchEventManager::CurrentTouchDownElement() {
  if (touch_attribute_map_.empty() || touch_attribute_map_.size() > 1)
    return nullptr;
  Node* touch_node = touch_attribute_map_.begin()->value->target_;
  return touch_node ? DynamicTo<Element>(*touch_node) : nullptr;
}

WebInputEventResult TouchEventManager::EnsureVerticalScrollIsPossible(
    WebInputEventResult event_result) {
  bool prevent_defaulted =
      event_result == WebInputEventResult::kHandledApplication;
  if (prevent_defaulted && delayed_effective_touch_action_) {
    // Make sure that only vertical scrolling is permitted.
    *delayed_effective_touch_action_ &= TouchAction::kPanY;
  }

  if (delayed_effective_touch_action_) {
    // If 'touchstart' is preventDefault()-ed then we can proceed with reporting
    // the effective 'touch-action'.
    // TODO(ekaramad): This does not block horizontal scroll after enforcing
    // vertical scrolling. We should ideally send the 'touch-action' to browser
    // after the first 'touchmove' event has been dispatched.
    // (https://crbug.com/844493).
    frame_->GetPage()->GetChromeClient().SetTouchAction(
        frame_, delayed_effective_touch_action_.value());
    delayed_effective_touch_action_ = std::nullopt;
  }

  // If the event was canceled the result is ignored to make sure vertical
  // scrolling is possible.
  return prevent_defaulted ? WebInputEventResult::kNotHandled : event_result;
}

}  // namespace blink
