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
#include "base/time/time.h"
#include "third_party/blink/renderer/bindings/core/v8/js_based_event_listener.h"
#include "third_party/blink/renderer/bindings/core/v8/js_event_listener.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_observable_event_listener_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_addeventlisteneroptions_boolean.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_boolean_eventlisteneroptions.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/abort_signal_registry.h"
#include "third_party/blink/renderer/core/dom/events/add_event_listener_options_resolved.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatch_forbidden_scope.h"
#include "third_party/blink/renderer/core/dom/events/event_target_impl.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/dom/observable.h"
#include "third_party/blink/renderer/core/dom/subscriber.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/events/event_util.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/performance_monitor.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/pointer_type_names.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_activity_logger.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
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
    if (IsA<PointerEvent>(event) &&
        static_cast<const PointerEvent&>(event).pointerType() ==
            pointer_type_names::kTouch) {
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
}

// See documentation for `ObservableSubscribeDelegate` below.
class ObservableEventListener final : public NativeEventListener {
 public:
  ObservableEventListener(Subscriber*,
                          ScriptState*,
                          const AtomicString&,
                          EventTarget*,
                          const ObservableEventListenerOptions*);

  // NativeEventListener overrides:
  void Invoke(ExecutionContext*, Event*) final;

  void Trace(Visitor*) const override;

 private:
  // The `Subscriber` that `this` forwards events to when `Invoke()` is called.
  const Member<Subscriber> subscriber_;
  // This is the `ScriptState` associated with the subscription. We store it
  // here so that asynchronously later, when `Invoke()` is called (i.e., when
  // the `EventTarget` that `this` is listening to starts firing events), we can
  // transform the events into `ScriptValue` objects (which requires a
  // `ScriptState`) and forward them to `subscriber_` above.
  const Member<ScriptState> script_state_;
};

ObservableEventListener::ObservableEventListener(
    Subscriber* subscriber,
    ScriptState* script_state,
    const AtomicString& event_type,
    EventTarget* event_target,
    const ObservableEventListenerOptions* options)
    : subscriber_(subscriber), script_state_(script_state) {
  // `event_target_` is non-null here. If the event target were null (i.e.,
  // garbage collected before this gets called), then this constructor would not
  // be invoked with it.
  CHECK(event_target);

  // `this` only gets constructed if `subscriber_` is active. If the subscriber
  // becomes inactive immediately upon subscription (i.e., an already-aborted
  // signal is passed into `Observable::subscribe()`, then `this` does not even
  // get constructed, since we must not add an event listener to `event_target`
  // in that case.
  CHECK(subscriber_->active());

  AddEventListenerOptionsResolved* options_resolved =
      MakeGarbageCollected<AddEventListenerOptionsResolved>();
  if (options->hasCapture()) {
    options_resolved->setCapture(options->capture());
  }
  if (options->hasPassive()) {
    options_resolved->setPassive(options->passive());
  }
  options_resolved->setSignal(subscriber->signal());

  event_target->addEventListener(event_type, this, options_resolved);
}

void ObservableEventListener::Invoke(ExecutionContext* execution_context,
                                     Event* event) {
  // The `script_state_` will always be valid here, because
  // `EventTarget::FireEventListeners()` early-returns if its `ExecutionContext`
  // is detached.
  DCHECK(script_state_->ContextIsValid());
  ScriptState::Scope scope(script_state_);
  ScriptValue script_value = ScriptValue::From(script_state_, event);

  subscriber_->next(script_value);
}

void ObservableEventListener::Trace(Visitor* visitor) const {
  visitor->Trace(subscriber_);
  visitor->Trace(script_state_);

  NativeEventListener::Trace(visitor);
}

// This is the synthetic subscribe callback that we construct `Observable`s with
// that are created by `EventTarget#when()`. `OnSubscribe()` adds a brand new
// `ObservableEventListener` as a new event listener for events named
// `event_type_`. When events are received, they are propagated directly to
// `Subscriber`.
class ObservableSubscribeDelegate final : public Observable::SubscribeDelegate {
 public:
  ObservableSubscribeDelegate(EventTarget*,
                              const AtomicString&,
                              const ObservableEventListenerOptions*);

  // Observable::SubscribeDelegate overrides:
  void OnSubscribe(Subscriber*, ScriptState*) final;

  void Trace(Visitor*) const override;

 private:
  // This is the event target for which we will vend per-subscriber event
  // listeners. The typical flow here looks like this:
  //   1.) `EventTarget::when()` is called, returning an observable whose
  //       subscribe callback is `this` (instead of a JS-provided v8
  //       callback).
  //   2.) `Observable::subscribe()` is called by JS, and thus `OnSubscribe()`
  //       is invoked on `this`.
  //   3.) `OnSubscribe()` creates a new `ObservableEventListener` just for
  //       the new subscriber, listening for events named `event_type_` from
  //       `event_target_`.
  //   4.) The `ObservableEventListener` keeps a pointer to the subscriber,
  //       and when events are dispatched to the listener, they are forwarded
  //       to `Subscriber::next()`.
  const WeakMember<EventTarget> event_target_;
  AtomicString event_type_;
  const Member<const ObservableEventListenerOptions> options_;
};

ObservableSubscribeDelegate::ObservableSubscribeDelegate(
    EventTarget* event_target,
    const AtomicString& event_type,
    const ObservableEventListenerOptions* options)
    : event_target_(event_target), event_type_(event_type), options_(options) {}

void ObservableSubscribeDelegate::OnSubscribe(Subscriber* subscriber,
                                              ScriptState* script_state) {
  // This should have already been checked by `Observable::subscribe()` before
  // getting here.
  CHECK(script_state->ContextIsValid());

  // If the subscriber is already aborted, early return because there is no use
  // in adding the event listener, since it will never be able to removed again.
  // It is possible for the subscriber to be aborted at this point if
  // `Observable#subscribe()` is called with an already-aborted signal in
  // `SubscribeOptions`.
  //
  // TODO(crbug.com/1485981): Once we agree on proper spec text for this, quote
  // it here.
  if (subscriber->signal()->aborted()) {
    return;
  }

  // The weak `event_target_` could be null at this point, if the target has
  // been garbage collected by the time `this`'s associated Observable has been
  // subscribed to. We early return in this case, as to avoid setting up the
  // entire event listener / abort signal mechanism.
  if (!event_target_) {
    return;
  }

  // This freshly-created event listener immediately gets owned by
  // `event_target_`'s event listener vector. `this` does not need to hold onto
  // any of the event listeners created here.
  MakeGarbageCollected<ObservableEventListener>(
      subscriber, script_state, event_type_, event_target_, options_);
}

void ObservableSubscribeDelegate::Trace(Visitor* visitor) const {
  visitor->Trace(event_target_);
  visitor->Trace(options_);

  Observable::SubscribeDelegate::Trace(visitor);
}

}  // namespace

EventTargetData::EventTargetData() = default;

EventTargetData::~EventTargetData() = default;

void EventTargetData::Trace(Visitor* visitor) const {
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

void EventTarget::ResetEventQueueStatus(const AtomicString& event_type) {}

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
  return DynamicTo<LocalDOMWindow>(GetExecutionContext());
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

  if (IsTouchScrollBlockingEvent(event_type)) {
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
      options->setPassive(true);
      options->SetPassiveForcedForDocumentTarget(true);
      return;
    }
  }

  if (!options->hasPassive())
    options->setPassive(false);

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

Observable* EventTarget::when(const AtomicString& event_type,
                              const ObservableEventListenerOptions* options) {
  DCHECK(RuntimeEnabledFeatures::ObservableAPIEnabled());
  return MakeGarbageCollected<Observable>(
      GetExecutionContext(), MakeGarbageCollected<ObservableSubscribeDelegate>(
                                 this, event_type, options));
}

bool EventTarget::addEventListener(const AtomicString& event_type,
                                   V8EventListener* listener) {
  EventListener* event_listener = JSEventListener::CreateOrNull(listener);
  return addEventListener(event_type, event_listener);
}

bool EventTarget::addEventListener(
    const AtomicString& event_type,
    V8EventListener* listener,
    const V8UnionAddEventListenerOptionsOrBoolean* bool_or_options) {
  DCHECK(bool_or_options);

  EventListener* event_listener = JSEventListener::CreateOrNull(listener);

  switch (bool_or_options->GetContentType()) {
    case V8UnionAddEventListenerOptionsOrBoolean::ContentType::kBoolean:
      return addEventListener(event_type, event_listener,
                              bool_or_options->GetAsBoolean());
    case V8UnionAddEventListenerOptionsOrBoolean::ContentType::
        kAddEventListenerOptions: {
      auto* options_resolved =
          MakeGarbageCollected<AddEventListenerOptionsResolved>();
      AddEventListenerOptions* options =
          bool_or_options->GetAsAddEventListenerOptions();
      if (options->hasPassive())
        options_resolved->setPassive(options->passive());
      if (options->hasOnce())
        options_resolved->setOnce(options->once());
      if (options->hasCapture())
        options_resolved->setCapture(options->capture());
      if (options->hasSignal())
        options_resolved->setSignal(options->signal());
      return addEventListener(event_type, event_listener, options_resolved);
    }
  }

  NOTREACHED_IN_MIGRATION();
  return false;
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

  if (options->hasSignal() && options->signal()->aborted())
    return false;

  // It doesn't make sense to add an event listener without an ExecutionContext
  // and some code below here assumes we have one.
  auto* execution_context = GetExecutionContext();
  if (!execution_context)
    return false;

  // Unload/Beforeunload handlers are not allowed in fenced frames.
  if (event_type == event_type_names::kUnload ||
      event_type == event_type_names::kBeforeunload) {
    if (const LocalDOMWindow* window = ExecutingWindow()) {
      if (const LocalFrame* frame = window->GetFrame()) {
        if (frame->IsInFencedFrameTree()) {
          window->PrintErrorMessage(
              "unload/beforeunload handlers are prohibited in fenced frames.");
          return false;
        }
      }
    }
  }

  // Consider `Permissions-Policy: unload` unless the deprecation trial is in
  // effect.
  if (event_type == event_type_names::kUnload &&
      !RuntimeEnabledFeatures::DeprecateUnloadOptOutEnabled(
          execution_context) &&
      !execution_context->IsFeatureEnabled(
          mojom::blink::PermissionsPolicyFeature::kUnload,
          ReportOptions::kReportOnFailure)) {
    return false;
  }

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
      V8DOMActivityLogger::CurrentActivityLoggerIfIsolatedWorld(
          execution_context->GetIsolate());
  if (activity_logger) {
    Vector<String> argv;
    argv.push_back(ToNode() ? ToNode()->nodeName() : InterfaceName());
    argv.push_back(event_type);
    activity_logger->LogEvent(execution_context, "blinkAddEventListener", argv);
  }

  RegisteredEventListener* registered_listener = nullptr;
  bool added = EnsureEventTargetData().event_listener_map.Add(
      event_type, listener, options, &registered_listener);
  if (added) {
    CHECK(registered_listener);
    if (options->hasSignal()) {
      // Instead of passing the entire |options| here, which could create a
      // circular reference due to |options| holding a Member<AbortSignal>, just
      // pass the |options->capture()| boolean, which is the only thing
      // removeEventListener actually uses to find and remove the event
      // listener.
      AbortSignal::AlgorithmHandle* handle =
          options->signal()->AddAlgorithm(WTF::BindOnce(
              [](EventTarget* event_target, const AtomicString& event_type,
                 const EventListener* listener, bool capture) {
                if (event_target) {
                  event_target->removeEventListener(event_type, listener,
                                                    capture);
                }
              },
              WrapWeakPersistent(this), event_type,
              WrapWeakPersistent(listener), options->capture()));
      AbortSignalRegistry::From(*execution_context)
          ->RegisterAbortAlgorithm(listener, handle);
      if (const LocalDOMWindow* executing_window = ExecutingWindow()) {
        if (const Document* document = executing_window->document()) {
          document->CountUse(WebFeature::kAddEventListenerWithAbortSignal);
        }
      }
    }

    AddedEventListener(event_type, *registered_listener);
    if (IsA<JSBasedEventListener>(listener) &&
        IsInstrumentedForAsyncStack(event_type)) {
      listener->async_task_context()->Schedule(GetExecutionContext(),
                                               event_type);
    }
  }
  return added;
}

void EventTarget::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  const LocalDOMWindow* executing_window = ExecutingWindow();
  Document* document =
      executing_window ? executing_window->document() : nullptr;
  if (document) {
    if (event_type == event_type_names::kAuxclick) {
      UseCounter::Count(*document, WebFeature::kAuxclickAddListenerCount);
    } else if (event_type == event_type_names::kAppinstalled) {
      UseCounter::Count(*document, WebFeature::kAppInstalledEventAddListener);
    } else if (event_util::IsPointerEventType(event_type)) {
      UseCounter::Count(*document, WebFeature::kPointerEventAddListenerCount);
    } else if (event_type == event_type_names::kSlotchange) {
      UseCounter::Count(*document, WebFeature::kSlotChangeEventAddListener);
    } else if (event_type == event_type_names::kBeforematch) {
      UseCounter::Count(*document, WebFeature::kBeforematchHandlerRegistered);
    } else if (event_type ==
               event_type_names::kContentvisibilityautostatechange) {
      UseCounter::Count(
          *document,
          WebFeature::kContentVisibilityAutoStateChangeHandlerRegistered);
    } else if (event_type == event_type_names::kScrollend) {
      UseCounter::Count(*document, WebFeature::kScrollend);
    } else if (event_util::IsSnapEventType(event_type)) {
      UseCounter::Count(*document, WebFeature::kSnapEvent);
    }
  }

  auto info = event_util::IsDOMMutationEventType(event_type);
  if (info.is_mutation_event) {
    if (ExecutionContext* context = GetExecutionContext()) {
      if (RuntimeEnabledFeatures::MutationEventsEnabled(context) &&
          (!document || document->SupportsLegacyDOMMutations())) {
        String message_text = String::Format(
            "Listener added for a '%s' mutation event. This event type is "
            "deprecated, and will be removed from this browser VERY soon. "
            "Usage of this event listener will cause performance issues today, "
            "and represents a large risk of imminent site breakage. Consider "
            "using MutationObserver instead. See "
            "https://chromestatus.com/feature/5083947249172480 for more "
            "information.",
            event_type.GetString().Utf8().c_str());
        PerformanceMonitor::ReportGenericViolation(
            context, PerformanceMonitor::kDiscouragedAPIUse, message_text,
            base::TimeDelta(), nullptr);
        context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kDeprecation,
            mojom::blink::ConsoleMessageLevel::kWarning, message_text));
        Deprecation::CountDeprecation(context, info.listener_feature);
        UseCounter::Count(context, WebFeature::kAnyMutationEventListenerAdded);
      } else {
        String message_text;
        // Only show the special trial message if mutation events are disabled
        // via the feature flag, and not via lack of embedder support.
        if (!RuntimeEnabledFeatures::MutationEventsEnabled(context) &&
            RuntimeEnabledFeatures::MutationEventsSpecialTrialMessageEnabled(
                context)) {
          message_text = String::Format(
              "Usage of mutation events (%s) was detected. This event type has "
              "been deprecated, and an early trial-run of complete removal is "
              "underway. In this browser, mutation events are currently not "
              "being fired. If you are a *user* experiencing a problem, please "
              "report the issue to the operator of the website. If you are a "
              "site owner, and you think this trial is causing an unexpected "
              "issue, please report a bug at "
              "https://issues.chromium.org/issues/"
              "new?component=1456718&template=1948649. Note that these events "
              "will stop being fired for ALL USERS starting in version 127, "
              "which is the next release.",
              event_type.GetString().Utf8().c_str());
        } else {
          message_text = String::Format(
              "Listener added for a '%s' mutation event. Support for this "
              "event type has been removed, and this event will no longer be "
              "fired. See https://chromestatus.com/feature/5083947249172480 "
              "for more information.",
              event_type.GetString().Utf8().c_str());
        }
        context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kDeprecation,
            mojom::blink::ConsoleMessageLevel::kError, message_text));
      }
    }
  }
}

bool EventTarget::removeEventListener(const AtomicString& event_type,
                                      V8EventListener* listener) {
  EventListener* event_listener = JSEventListener::CreateOrNull(listener);
  return removeEventListener(event_type, event_listener, /*use_capture=*/false);
}

bool EventTarget::removeEventListener(
    const AtomicString& event_type,
    V8EventListener* listener,
    const V8UnionBooleanOrEventListenerOptions* bool_or_options) {
  DCHECK(bool_or_options);

  EventListener* event_listener = JSEventListener::CreateOrNull(listener);

  switch (bool_or_options->GetContentType()) {
    case V8UnionBooleanOrEventListenerOptions::ContentType::kBoolean:
      return removeEventListener(event_type, event_listener,
                                 bool_or_options->GetAsBoolean());
    case V8UnionBooleanOrEventListenerOptions::ContentType::
        kEventListenerOptions: {
      EventListenerOptions* options =
          bool_or_options->GetAsEventListenerOptions();
      return removeEventListener(event_type, event_listener, options);
    }
  }

  NOTREACHED_IN_MIGRATION();
  return false;
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

  RegisteredEventListener* registered_listener;

  if (!d->event_listener_map.Remove(event_type, listener, options,
                                    &registered_listener)) {
    return false;
  }

  CHECK(registered_listener);
  RemovedEventListener(event_type, *registered_listener);
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

  for (auto& registered_listener : *listener_vector) {
    EventListener* listener = registered_listener->Callback();
    if (GetExecutionContext() && listener->IsEventHandler() &&
        listener->BelongsToTheCurrentWorld(GetExecutionContext()))
      return registered_listener.Get();
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
      listener->async_task_context()->Schedule(GetExecutionContext(),
                                               event_type);
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
  if (!GetExecutionContext())
    return DispatchEventResult::kCanceledBeforeDispatch;
  event.SetTrusted(true);
  return DispatchEventInternal(event);
}

DispatchEventResult EventTarget::DispatchEventInternal(Event& event) {
  event.SetTarget(this);
  event.SetCurrentTarget(this);
  event.SetEventPhase(Event::PhaseType::kAtTarget);
  DispatchEventResult dispatch_result = FireEventListeners(event);
  event.SetEventPhase(Event::PhaseType::kNone);
  return dispatch_result;
}

EventTargetData* EventTarget::GetEventTargetData() {
  return data_.Get();
}

EventTargetData& EventTarget::EnsureEventTargetData() {
  if (!data_) {
    data_ = MakeGarbageCollected<EventTargetData>();
  }
  return *data_;
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
  if (!legacy_type_name.empty())
    legacy_listeners_vector = d->event_listener_map.Find(legacy_type_name);

  EventListenerVector* listeners_vector =
      d->event_listener_map.Find(event.type());

  bool fired_event_listeners = false;
  if (listeners_vector) {
    // Calling `FireEventListener` causes a clone of `listeners_vector`.
    fired_event_listeners = FireEventListeners(event, d, *listeners_vector);
  } else if (event.isTrusted() && legacy_listeners_vector) {
    AtomicString unprefixed_type_name = event.type();
    event.SetType(legacy_type_name);
    // Calling `FireEventListener` causes a clone of `legacy_listeners_vector`.
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

// Fire event listeners, creates a copy of EventListenerVector on being called.
bool EventTarget::FireEventListeners(Event& event,
                                     EventTargetData* d,
                                     EventListenerVector entry) {
  // Fire all listeners registered for this event. Don't fire listeners removed
  // during event dispatch. Also, don't fire event listeners added during event
  // dispatch. Conveniently, all new event listeners will be added after or at
  // index |size|, so iterating up to (but not including) |size| naturally
  // excludes new event listeners.

  ExecutionContext* context = GetExecutionContext();
  if (!context)
    return false;

  CountFiringEventListeners(event, ExecutingWindow());

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

  for (auto& registered_listener : entry) {
    if (registered_listener->Removed()) [[unlikely]] {
      continue;
    }

    // If stopImmediatePropagation has been called, we just break out
    // immediately, without handling any more events on this target.
    if (event.ImmediatePropagationStopped()) {
      break;
    }

    if (!registered_listener->ShouldFire(event)) {
      continue;
    }

    EventListener* listener = registered_listener->Callback();
    // The listener will be retained by Member<EventListener> in the
    // registeredListener, i and size are updated with the firing event iterator
    // in case the listener is removed from the listener vector below.
    if (registered_listener->Once()) {
      removeEventListener(event.type(), listener,
                          registered_listener->Capture());
    }
    event.SetHandlingPassive(EventPassiveMode(*registered_listener));

    probe::UserCallback probe(context, nullptr, event.type(), false, this);
    probe::AsyncTask async_task(context, listener->async_task_context(),
                                "event",
                                IsInstrumentedForAsyncStack(event.type()));

    // To match Mozilla, the AT_TARGET phase fires both capturing and bubbling
    // event listeners, even though that violates some versions of the DOM spec.
    listener->Invoke(context, &event);
    fired_listener = true;

    // If we're about to report this event listener as blocking, make sure it
    // wasn't removed while handling the event.
    if (should_report_blocked_event && !registered_listener->Removed() &&
        !registered_listener->Passive() &&
        !registered_listener->BlockedEventWarningEmitted() &&
        !event.defaultPrevented()) {
      ReportBlockedEvent(*this, event, registered_listener,
                         now - event.PlatformTimeStamp());
    }

    event.SetHandlingPassive(Event::PassiveMode::kNotPassive);
  }
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

int EventTarget::NumberOfEventListeners(const AtomicString& event_type) const {
  EventListenerVector* listeners =
      const_cast<EventTarget*>(this)->GetEventListeners(event_type);
  return listeners ? listeners->size() : 0;
}

Vector<AtomicString> EventTarget::EventTypes() {
  EventTargetData* d = GetEventTargetData();
  return d ? d->event_listener_map.EventTypes() : Vector<AtomicString>();
}

void EventTarget::RemoveAllEventListeners() {
  if (auto* d = GetEventTargetData()) {
    d->event_listener_map.Clear();
  }
}

void EventTarget::EnqueueEvent(Event& event, TaskType task_type) {
  ExecutionContext* context = GetExecutionContext();
  if (!context)
    return;
  event.async_task_context()->Schedule(context, event.type());
  context->GetTaskRunner(task_type)->PostTask(
      FROM_HERE,
      WTF::BindOnce(&EventTarget::DispatchEnqueuedEvent, WrapPersistent(this),
                    WrapPersistent(&event), WrapPersistent(context)));
}

void EventTarget::DispatchEnqueuedEvent(Event* event,
                                        ExecutionContext* context) {
  if (!GetExecutionContext()) {
    event->async_task_context()->Cancel();
    return;
  }
  this->ResetEventQueueStatus(event->type());
  probe::AsyncTask async_task(context, event->async_task_context());
  DispatchEvent(*event);
}

void EventTarget::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(data_);
}

}  // namespace blink
