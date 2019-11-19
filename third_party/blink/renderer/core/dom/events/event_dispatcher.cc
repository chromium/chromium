/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/dom/events/event_dispatcher.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatch_forbidden_scope.h"
#include "third_party/blink/renderer/core/dom/events/event_path.h"
#include "third_party/blink/renderer/core/dom/events/scoped_event_queue.h"
#include "third_party/blink/renderer/core/dom/events/window_event_context.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/frame/ad_tracker.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/spatial_navigation_controller.h"
#include "third_party/blink/renderer/core/timing/event_timing.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

DispatchEventResult EventDispatcher::DispatchEvent(Node& node, Event& event) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("blink.debug"),
               "EventDispatcher::dispatchEvent");
#if DCHECK_IS_ON()
  DCHECK(!EventDispatchForbiddenScope::IsEventDispatchForbidden());
#endif
  EventDispatcher dispatcher(node, event);
  return event.DispatchEvent(dispatcher);
}

EventDispatcher::EventDispatcher(Node& node, Event& event)
    : node_(node), event_(event) {
  view_ = node.GetDocument().View();
  event_->InitEventPath(*node_);
}

void EventDispatcher::DispatchScopedEvent(Node& node, Event& event) {
  // We need to set the target here because it can go away by the time we
  // actually fire the event.
  event.SetTarget(&EventPath::EventTargetRespectingTargetRules(node));
  ScopedEventQueue::Instance()->EnqueueEvent(event);
}

void EventDispatcher::DispatchSimulatedClick(
    Node& node,
    Event* underlying_event,
    SimulatedClickMouseEventOptions mouse_event_options,
    SimulatedClickCreationScope creation_scope) {
  // This persistent vector doesn't cause leaks, because added Nodes are removed
  // before dispatchSimulatedClick() returns. This vector is here just to
  // prevent the code from running into an infinite recursion of
  // dispatchSimulatedClick().
  DEFINE_STATIC_LOCAL(Persistent<HeapHashSet<Member<Node>>>,
                      nodes_dispatching_simulated_clicks,
                      (MakeGarbageCollected<HeapHashSet<Member<Node>>>()));

  if (IsDisabledFormControl(&node))
    return;

  if (nodes_dispatching_simulated_clicks->Contains(&node))
    return;

  nodes_dispatching_simulated_clicks->insert(&node);

  if (mouse_event_options == kSendMouseOverUpDownEvents)
    EventDispatcher(node, *MouseEvent::Create(event_type_names::kMouseover,
                                              node.GetDocument().domWindow(),
                                              underlying_event, creation_scope))
        .Dispatch();

  Element* element = DynamicTo<Element>(node);
  if (mouse_event_options != kSendNoEvents) {
    EventDispatcher(node, *MouseEvent::Create(event_type_names::kMousedown,
                                              node.GetDocument().domWindow(),
                                              underlying_event, creation_scope))
        .Dispatch();
    if (element)
      element->SetActive(true);
    EventDispatcher(node, *MouseEvent::Create(event_type_names::kMouseup,
                                              node.GetDocument().domWindow(),
                                              underlying_event, creation_scope))
        .Dispatch();
  }
  // Some elements (e.g. the color picker) may set active state to true before
  // calling this method and expect the state to be reset during the call.
  if (element)
    element->SetActive(false);

  // always send click
  EventDispatcher(node, *MouseEvent::Create(event_type_names::kClick,
                                            node.GetDocument().domWindow(),
                                            underlying_event, creation_scope))
      .Dispatch();

  nodes_dispatching_simulated_clicks->erase(&node);
}

// https://dom.spec.whatwg.org/#dispatching-events
DispatchEventResult EventDispatcher::Dispatch() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("blink.debug"),
               "EventDispatcher::dispatch");

#if DCHECK_IS_ON()
  DCHECK(!event_dispatched_);
  event_dispatched_ = true;
#endif
  if (GetEvent().GetEventPath().IsEmpty()) {
    // eventPath() can be empty if event path is shrinked by relataedTarget
    // retargeting.
    return DispatchEventResult::kNotCanceled;
  }
  std::unique_ptr<EventTiming> eventTiming;
  LocalFrame* frame = node_->GetDocument().GetFrame();
  if (frame && frame->DomWindow())
    eventTiming = EventTiming::Create(frame->DomWindow(), *event_);
  event_->GetEventPath().EnsureWindowEventContext();

  const bool is_click =
      event_->IsMouseEvent() && event_->type() == event_type_names::kClick;

  if (is_click && event_->isTrusted()) {
    Document& document = node_->GetDocument();
    if (frame) {
      // A genuine mouse click cannot be triggered by script so we don't expect
      // there are any script in the stack.
      DCHECK(!frame->GetAdTracker() ||
             !frame->GetAdTracker()->IsAdScriptInStack());
      if (frame->IsAdSubframe()) {
        UseCounter::Count(document, WebFeature::kAdClick);
      }
    }
  }

  // 6. Let isActivationEvent be true, if event is a MouseEvent object and
  // event's type attribute is "click", and false otherwise.
  //
  // We need to include non-standard textInput event for HTMLInputElement.
  const bool is_activation_event =
      is_click || event_->type() == event_type_names::kTextInput;

  // 7. Let activationTarget be target, if isActivationEvent is true and target
  // has activation behavior, and null otherwise.
  Node* activation_target =
      is_activation_event && node_->HasActivationBehavior() ? node_ : nullptr;

  // A part of step 9 loop.
  if (is_activation_event && !activation_target && event_->bubbles()) {
    wtf_size_t size = event_->GetEventPath().size();
    for (wtf_size_t i = 1; i < size; ++i) {
      Node& target = event_->GetEventPath()[i].GetNode();
      if (target.HasActivationBehavior()) {
        activation_target = &target;
        break;
      }
    }
  }

  event_->SetTarget(&EventPath::EventTargetRespectingTargetRules(*node_));
#if DCHECK_IS_ON()
  DCHECK(!EventDispatchForbiddenScope::IsEventDispatchForbidden());
#endif
  DCHECK(event_->target());
  TRACE_EVENT1("devtools.timeline", "EventDispatch", "data",
               inspector_event_dispatch_event::Data(*event_));
  EventDispatchHandlingState* pre_dispatch_event_handler_result = nullptr;
  if (DispatchEventPreProcess(activation_target,
                              pre_dispatch_event_handler_result) ==
      kContinueDispatching) {
    if (DispatchEventAtCapturing() == kContinueDispatching) {
      // TODO(crbug/882574): Remove these.
      CHECK(event_->HasEventPath());
      CHECK(!event_->GetEventPath().IsEmpty());
      if (DispatchEventAtTarget() == kContinueDispatching)
        DispatchEventAtBubbling();
    }
  }
  DispatchEventPostProcess(activation_target,
                           pre_dispatch_event_handler_result);
  if (eventTiming)
    eventTiming->DidDispatchEvent(*event_);

  return EventTarget::GetDispatchEventResult(*event_);
}

inline EventDispatchContinuation EventDispatcher::DispatchEventPreProcess(
    Node* activation_target,
    EventDispatchHandlingState*& pre_dispatch_event_handler_result) {
  // 11. If activationTarget is non-null and activationTarget has
  // legacy-pre-activation behavior, then run activationTarget's
  // legacy-pre-activation behavior.
  if (activation_target) {
    pre_dispatch_event_handler_result =
        activation_target->PreDispatchEventHandler(*event_);
  }

  return (event_->GetEventPath().IsEmpty() || event_->PropagationStopped())
             ? kDoneDispatching
             : kContinueDispatching;
}

inline EventDispatchContinuation EventDispatcher::DispatchEventAtCapturing() {
  // Trigger capturing event handlers, starting at the top and working our way
  // down.
  event_->SetEventPhase(Event::kCapturingPhase);

  if (event_->GetEventPath().GetWindowEventContext().HandleLocalEvents(
          *event_) &&
      event_->PropagationStopped())
    return kDoneDispatching;

  for (wtf_size_t i = event_->GetEventPath().size() - 1; i > 0; --i) {
    const NodeEventContext& event_context = event_->GetEventPath()[i];
    if (event_context.CurrentTargetSameAsTarget()) {
      event_->SetEventPhase(Event::kAtTarget);
      event_->SetFireOnlyCaptureListenersAtTarget(true);
      event_context.HandleLocalEvents(*event_);
      event_->SetFireOnlyCaptureListenersAtTarget(false);
    } else {
      event_->SetEventPhase(Event::kCapturingPhase);
      event_context.HandleLocalEvents(*event_);
    }
    if (event_->PropagationStopped())
      return kDoneDispatching;
  }

  return kContinueDispatching;
}

inline EventDispatchContinuation EventDispatcher::DispatchEventAtTarget() {
  event_->SetEventPhase(Event::kAtTarget);
  event_->GetEventPath()[0].HandleLocalEvents(*event_);
  return event_->PropagationStopped() ? kDoneDispatching : kContinueDispatching;
}

inline void EventDispatcher::DispatchEventAtBubbling() {
  // Trigger bubbling event handlers, starting at the bottom and working our way
  // up.
  wtf_size_t size = event_->GetEventPath().size();
  for (wtf_size_t i = 1; i < size; ++i) {
    const NodeEventContext& event_context = event_->GetEventPath()[i];
    if (event_context.CurrentTargetSameAsTarget()) {
      // TODO(hayato): Need to check cancelBubble() also here?
      event_->SetEventPhase(Event::kAtTarget);
      event_->SetFireOnlyNonCaptureListenersAtTarget(true);
      event_context.HandleLocalEvents(*event_);
      event_->SetFireOnlyNonCaptureListenersAtTarget(false);
    } else if (event_->bubbles() && !event_->cancelBubble()) {
      event_->SetEventPhase(Event::kBubblingPhase);
      event_context.HandleLocalEvents(*event_);
    } else {
      continue;
    }
    if (event_->PropagationStopped())
      return;
  }
  if (event_->bubbles() && !event_->cancelBubble()) {
    event_->SetEventPhase(Event::kBubblingPhase);
    event_->GetEventPath().GetWindowEventContext().HandleLocalEvents(*event_);
  }
}

inline void EventDispatcher::DispatchEventPostProcess(
    Node* activation_target,
    EventDispatchHandlingState* pre_dispatch_event_handler_result) {
  event_->SetTarget(&EventPath::EventTargetRespectingTargetRules(*node_));
  // https://dom.spec.whatwg.org/#concept-event-dispatch
  // 14. Unset event’s dispatch flag, stop propagation flag, and stop immediate
  // propagation flag.
  event_->SetStopPropagation(false);
  event_->SetStopImmediatePropagation(false);
  // 15. Set event’s eventPhase attribute to NONE.
  event_->SetEventPhase(0);
  // TODO(rakina): investigate this and move it to the bottom of step 16
  // 17. Set event’s currentTarget attribute to null.
  event_->SetCurrentTarget(nullptr);

  bool is_click = event_->IsMouseEvent() &&
                  ToMouseEvent(*event_).type() == event_type_names::kClick;
  if (is_click) {
    // Fire an accessibility event indicating a node was clicked on.  This is
    // safe if event_->target()->ToNode() returns null.
    if (AXObjectCache* cache = node_->GetDocument().ExistingAXObjectCache())
      cache->HandleClicked(event_->target()->ToNode());

    // Pass the data from the PreDispatchEventHandler to the
    // PostDispatchEventHandler.
    // This may dispatch an event, and node_ and event_ might be altered.
    if (activation_target) {
      activation_target->PostDispatchEventHandler(
          *event_, pre_dispatch_event_handler_result);
    }
    // TODO(tkent): Is it safe to kick DefaultEventHandler() with such altered
    // event_?
  }

  // The DOM Events spec says that events dispatched by JS (other than "click")
  // should not have their default handlers invoked.
  bool is_trusted_or_click = event_->isTrusted() || is_click;

  // For Android WebView (distinguished by wideViewportQuirkEnabled)
  // enable untrusted events for mouse down on select elements because
  // fastclick.js seems to generate these. crbug.com/642698
  // TODO(dtapuska): Change this to a target SDK quirk crbug.com/643705
  if (!is_trusted_or_click && event_->IsMouseEvent() &&
      event_->type() == event_type_names::kMousedown &&
      IsA<HTMLSelectElement>(*node_)) {
    if (Settings* settings = node_->GetDocument().GetSettings()) {
      is_trusted_or_click = settings->GetWideViewportQuirkEnabled();
    }
  }

  // Call default event handlers. While the DOM does have a concept of
  // preventing default handling, the detail of which handlers are called is an
  // internal implementation detail and not part of the DOM.
  if (event_->defaultPrevented()) {
    if (activation_target)
      activation_target->DidPreventDefault(*event_);
  } else if (!event_->DefaultHandled() && is_trusted_or_click) {
    // Non-bubbling events call only one default event handler, the one for the
    // target.
    node_->DefaultEventHandler(*event_);
    DCHECK(!event_->defaultPrevented());
    // For bubbling events, call default event handlers on the same targets in
    // the same order as the bubbling phase.
    if (!event_->DefaultHandled() && event_->bubbles()) {
      wtf_size_t size = event_->GetEventPath().size();
      for (wtf_size_t i = 1; i < size; ++i) {
        event_->GetEventPath()[i].GetNode().DefaultEventHandler(*event_);
        DCHECK(!event_->defaultPrevented());
        if (event_->DefaultHandled())
          break;
      }
    }
  }

  if (Page* page = node_->GetDocument().GetPage()) {
    if (page->GetSettings().GetSpatialNavigationEnabled() &&
        is_trusted_or_click && event_->IsKeyboardEvent() &&
        ToKeyboardEvent(*event_).key() == "Enter" &&
        event_->type() == event_type_names::kKeyup) {
      page->GetSpatialNavigationController().ResetEnterKeyState();
    }
  }

  // Track the usage of sending a mousedown event to a select element to force
  // it to open. This measures a possible breakage of not allowing untrusted
  // events to open select boxes.
  if (!event_->isTrusted() && event_->IsMouseEvent() &&
      event_->type() == event_type_names::kMousedown &&
      IsA<HTMLSelectElement>(*node_)) {
    UseCounter::Count(node_->GetDocument(),
                      WebFeature::kUntrustedMouseDownEventDispatchedToSelect);
  }
  // 16. If target's root is a shadow root, then set event's target attribute
  // and event's relatedTarget to null.
  event_->SetTarget(event_->GetEventPath().GetWindowEventContext().Target());
  if (!event_->target())
    event_->SetRelatedTargetIfExists(nullptr);
}

}  // namespace blink
