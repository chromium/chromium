// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/event_handler_registry.h"

#include "third_party/blink/renderer/core/dom/events/event_listener_options.h"
#include "third_party/blink/renderer/core/events/event_util.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator.h"

namespace blink {

namespace {

cc::EventListenerProperties GetEventListenerProperties(bool has_blocking,
                                                       bool has_passive) {
  if (has_blocking && has_passive)
    return cc::EventListenerProperties::kBlockingAndPassive;
  if (has_blocking)
    return cc::EventListenerProperties::kBlocking;
  if (has_passive)
    return cc::EventListenerProperties::kPassive;
  return cc::EventListenerProperties::kNone;
}

LocalFrame* GetLocalFrameForTarget(EventTarget* target) {
  LocalFrame* frame = nullptr;
  if (Node* node = target->ToNode()) {
    frame = node->GetDocument().GetFrame();
  } else if (LocalDOMWindow* dom_window = target->ToLocalDOMWindow()) {
    frame = dom_window->GetFrame();
  } else {
    NOTREACHED() << "Unexpected target type for event handler.";
  }
  return frame;
}

}  // namespace

EventHandlerRegistry::EventHandlerRegistry(LocalFrame& frame) : frame_(frame) {}

EventHandlerRegistry::~EventHandlerRegistry() {
  for (int i = 0; i < kEventHandlerClassCount; ++i) {
    EventHandlerClass handler_class = static_cast<EventHandlerClass>(i);
    CheckConsistency(handler_class);
  }
}

bool EventHandlerRegistry::EventTypeToClass(
    const AtomicString& event_type,
    const AddEventListenerOptions* options,
    EventHandlerClass* result) {
  if (event_type == event_type_names::kScroll) {
    *result = kScrollEvent;
  } else if (event_type == event_type_names::kWheel ||
             event_type == event_type_names::kMousewheel) {
    *result = options->passive() ? kWheelEventPassive : kWheelEventBlocking;
  } else if (event_type == event_type_names::kTouchend ||
             event_type == event_type_names::kTouchcancel) {
    *result = options->passive() ? kTouchEndOrCancelEventPassive
                                 : kTouchEndOrCancelEventBlocking;
  } else if (event_type == event_type_names::kTouchstart ||
             event_type == event_type_names::kTouchmove) {
    *result = options->passive() ? kTouchStartOrMoveEventPassive
                                 : kTouchStartOrMoveEventBlocking;
  } else if (event_type == event_type_names::kPointerrawupdate) {
    // This will be used to avoid waking up the main thread to
    // process pointerrawupdate events and hit-test them when
    // there is no listener on the page.
    *result = kPointerRawUpdateEvent;
  } else if (event_util::IsPointerEventType(event_type)) {
    // The pointer events never block scrolling and the compositor
    // only needs to know about the touch listeners.
    *result = kPointerEvent;
#if DCHECK_IS_ON()
  } else if (event_type == event_type_names::kLoad ||
             event_type == event_type_names::kMousemove ||
             event_type == event_type_names::kTouchstart) {
    *result = kEventsForTesting;
#endif
  } else {
    return false;
  }
  return true;
}

const EventTargetSet* EventHandlerRegistry::EventHandlerTargets(
    EventHandlerClass handler_class) const {
  CheckConsistency(handler_class);
  return &targets_[handler_class];
}

bool EventHandlerRegistry::HasEventHandlers(
    EventHandlerClass handler_class) const {
  CheckConsistency(handler_class);
  return targets_[handler_class].size();
}

bool EventHandlerRegistry::UpdateEventHandlerTargets(
    ChangeOperation op,
    EventHandlerClass handler_class,
    EventTarget* target) {
  EventTargetSet* targets = &targets_[handler_class];
  if (op == kAdd) {
    if (!targets->insert(target).is_new_entry) {
      // Just incremented refcount, no real change.
      return false;
    }
  } else {
    DCHECK(op == kRemove || op == kRemoveAll);
    DCHECK(op == kRemoveAll || targets->Contains(target));

    if (op == kRemoveAll) {
      if (!targets->Contains(target))
        return false;
      targets->RemoveAll(target);
    } else {
      if (!targets->erase(target)) {
        // Just decremented refcount, no real update.
        return false;
      }
    }
  }
  return true;
}

bool EventHandlerRegistry::UpdateEventHandlerInternal(
    ChangeOperation op,
    EventHandlerClass handler_class,
    EventTarget* target) {
  unsigned old_num_handlers = targets_[handler_class].size();
  bool target_set_changed =
      UpdateEventHandlerTargets(op, handler_class, target);
  unsigned new_num_handlers = targets_[handler_class].size();

  bool handlers_changed = old_num_handlers != new_num_handlers;

  if (op != kRemoveAll) {
    if (handlers_changed)
      NotifyHandlersChanged(target, handler_class, new_num_handlers > 0);

    if (target_set_changed) {
      NotifyDidAddOrRemoveEventHandlerTarget(GetLocalFrameForTarget(target),
                                             handler_class);
    }
  }
  return handlers_changed;
}

void EventHandlerRegistry::UpdateEventHandlerOfType(
    ChangeOperation op,
    const AtomicString& event_type,
    const AddEventListenerOptions* options,
    EventTarget* target) {
  EventHandlerClass handler_class;
  if (!EventTypeToClass(event_type, options, &handler_class))
    return;
  UpdateEventHandlerInternal(op, handler_class, target);
}

void EventHandlerRegistry::DidAddEventHandler(
    EventTarget& target,
    const AtomicString& event_type,
    const AddEventListenerOptions* options) {
  UpdateEventHandlerOfType(kAdd, event_type, options, &target);
}

void EventHandlerRegistry::DidRemoveEventHandler(
    EventTarget& target,
    const AtomicString& event_type,
    const AddEventListenerOptions* options) {
  UpdateEventHandlerOfType(kRemove, event_type, options, &target);
}

void EventHandlerRegistry::DidAddEventHandler(EventTarget& target,
                                              EventHandlerClass handler_class) {
  UpdateEventHandlerInternal(kAdd, handler_class, &target);
}

void EventHandlerRegistry::DidRemoveEventHandler(
    EventTarget& target,
    EventHandlerClass handler_class) {
  UpdateEventHandlerInternal(kRemove, handler_class, &target);
}

void EventHandlerRegistry::DidMoveIntoPage(EventTarget& target) {
  if (!target.HasEventListeners())
    return;

  // This code is not efficient at all.
  Vector<AtomicString> event_types = target.EventTypes();
  for (wtf_size_t i = 0; i < event_types.size(); ++i) {
    EventListenerVector* listeners = target.GetEventListeners(event_types[i]);
    if (!listeners)
      continue;
    for (wtf_size_t count = listeners->size(); count > 0; --count) {
      EventHandlerClass handler_class;
      if (!EventTypeToClass(event_types[i], (*listeners)[count - 1].Options(),
                            &handler_class))
        continue;

      DidAddEventHandler(target, handler_class);
    }
  }
}

void EventHandlerRegistry::DidMoveOutOfPage(EventTarget& target) {
  DidRemoveAllEventHandlers(target);
}

void EventHandlerRegistry::DidRemoveAllEventHandlers(EventTarget& target) {
  bool handlers_changed[kEventHandlerClassCount];
  bool target_set_changed[kEventHandlerClassCount];

  for (int i = 0; i < kEventHandlerClassCount; ++i) {
    EventHandlerClass handler_class = static_cast<EventHandlerClass>(i);

    EventTargetSet* targets = &targets_[handler_class];
    target_set_changed[i] = targets->Contains(&target);

    handlers_changed[i] =
        UpdateEventHandlerInternal(kRemoveAll, handler_class, &target);
  }

  for (int i = 0; i < kEventHandlerClassCount; ++i) {
    EventHandlerClass handler_class = static_cast<EventHandlerClass>(i);
    if (handlers_changed[i]) {
      bool has_handlers = targets_[handler_class].Contains(&target);
      NotifyHandlersChanged(&target, handler_class, has_handlers);
    }
    if (target_set_changed[i]) {
      NotifyDidAddOrRemoveEventHandlerTarget(GetLocalFrameForTarget(&target),
                                             handler_class);
    }
  }
}

void EventHandlerRegistry::NotifyHandlersChanged(
    EventTarget* target,
    EventHandlerClass handler_class,
    bool has_active_handlers) {
  LocalFrame* frame = GetLocalFrameForTarget(target);

  switch (handler_class) {
    case kScrollEvent:
      GetPage()->GetChromeClient().SetHasScrollEventHandlers(
          frame, has_active_handlers);
      break;
    case kWheelEventBlocking:
    case kWheelEventPassive:
      GetPage()->GetChromeClient().SetEventListenerProperties(
          frame, cc::EventListenerClass::kMouseWheel,
          GetEventListenerProperties(HasEventHandlers(kWheelEventBlocking),
                                     HasEventHandlers(kWheelEventPassive)));
      break;
    case kTouchStartOrMoveEventBlockingLowLatency:
      GetPage()->GetChromeClient().SetNeedsLowLatencyInput(frame,
                                                           has_active_handlers);
      FALLTHROUGH;
    case kTouchAction:
    case kTouchStartOrMoveEventBlocking:
    case kTouchStartOrMoveEventPassive:
    case kPointerEvent:
      GetPage()->GetChromeClient().SetEventListenerProperties(
          frame, cc::EventListenerClass::kTouchStartOrMove,
          GetEventListenerProperties(
              HasEventHandlers(kTouchAction) ||
                  HasEventHandlers(kTouchStartOrMoveEventBlocking) ||
                  HasEventHandlers(kTouchStartOrMoveEventBlockingLowLatency),
              HasEventHandlers(kTouchStartOrMoveEventPassive) ||
                  HasEventHandlers(kPointerEvent)));
      break;
    case kPointerRawUpdateEvent:
      GetPage()->GetChromeClient().SetEventListenerProperties(
          frame, cc::EventListenerClass::kPointerRawUpdate,
          GetEventListenerProperties(false,
                                     HasEventHandlers(kPointerRawUpdateEvent)));
      break;
    case kTouchEndOrCancelEventBlocking:
    case kTouchEndOrCancelEventPassive:
      GetPage()->GetChromeClient().SetEventListenerProperties(
          frame, cc::EventListenerClass::kTouchEndOrCancel,
          GetEventListenerProperties(
              HasEventHandlers(kTouchEndOrCancelEventBlocking),
              HasEventHandlers(kTouchEndOrCancelEventPassive)));
      break;
#if DCHECK_IS_ON()
    case kEventsForTesting:
      break;
#endif
    default:
      NOTREACHED();
      break;
  }

  if (handler_class == kTouchStartOrMoveEventBlocking ||
      handler_class == kTouchStartOrMoveEventBlockingLowLatency) {
    if (auto* node = target->ToNode()) {
      if (auto* layout_object = node->GetLayoutObject()) {
        layout_object->MarkEffectiveAllowedTouchActionChanged();
        auto* continuation = layout_object->VirtualContinuation();
        while (continuation) {
          continuation->MarkEffectiveAllowedTouchActionChanged();
          continuation = continuation->VirtualContinuation();
        }
      }
    } else if (auto* dom_window = target->ToLocalDOMWindow()) {
      // This event handler is on a window. Ensure the layout view is
      // invalidated because the layout view tracks the window's blocking
      // touch event rects.
      if (auto* layout_view = dom_window->GetFrame()->ContentLayoutObject())
        layout_view->MarkEffectiveAllowedTouchActionChanged();
    }
  }
}

void EventHandlerRegistry::NotifyDidAddOrRemoveEventHandlerTarget(
    LocalFrame* frame,
    EventHandlerClass handler_class) {
  ScrollingCoordinator* scrolling_coordinator =
      GetPage()->GetScrollingCoordinator();
  if (scrolling_coordinator &&
      (handler_class == kTouchAction ||
       handler_class == kTouchStartOrMoveEventBlocking ||
       handler_class == kTouchStartOrMoveEventBlockingLowLatency)) {
    scrolling_coordinator->TouchEventTargetRectsDidChange(
        &frame->LocalFrameRoot());
  }
}

void EventHandlerRegistry::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_);
  visitor->template RegisterWeakCallbackMethod<
      EventHandlerRegistry, &EventHandlerRegistry::ProcessCustomWeakness>(this);
}

void EventHandlerRegistry::ProcessCustomWeakness(const WeakCallbackInfo& info) {
  Vector<UntracedMember<EventTarget>> dead_targets;
  for (int i = 0; i < kEventHandlerClassCount; ++i) {
    EventHandlerClass handler_class = static_cast<EventHandlerClass>(i);
    const EventTargetSet* targets = &targets_[handler_class];
    for (const auto& event_target : *targets) {
      Node* node = event_target.key->ToNode();
      LocalDOMWindow* window = event_target.key->ToLocalDOMWindow();
      if (node && !info.IsHeapObjectAlive(node)) {
        dead_targets.push_back(node);
      } else if (window && !info.IsHeapObjectAlive(window)) {
        dead_targets.push_back(window);
      }
    }
  }
  for (wtf_size_t i = 0; i < dead_targets.size(); ++i)
    DidRemoveAllEventHandlers(*dead_targets[i]);
}

void EventHandlerRegistry::DocumentDetached(Document& document) {
  // Remove all event targets under the detached document.
  for (int handler_class_index = 0;
       handler_class_index < kEventHandlerClassCount; ++handler_class_index) {
    EventHandlerClass handler_class =
        static_cast<EventHandlerClass>(handler_class_index);
    Vector<UntracedMember<EventTarget>> targets_to_remove;
    const EventTargetSet* targets = &targets_[handler_class];
    for (const auto& event_target : *targets) {
      if (Node* node = event_target.key->ToNode()) {
        for (Document* doc = &node->GetDocument(); doc;
             doc = doc->LocalOwner() ? &doc->LocalOwner()->GetDocument()
                                     : nullptr) {
          if (doc == &document) {
            targets_to_remove.push_back(event_target.key);
            break;
          }
        }
      } else if (event_target.key->ToLocalDOMWindow()) {
        // DOMWindows may outlive their documents, so we shouldn't remove their
        // handlers here.
      } else {
        NOTREACHED();
      }
    }
    for (wtf_size_t i = 0; i < targets_to_remove.size(); ++i)
      UpdateEventHandlerInternal(kRemoveAll, handler_class,
                                 targets_to_remove[i]);
  }
}

void EventHandlerRegistry::CheckConsistency(
    EventHandlerClass handler_class) const {
#if DCHECK_IS_ON()
  const EventTargetSet* targets = &targets_[handler_class];
  for (const auto& event_target : *targets) {
    if (Node* node = event_target.key->ToNode()) {
      // See the comment for |documentDetached| if either of these assertions
      // fails.
      DCHECK(node->GetDocument().GetPage());
      DCHECK_EQ(frame_, &node->GetDocument().GetFrame()->LocalFrameRoot());
    } else if (LocalDOMWindow* window = event_target.key->ToLocalDOMWindow()) {
      // If any of these assertions fail, LocalDOMWindow failed to unregister
      // its handlers properly.
      DCHECK(window->GetFrame());
      DCHECK(window->GetFrame()->GetPage());
      DCHECK_EQ(frame_, &window->GetFrame()->LocalFrameRoot());
    }
  }
#endif  // DCHECK_IS_ON()
}

Page* EventHandlerRegistry::GetPage() const {
  DCHECK(frame_->GetPage());
  return frame_->GetPage();
}

}  // namespace blink
