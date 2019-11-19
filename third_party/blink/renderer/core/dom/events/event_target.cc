/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 *           (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "third_party/blink/renderer/core/dom/events/event_target.h"

#include <memory>

#include "base/format_macros.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/renderer/bindings/core/v8/add_event_listener_options_or_boolean.h"
#include "third_party/blink/renderer/bindings/core/v8/event_listener_options_or_boolean.h"
#include "third_party/blink/renderer/bindings/core/v8/js_based_event_listener.h"
#include "third_party/blink/renderer/bindings/core/v8/js_event_listener.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatch_forbidden_scope.h"
#include "third_party/blink/renderer/core/dom/events/event_target_impl.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/events/event_util.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/performance_monitor.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_activity_logger.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
namespace {

enum PassiveForcedListenerResultType {
  kPreventDefaultNotCalled,
  kDocumentLevelTouchPreventDefaultCalled,
  kPassiveForcedListenerResultTypeMax
};

Event::PassiveMode EventPassiveMode(
    const RegisteredEventListener& event_listener) {
  if (!event_listener.Passive()) {
    if (event_listener.PassiveSpecified())
      return Event::PassiveMode::kNotPassive;
    return Event::PassiveMode::kNotPassiveDefault;
  }
  if (event_listener.PassiveForcedForDocumentTarget())
    return Event::PassiveMode::kPassiveForcedDocumentLevel;
  if (event_listener.PassiveSpecified())
    return Event::PassiveMode::kPassive;
  return Event::PassiveMode::kPassiveDefault;
}

Settings* WindowSettings(LocalDOMWindow* executing_window) {
  if (executing_window) {
    if (LocalFrame* frame = executing_window->GetFrame()) {
      return frame->GetSettings();
    }
  }
  return nullptr;
}

bool IsTouchScrollBlockingEvent(const AtomicString& event_type) {
  return event_type == event_type_names::kTouchstart ||
         event_type == event_type_names::kTouchmove;
}

bool IsWheelScrollBlockingEvent(const AtomicString& event_type) {
  return event_type == event_type_names::kMousewheel ||
         event_type == event_type_names::kWheel;
}

bool IsScrollBlockingEvent(const AtomicString& event_type) {
  return IsTouchScrollBlockingEvent(event_type) ||
         IsWheelScrollBlockingEvent(event_type);
}

bool IsInstrumentedForAsyncStack(const AtomicString& event_type) {
  return event_type == event_type_names::kLoad ||
         event_type == event_type_names::kError;
}

base::TimeDelta BlockedEventsWarningThreshold(ExecutionContext* context,
                                              const Event& event) {
  if (!event.cancelable())
    return base::TimeDelta();
  if (!IsScrollBlockingEvent(event.type()))
    return base::TimeDelta();
  return PerformanceMonitor::Threshold(context,
                                       PerformanceMonitor::kBlockedEvent);
}

void ReportBlockedEvent(EventTarget& target,
                        const Event& event,
                        RegisteredEventListener* registered_listener,
                        base::TimeDelta delayed) {
  JSBasedEventListener* listener =
      DynamicTo<JSBasedEventListener>(registered_listener->Callback());
  if (!listener)
    return;

  String message_text = String::Format(
      "Handling of '%s' input event was delayed for %" PRId64
      " ms due to main thread being busy. "
      "Consider marking event handler as 'passive' to make the page more "
      "responsive.",
      event.type().GetString().Utf8().c_str(), delayed.InMilliseconds());
  PerformanceMonitor::ReportGenericViolation(
      target.GetExecutionContext(), PerformanceMonitor::kBlockedEvent,
      message_text, delayed, listener->GetSourceLocation(target));
  registered_listener->SetBlockedEventWarningEmitted();
}

// UseCounts the event if it has the specified type. Returns true iff the event
// type matches.
bool CheckTypeThenUseCount(const Event& event,
                           const AtomicString& event_type_to_count,
                           const WebFeature feature,
                           Document& document) {
  if (event.type() != event_type_to_count)
    return false;
  UseCounter::Count(document, feature);
  return true;
}

void CountFiringEventListeners(const Event& event,
                               const LocalDOMWindow* executing_window) {
  if (!executing_window)
    return;
  if (!executing_window->document())
    return;
  Document& document = *executing_window->document();

  if (event.type() == event_type_names::kToggle &&
      document.ToggleDuringParsing()) {
    UseCounter::Count(document, WebFeature::kToggleEventHandlerDuringParsing);
    return;
  }
  if (CheckTypeThenUseCount(event, event_type_names::kBeforeunload,
                            WebFeature::kDocumentBeforeUnloadFired, document)) {
    if (executing_window != executing_window->top())
      UseCounter::Count(document, WebFeature::kSubFrameBeforeUnloadFired);
    return;
  }
  if (CheckTypeThenUseCount(event, event_type_names::kPointerdown,
                            WebFeature::kPointerDownFired, document)) {
    if (event.IsPointerEvent() &&
        static_cast<const PointerEvent&>(event).pointerType() == "touch") {
      UseCounter::Count(document, WebFeature::kPointerDownFiredForTouch);
    }
    return;
  }

  struct CountedEvent {
    const AtomicString& event_type;
    const WebFeature feature;
  };
  static const CountedEvent counted_events[] = {
      {event_type_names::kUnload, WebFeature::kDocumentUnloadFired},
      {event_type_names::kPagehide, WebFeature::kDocumentPageHideFired},
      {event_type_names::kPageshow, WebFeature::kDocumentPageShowFired},
      {event_type_names::kDOMFocusIn, WebFeature::kDOMFocusInOutEvent},
      {event_type_names::kDOMFocusOut, WebFeature::kDOMFocusInOutEvent},
      {event_type_names::kFocusin, WebFeature::kFocusInOutEvent},
      {event_type_names::kFocusout, WebFeature::kFocusInOutEvent},
      {event_type_names::kTextInput, WebFeature::kTextInputFired},
      {event_type_names::kTouchstart, WebFeature::kTouchStartFired},
      {event_type_names::kMousedown, WebFeature::kMouseDownFired},
      {event_type_names::kPointerenter, WebFeature::kPointerEnterLeaveFired},
      {event_type_names::kPointerleave, WebFeature::kPointerEnterLeaveFired},
      {event_type_names::kPointerover, WebFeature::kPointerOverOutFired},
      {event_type_names::kPointerout, WebFeature::kPointerOverOutFired},
      {event_type_names::kSearch, WebFeature::kSearchEventFired},
  };
  for (const auto& counted_event : counted_events) {
    if (CheckTypeThenUseCount(event, counted_event.event_type,
                              counted_event.feature, document))
      return;
  }

  if (event.eventPhase() == Event::kCapturingPhase ||
      event.eventPhase() == Event::kBubblingPhase) {
    if (CheckTypeThenUseCount(
            event, event_type_names::kDOMNodeRemoved,
            WebFeature::kDOMNodeRemovedEventListenedAtNonTarget, document))
      return;
    if (CheckTypeThenUseCount(
            event, event_type_names::kDOMNodeRemovedFromDocument,
            WebFeature::kDOMNodeRemovedFromDocumentEventListenedAtNonTarget,
            document))
      return;
  }
}

void RegisterWithScheduler(ExecutionContext* execution_context,
                           const AtomicString& event_type) {
  if (!execution_context)
    return;
  // TODO(altimin): Ideally we would also support tracking unregistration of
  // event listeners, but we don't do this for performance reasons.
  base::Optional<SchedulingPolicy::Feature> feature_for_scheduler;
  if (event_type == event_type_names::kPageshow) {
    feature_for_scheduler = SchedulingPolicy::Feature::kPageShowEventListener;
  } else if (event_type == event_type_names::kPagehide) {
    feature_for_scheduler = SchedulingPolicy::Feature::kPageHideEventListener;
  } else if (event_type == event_type_names::kBeforeunload) {
    feature_for_scheduler =
        SchedulingPolicy::Feature::kBeforeUnloadEventListener;
  } else if (event_type == event_type_names::kUnload) {
    feature_for_scheduler = SchedulingPolicy::Feature::kUnloadEventListener;
  } else if (event_type == event_type_names::kFreeze) {
    feature_for_scheduler = SchedulingPolicy::Feature::kFreezeEventListener;
  } else if (event_type == event_type_names::kResume) {
    feature_for_scheduler = SchedulingPolicy::Feature::kResumeEventListener;
  }
  if (feature_for_scheduler) {
    execution_context->GetScheduler()->RegisterStickyFeature(
        feature_for_scheduler.value(),
        {SchedulingPolicy::RecordMetricsForBackForwardCache()});
  }
}

}  // namespace

EventTargetData::EventTargetData() = default;

EventTargetData::~EventTargetData() = default;

void EventTargetData::Trace(Visitor* visitor) {
  visitor->Trace(event_listener_map);
}

EventTarget::EventTarget() = default;

EventTarget::~EventTarget() = default;

Node* EventTarget::ToNode() {
  return nullptr;
}

const DOMWindow* EventTarget::ToDOMWindow() const {
  return nullptr;
}

const LocalDOMWindow* EventTarget::ToLocalDOMWindow() const {
  return nullptr;
}

LocalDOMWindow* EventTarget::ToLocalDOMWindow() {
  return nullptr;
}

MessagePort* EventTarget::ToMessagePort() {
  return nullptr;
}

ServiceWorker* EventTarget::ToServiceWorker() {
  return nullptr;
}

PortalHost* EventTarget::ToPortalHost() {
  return nullptr;
}

// An instance of EventTargetImpl is returned because EventTarget
// is an abstract class, and making it non-abstract is unfavorable
// because it will increase the size of EventTarget and all of its
// subclasses with code that are mostly unnecessary for them,
// resulting in a performance decrease.
// We also don't use ImplementedAs=EventTargetImpl in event_target.idl
// because it will result in some complications with classes that are
// currently derived from EventTarget.
// Spec: https://dom.spec.whatwg.org/#dom-eventtarget-eventtarget
EventTarget* EventTarget::Create(ScriptState* script_state) {
  return MakeGarbageCollected<EventTargetImpl>(script_state);
}

inline LocalDOMWindow* EventTarget::ExecutingWindow() {
  if (ExecutionContext* context = GetExecutionContext())
    return context->ExecutingWindow();
  return nullptr;
}

bool EventTarget::IsTopLevelNode() {
  if (ToLocalDOMWindow())
    return true;

  Node* node = ToNode();
  if (!node)
    return false;

  if (node->IsDocumentNode() || node->GetDocument().documentElement() == node ||
      node->GetDocument().body() == node) {
    return true;
  }

  return false;
}

void EventTarget::SetDefaultAddEventListenerOptions(
    const AtomicString& event_type,
    EventListener* event_listener,
    AddEventListenerOptionsResolved* options) {
  options->SetPassiveSpecified(options->hasPassive());

  if (!IsScrollBlockingEvent(event_type)) {
    if (!options->hasPassive())
      options->setPassive(false);
    return;
  }

  LocalDOMWindow* executing_window = ExecutingWindow();
  if (executing_window) {
    if (options->hasPassive()) {
      UseCounter::Count(executing_window->document(),
                        options->passive()
                            ? WebFeature::kAddEventListenerPassiveTrue
                            : WebFeature::kAddEventListenerPassiveFalse);
    }
  }

  if (RuntimeEnabledFeatures::PassiveDocumentEventListenersEnabled() &&
      IsTouchScrollBlockingEvent(event_type)) {
    if (!options->hasPassive() && IsTopLevelNode()) {
      options->setPassive(true);
      options->SetPassiveForcedForDocumentTarget(true);
      return;
    }
  }

  if (IsWheelScrollBlockingEvent(event_type) && IsTopLevelNode()) {
    if (options->hasPassive()) {
      if (executing_window) {
        UseCounter::Count(
            executing_window->document(),
            options->passive()
                ? WebFeature::kAddDocumentLevelPassiveTrueWheelEventListener
                : WebFeature::kAddDocumentLevelPassiveFalseWheelEventListener);
      }
    } else {  // !options->hasPassive()
      if (executing_window) {
        UseCounter::Count(
            executing_window->document(),
            WebFeature::kAddDocumentLevelPassiveDefaultWheelEventListener);
      }
      if (RuntimeEnabledFeatures::PassiveDocumentWheelEventListenersEnabled()) {
        options->setPassive(true);
        options->SetPassiveForcedForDocumentTarget(true);
        return;
      }
    }
  }

  // For mousewheel event listeners that have the target as the window and
  // a bound function name of "ssc_wheel" treat and no passive value default
  // passive to true. See crbug.com/501568.
  if (event_type == event_type_names::kMousewheel && ToLocalDOMWindow() &&
      event_listener && !options->hasPassive()) {
    JSBasedEventListener* v8_listener =
        DynamicTo<JSBasedEventListener>(event_listener);
    if (!v8_listener)
      return;
    v8::Local<v8::Value> callback_object =
        v8_listener->GetListenerObject(*this);
    if (!callback_object.IsEmpty() && callback_object->IsFunction() &&
        strcmp(
            "ssc_wheel",
            *v8::String::Utf8Value(
                v8::Isolate::GetCurrent(),
                v8::Local<v8::Function>::Cast(callback_object)->GetName())) ==
            0) {
      options->setPassive(true);
      if (executing_window) {
        UseCounter::Count(executing_window->document(),
                          WebFeature::kSmoothScrollJSInterventionActivated);

        executing_window->GetFrame()->Console().AddMessage(
            ConsoleMessage::Create(
                mojom::ConsoleMessageSource::kIntervention,
                mojom::ConsoleMessageLevel::kWarning,
                "Registering mousewheel event as passive due to "
                "smoothscroll.js usage. The smoothscroll.js library is "
                "buggy, no longer necessary and degrades performance. See "
                "https://www.chromestatus.com/feature/5749447073988608"));
      }
      return;
    }
  }

  if (Settings* settings = WindowSettings(ExecutingWindow())) {
    switch (settings->GetPassiveListenerDefault()) {
      case PassiveListenerDefault::kFalse:
        if (!options->hasPassive())
          options->setPassive(false);
        break;
      case PassiveListenerDefault::kTrue:
        if (!options->hasPassive())
          options->setPassive(true);
        break;
      case PassiveListenerDefault::kForceAllTrue:
        options->setPassive(true);
        break;
    }
  } else {
    if (!options->hasPassive())
      options->setPassive(false);
  }

  if (!options->passive() && !options->PassiveSpecified()) {
    String message_text = String::Format(
        "Added non-passive event listener to a scroll-blocking '%s' event. "
        "Consider marking event handler as 'passive' to make the page more "
        "responsive. See "
        "https://www.chromestatus.com/feature/5745543795965952",
        event_type.GetString().Utf8().c_str());

    PerformanceMonitor::ReportGenericViolation(
        GetExecutionContext(), PerformanceMonitor::kDiscouragedAPIUse,
        message_text, base::TimeDelta(), nullptr);
  }
}

bool EventTarget::addEventListener(const AtomicString& event_type,
                                   V8EventListener* listener) {
  EventListener* event_listener = JSEventListener::CreateOrNull(listener);
  return addEventListener(event_type, event_listener);
}

bool EventTarget::addEventListener(
    const AtomicString& event_type,
    V8EventListener* listener,
    const AddEventListenerOptionsOrBoolean& options_union) {
  EventListener* event_listener = JSEventListener::CreateOrNull(listener);

  if (options_union.IsBoolean()) {
    return addEventListener(event_type, event_listener,
                            options_union.GetAsBoolean());
  }

  if (options_union.IsAddEventListenerOptions()) {
    auto* resolved_options =
        MakeGarbageCollected<AddEventListenerOptionsResolved>();
    AddEventListenerOptions* options =
        options_union.GetAsAddEventListenerOptions();
    if (options->hasPassive())
      resolved_options->setPassive(options->passive());
    if (options->hasOnce())
      resolved_options->setOnce(options->once());
    if (options->hasCapture())
      resolved_options->setCapture(options->capture());
    return addEventListener(event_type, event_listener, resolved_options);
  }

  return addEventListener(event_type, event_listener);
}

bool EventTarget::addEventListener(const AtomicString& event_type,
                                   EventListener* listener,
                                   bool use_capture) {
  auto* options = MakeGarbageCollected<AddEventListenerOptionsResolved>();
  options->setCapture(use_capture);
  SetDefaultAddEventListenerOptions(event_type, listener, options);
  return AddEventListenerInternal(event_type, listener, options);
}

bool EventTarget::addEventListener(const AtomicString& event_type,
                                   EventListener* listener,
                                   AddEventListenerOptionsResolved* options) {
  SetDefaultAddEventListenerOptions(event_type, listener, options);
  return AddEventListenerInternal(event_type, listener, options);
}

bool EventTarget::AddEventListenerInternal(
    const AtomicString& event_type,
    EventListener* listener,
    const AddEventListenerOptionsResolved* options) {
  if (!listener)
    return false;

  if (event_type == event_type_names::kTouchcancel ||
      event_type == event_type_names::kTouchend ||
      event_type == event_type_names::kTouchmove ||
      event_type == event_type_names::kTouchstart) {
    if (const LocalDOMWindow* executing_window = ExecutingWindow()) {
      if (const Document* document = executing_window->document()) {
        document->CountUse(options->passive()
                               ? WebFeature::kPassiveTouchEventListener
                               : WebFeature::kNonPassiveTouchEventListener);
      }
    }
  }

  V8DOMActivityLogger* activity_logger =
      V8DOMActivityLogger::CurrentActivityLoggerIfIsolatedWorld();
  if (activity_logger) {
    Vector<String> argv;
    argv.push_back(ToNode() ? ToNode()->nodeName() : InterfaceName());
    argv.push_back(event_type);
    activity_logger->LogEvent("blinkAddEventListener", argv.size(),
                              argv.data());
  }

  RegisteredEventListener registered_listener;
  bool added = EnsureEventTargetData().event_listener_map.Add(
      event_type, listener, options, &registered_listener);
  if (added) {
    AddedEventListener(event_type, registered_listener);
    if (IsA<JSBasedEventListener>(listener) &&
        IsInstrumentedForAsyncStack(event_type)) {
      probe::AsyncTaskScheduled(GetExecutionContext(), event_type,
                                listener->async_task_id());
    }
  }
  return added;
}

void EventTarget::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  if (const LocalDOMWindow* executing_window = ExecutingWindow()) {
    if (Document* document = executing_window->document()) {
      if (event_type == event_type_names::kAuxclick)
        UseCounter::Count(*document, WebFeature::kAuxclickAddListenerCount);
      else if (event_type == event_type_names::kAppinstalled)
        UseCounter::Count(*document, WebFeature::kAppInstalledEventAddListener);
      else if (event_util::IsPointerEventType(event_type))
        UseCounter::Count(*document, WebFeature::kPointerEventAddListenerCount);
      else if (event_type == event_type_names::kSlotchange)
        UseCounter::Count(*document, WebFeature::kSlotChangeEventAddListener);
    }
  }

  RegisterWithScheduler(GetExecutionContext(), event_type);

  if (event_util::IsDOMMutationEventType(event_type)) {
    if (ExecutionContext* context = GetExecutionContext()) {
      String message_text = String::Format(
          "Added synchronous DOM mutation listener to a '%s' event. "
          "Consider using MutationObserver to make the page more responsive.",
          event_type.GetString().Utf8().c_str());
      PerformanceMonitor::ReportGenericViolation(
          context, PerformanceMonitor::kDiscouragedAPIUse, message_text,
          base::TimeDelta(), nullptr);
    }
  }
}

bool EventTarget::removeEventListener(const AtomicString& event_type,
                                      V8EventListener* listener) {
  EventListener* event_listener = JSEventListener::CreateOrNull(listener);
  return removeEventListener(event_type, event_listener);
}

bool EventTarget::removeEventListener(
    const AtomicString& event_type,
    V8EventListener* listener,
    const EventListenerOptionsOrBoolean& options_union) {
  EventListener* event_listener = JSEventListener::CreateOrNull(listener);

  if (options_union.IsBoolean()) {
    return removeEventListener(event_type, event_listener,
                               options_union.GetAsBoolean());
  }

  if (options_union.IsEventListenerOptions()) {
    EventListenerOptions* options = options_union.GetAsEventListenerOptions();
    return removeEventListener(event_type, event_listener, options);
  }

  return removeEventListener(event_type, event_listener);
}

bool EventTarget::removeEventListener(const AtomicString& event_type,
                                      const EventListener* listener,
                                      bool use_capture) {
  EventListenerOptions* options = EventListenerOptions::Create();
  options->setCapture(use_capture);
  return RemoveEventListenerInternal(event_type, listener, options);
}

bool EventTarget::removeEventListener(const AtomicString& event_type,
                                      const EventListener* listener,
                                      EventListenerOptions* options) {
  return RemoveEventListenerInternal(event_type, listener, options);
}

bool EventTarget::RemoveEventListenerInternal(
    const AtomicString& event_type,
    const EventListener* listener,
    const EventListenerOptions* options) {
  if (!listener)
    return false;

  EventTargetData* d = GetEventTargetData();
  if (!d)
    return false;

  wtf_size_t index_of_removed_listener;
  RegisteredEventListener registered_listener;

  if (!d->event_listener_map.Remove(event_type, listener, options,
                                    &index_of_removed_listener,
                                    &registered_listener))
    return false;

  // Notify firing events planning to invoke the listener at 'index' that
  // they have one less listener to invoke.
  if (d->firing_event_iterators) {
    for (const auto& firing_iterator : *d->firing_event_iterators) {
      if (event_type != firing_iterator.event_type)
        continue;

      if (index_of_removed_listener >= firing_iterator.end)
        continue;

      --firing_iterator.end;
      // Note that when firing an event listener,
      // firingIterator.iterator indicates the next event listener
      // that would fire, not the currently firing event
      // listener. See EventTarget::fireEventListeners.
      if (index_of_removed_listener < firing_iterator.iterator)
        --firing_iterator.iterator;
    }
  }
  RemovedEventListener(event_type, registered_listener);
  return true;
}

void EventTarget::RemovedEventListener(
    const AtomicString& event_type,
    const RegisteredEventListener& registered_listener) {}

RegisteredEventListener* EventTarget::GetAttributeRegisteredEventListener(
    const AtomicString& event_type) {
  EventListenerVector* listener_vector = GetEventListeners(event_type);
  if (!listener_vector)
    return nullptr;

  for (auto& event_listener : *listener_vector) {
    EventListener* listener = event_listener.Callback();
    if (listener->IsEventHandler() &&
        listener->BelongsToTheCurrentWorld(GetExecutionContext()))
      return &event_listener;
  }
  return nullptr;
}

bool EventTarget::SetAttributeEventListener(const AtomicString& event_type,
                                            EventListener* listener) {
  RegisteredEventListener* registered_listener =
      GetAttributeRegisteredEventListener(event_type);
  if (!listener) {
    if (registered_listener)
      removeEventListener(event_type, registered_listener->Callback(), false);
    return false;
  }
  if (registered_listener) {
    if (IsA<JSBasedEventListener>(listener) &&
        IsInstrumentedForAsyncStack(event_type)) {
      probe::AsyncTaskScheduled(GetExecutionContext(), event_type,
                                listener->async_task_id());
    }
    registered_listener->SetCallback(listener);
    return true;
  }
  return addEventListener(event_type, listener, false);
}

EventListener* EventTarget::GetAttributeEventListener(
    const AtomicString& event_type) {
  RegisteredEventListener* registered_listener =
      GetAttributeRegisteredEventListener(event_type);
  if (registered_listener)
    return registered_listener->Callback();
  return nullptr;
}

bool EventTarget::dispatchEventForBindings(Event* event,
                                           ExceptionState& exception_state) {
  if (!event->WasInitialized()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The event provided is uninitialized.");
    return false;
  }
  if (event->IsBeingDispatched()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The event is already being dispatched.");
    return false;
  }

  if (!GetExecutionContext())
    return false;

  event->SetTrusted(false);

  // Return whether the event was cancelled or not to JS not that it
  // might have actually been default handled; so check only against
  // CanceledByEventHandler.
  return DispatchEventInternal(*event) !=
         DispatchEventResult::kCanceledByEventHandler;
}

DispatchEventResult EventTarget::DispatchEvent(Event& event) {
  event.SetTrusted(true);
  return DispatchEventInternal(event);
}

DispatchEventResult EventTarget::DispatchEventInternal(Event& event) {
  event.SetTarget(this);
  event.SetCurrentTarget(this);
  event.SetEventPhase(Event::kAtTarget);
  DispatchEventResult dispatch_result = FireEventListeners(event);
  event.SetEventPhase(0);
  return dispatch_result;
}

static const AtomicString& LegacyType(const Event& event) {
  if (event.type() == event_type_names::kTransitionend)
    return event_type_names::kWebkitTransitionEnd;

  if (event.type() == event_type_names::kAnimationstart)
    return event_type_names::kWebkitAnimationStart;

  if (event.type() == event_type_names::kAnimationend)
    return event_type_names::kWebkitAnimationEnd;

  if (event.type() == event_type_names::kAnimationiteration)
    return event_type_names::kWebkitAnimationIteration;

  if (event.type() == event_type_names::kWheel)
    return event_type_names::kMousewheel;

  return g_empty_atom;
}

void EventTarget::CountLegacyEvents(
    const AtomicString& legacy_type_name,
    EventListenerVector* listeners_vector,
    EventListenerVector* legacy_listeners_vector) {
  WebFeature unprefixed_feature;
  WebFeature prefixed_feature;
  WebFeature prefixed_and_unprefixed_feature;
  if (legacy_type_name == event_type_names::kWebkitTransitionEnd) {
    prefixed_feature = WebFeature::kPrefixedTransitionEndEvent;
    unprefixed_feature = WebFeature::kUnprefixedTransitionEndEvent;
    prefixed_and_unprefixed_feature =
        WebFeature::kPrefixedAndUnprefixedTransitionEndEvent;
  } else if (legacy_type_name == event_type_names::kWebkitAnimationEnd) {
    prefixed_feature = WebFeature::kPrefixedAnimationEndEvent;
    unprefixed_feature = WebFeature::kUnprefixedAnimationEndEvent;
    prefixed_and_unprefixed_feature =
        WebFeature::kPrefixedAndUnprefixedAnimationEndEvent;
  } else if (legacy_type_name == event_type_names::kWebkitAnimationStart) {
    prefixed_feature = WebFeature::kPrefixedAnimationStartEvent;
    unprefixed_feature = WebFeature::kUnprefixedAnimationStartEvent;
    prefixed_and_unprefixed_feature =
        WebFeature::kPrefixedAndUnprefixedAnimationStartEvent;
  } else if (legacy_type_name == event_type_names::kWebkitAnimationIteration) {
    prefixed_feature = WebFeature::kPrefixedAnimationIterationEvent;
    unprefixed_feature = WebFeature::kUnprefixedAnimationIterationEvent;
    prefixed_and_unprefixed_feature =
        WebFeature::kPrefixedAndUnprefixedAnimationIterationEvent;
  } else if (legacy_type_name == event_type_names::kMousewheel) {
    prefixed_feature = WebFeature::kMouseWheelEvent;
    unprefixed_feature = WebFeature::kWheelEvent;
    prefixed_and_unprefixed_feature = WebFeature::kMouseWheelAndWheelEvent;
  } else {
    return;
  }

  if (const LocalDOMWindow* executing_window = ExecutingWindow()) {
    if (Document* document = executing_window->document()) {
      if (legacy_listeners_vector) {
        if (listeners_vector)
          UseCounter::Count(*document, prefixed_and_unprefixed_feature);
        else
          UseCounter::Count(*document, prefixed_feature);
      } else if (listeners_vector) {
        UseCounter::Count(*document, unprefixed_feature);
      }
    }
  }
}

DispatchEventResult EventTarget::FireEventListeners(Event& event) {
#if DCHECK_IS_ON()
  DCHECK(!EventDispatchForbiddenScope::IsEventDispatchForbidden());
#endif
  DCHECK(event.WasInitialized());

  EventTargetData* d = GetEventTargetData();
  if (!d)
    return DispatchEventResult::kNotCanceled;

  EventListenerVector* legacy_listeners_vector = nullptr;
  AtomicString legacy_type_name = LegacyType(event);
  if (!legacy_type_name.IsEmpty())
    legacy_listeners_vector = d->event_listener_map.Find(legacy_type_name);

  EventListenerVector* listeners_vector =
      d->event_listener_map.Find(event.type());

  bool fired_event_listeners = false;
  if (listeners_vector) {
    fired_event_listeners = FireEventListeners(event, d, *listeners_vector);
  } else if (event.isTrusted() && legacy_listeners_vector) {
    AtomicString unprefixed_type_name = event.type();
    event.SetType(legacy_type_name);
    fired_event_listeners =
        FireEventListeners(event, d, *legacy_listeners_vector);
    event.SetType(unprefixed_type_name);
  }

  // Only invoke the callback if event listeners were fired for this phase.
  if (fired_event_listeners) {
    event.DoneDispatchingEventAtCurrentTarget();

    // Only count uma metrics if we really fired an event listener.
    Editor::CountEvent(GetExecutionContext(), event);
    CountLegacyEvents(legacy_type_name, listeners_vector,
                      legacy_listeners_vector);
  }
  return GetDispatchEventResult(event);
}

bool EventTarget::FireEventListeners(Event& event,
                                     EventTargetData* d,
                                     EventListenerVector& entry) {
  // Fire all listeners registered for this event. Don't fire listeners removed
  // during event dispatch. Also, don't fire event listeners added during event
  // dispatch. Conveniently, all new event listeners will be added after or at
  // index |size|, so iterating up to (but not including) |size| naturally
  // excludes new event listeners.

  ExecutionContext* context = GetExecutionContext();
  if (!context)
    return false;

  CountFiringEventListeners(event, ExecutingWindow());

  wtf_size_t i = 0;
  wtf_size_t size = entry.size();
  if (!d->firing_event_iterators)
    d->firing_event_iterators = std::make_unique<FiringEventIteratorVector>();
  d->firing_event_iterators->push_back(
      FiringEventIterator(event.type(), i, size));

  base::TimeDelta blocked_event_threshold =
      BlockedEventsWarningThreshold(context, event);
  base::TimeTicks now;
  bool should_report_blocked_event = false;
  if (!blocked_event_threshold.is_zero()) {
    now = base::TimeTicks::Now();
    should_report_blocked_event =
        now - event.PlatformTimeStamp() > blocked_event_threshold;
  }
  bool fired_listener = false;

  while (i < size) {
    RegisteredEventListener registered_listener = entry[i];

    // Move the iterator past this event listener. This must match
    // the handling of the FiringEventIterator::iterator in
    // EventTarget::removeEventListener.
    ++i;

    if (!registered_listener.ShouldFire(event))
      continue;

    EventListener* listener = registered_listener.Callback();
    // The listener will be retained by Member<EventListener> in the
    // registeredListener, i and size are updated with the firing event iterator
    // in case the listener is removed from the listener vector below.
    if (registered_listener.Once())
      removeEventListener(event.type(), listener,
                          registered_listener.Capture());

    // If stopImmediatePropagation has been called, we just break out
    // immediately, without handling any more events on this target.
    if (event.ImmediatePropagationStopped())
      break;

    event.SetHandlingPassive(EventPassiveMode(registered_listener));

    probe::UserCallback probe(context, nullptr, event.type(), false, this);
    probe::AsyncTask async_task(context, listener->async_task_id(), "event",
                                IsInstrumentedForAsyncStack(event.type()));

    // To match Mozilla, the AT_TARGET phase fires both capturing and bubbling
    // event listeners, even though that violates some versions of the DOM spec.
    listener->Invoke(context, &event);
    fired_listener = true;

    // If we're about to report this event listener as blocking, make sure it
    // wasn't removed while handling the event.
    if (should_report_blocked_event && i > 0 &&
        entry[i - 1].Callback() == listener && !entry[i - 1].Passive() &&
        !entry[i - 1].BlockedEventWarningEmitted() &&
        !event.defaultPrevented()) {
      ReportBlockedEvent(*this, event, &entry[i - 1],
                         now - event.PlatformTimeStamp());
    }

    event.SetHandlingPassive(Event::PassiveMode::kNotPassive);

    CHECK_LE(i, size);
  }
  d->firing_event_iterators->pop_back();
  return fired_listener;
}

DispatchEventResult EventTarget::GetDispatchEventResult(const Event& event) {
  if (event.defaultPrevented())
    return DispatchEventResult::kCanceledByEventHandler;
  if (event.DefaultHandled())
    return DispatchEventResult::kCanceledByDefaultEventHandler;
  return DispatchEventResult::kNotCanceled;
}

EventListenerVector* EventTarget::GetEventListeners(
    const AtomicString& event_type) {
  EventTargetData* data = GetEventTargetData();
  if (!data)
    return nullptr;
  return data->event_listener_map.Find(event_type);
}

Vector<AtomicString> EventTarget::EventTypes() {
  EventTargetData* d = GetEventTargetData();
  return d ? d->event_listener_map.EventTypes() : Vector<AtomicString>();
}

void EventTarget::RemoveAllEventListeners() {
  EventTargetData* d = GetEventTargetData();
  if (!d)
    return;
  d->event_listener_map.Clear();

  // Notify firing events planning to invoke the listener at 'index' that
  // they have one less listener to invoke.
  if (d->firing_event_iterators) {
    for (const auto& iterator : *d->firing_event_iterators) {
      iterator.iterator = 0;
      iterator.end = 0;
    }
  }
}

void EventTarget::EnqueueEvent(Event& event, TaskType task_type) {
  ExecutionContext* context = GetExecutionContext();
  if (!context)
    return;
  probe::AsyncTaskScheduled(context, event.type(), event.async_task_id());
  context->GetTaskRunner(task_type)->PostTask(
      FROM_HERE,
      WTF::Bind(&EventTarget::DispatchEnqueuedEvent, WrapPersistent(this),
                WrapPersistent(&event), WrapPersistent(context)));
}

void EventTarget::DispatchEnqueuedEvent(Event* event,
                                        ExecutionContext* context) {
  if (!GetExecutionContext()) {
    probe::AsyncTaskCanceled(context, event->async_task_id());
    return;
  }
  probe::AsyncTask async_task(context, event->async_task_id());
  DispatchEvent(*event);
}

STATIC_ASSERT_ENUM(WebSettings::PassiveEventListenerDefault::kFalse,
                   PassiveListenerDefault::kFalse);
STATIC_ASSERT_ENUM(WebSettings::PassiveEventListenerDefault::kTrue,
                   PassiveListenerDefault::kTrue);
STATIC_ASSERT_ENUM(WebSettings::PassiveEventListenerDefault::kForceAllTrue,
                   PassiveListenerDefault::kForceAllTrue);

}  // namespace blink
