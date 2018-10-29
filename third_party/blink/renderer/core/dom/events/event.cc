/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/dom/events/event.h"

#include "third_party/blink/renderer/core/dom/events/event_dispatcher.h"
#include "third_party/blink/renderer/core/dom/events/event_path.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/dom/events/window_event_context.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/events/focus_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/hosts_using_features.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

static bool IsEventTypeScopedInV0(const AtomicString& event_type) {
  // WebKit never allowed selectstart event to cross the the shadow DOM
  // boundary.  Changing this breaks existing sites.
  // See https://bugs.webkit.org/show_bug.cgi?id=52195 for details.
  return event_type == EventTypeNames::abort ||
         event_type == EventTypeNames::change ||
         event_type == EventTypeNames::error ||
         event_type == EventTypeNames::load ||
         event_type == EventTypeNames::reset ||
         event_type == EventTypeNames::resize ||
         event_type == EventTypeNames::scroll ||
         event_type == EventTypeNames::select ||
         event_type == EventTypeNames::selectstart ||
         event_type == EventTypeNames::slotchange;
}

Event::Event() : Event("", Bubbles::kNo, Cancelable::kNo) {
  was_initialized_ = false;
}

Event::Event(const AtomicString& event_type,
             Bubbles bubbles,
             Cancelable cancelable,
             TimeTicks platform_time_stamp)
    : Event(event_type,
            bubbles,
            cancelable,
            ComposedMode::kScoped,
            platform_time_stamp) {}

Event::Event(const AtomicString& event_type,
             Bubbles bubbles,
             Cancelable cancelable,
             ComposedMode composed_mode)
    : Event(event_type,
            bubbles,
            cancelable,
            composed_mode,
            CurrentTimeTicks()) {}

Event::Event(const AtomicString& event_type,
             Bubbles bubbles,
             Cancelable cancelable,
             ComposedMode composed_mode,
             TimeTicks platform_time_stamp)
    : type_(event_type),
      bubbles_(bubbles == Bubbles::kYes),
      cancelable_(cancelable == Cancelable::kYes),
      composed_(composed_mode == ComposedMode::kComposed),
      is_event_type_scoped_in_v0_(IsEventTypeScopedInV0(event_type)),
      propagation_stopped_(false),
      immediate_propagation_stopped_(false),
      default_prevented_(false),
      default_handled_(false),
      was_initialized_(true),
      is_trusted_(false),
      executed_listener_or_default_action_(false),
      prevent_default_called_on_uncancelable_event_(false),
      legacy_did_listeners_throw_flag_(false),
      fire_only_capture_listeners_at_target_(false),
      fire_only_non_capture_listeners_at_target_(false),
      handling_passive_(PassiveMode::kNotPassiveDefault),
      event_phase_(0),
      current_target_(nullptr),
      platform_time_stamp_(platform_time_stamp) {}

Event::Event(const AtomicString& event_type,
             const EventInit& initializer,
             TimeTicks platform_time_stamp)
    : Event(event_type,
            initializer.bubbles() ? Bubbles::kYes : Bubbles::kNo,
            initializer.cancelable() ? Cancelable::kYes : Cancelable::kNo,
            initializer.composed() ? ComposedMode::kComposed
                                   : ComposedMode::kScoped,
            platform_time_stamp) {}

Event::~Event() = default;

bool Event::IsScopedInV0() const {
  return isTrusted() && is_event_type_scoped_in_v0_;
}

void Event::initEvent(const AtomicString& event_type_arg,
                      bool bubbles_arg,
                      bool cancelable_arg) {
  initEvent(event_type_arg, bubbles_arg, cancelable_arg, nullptr);
}

void Event::initEvent(const AtomicString& event_type_arg,
                      bool bubbles_arg,
                      bool cancelable_arg,
                      EventTarget* related_target) {
  if (IsBeingDispatched())
    return;

  was_initialized_ = true;
  propagation_stopped_ = false;
  immediate_propagation_stopped_ = false;
  default_prevented_ = false;
  is_trusted_ = false;
  prevent_default_called_on_uncancelable_event_ = false;

  type_ = event_type_arg;
  bubbles_ = bubbles_arg;
  cancelable_ = cancelable_arg;
}

bool Event::legacyReturnValue(ScriptState* script_state) const {
  bool return_value = !defaultPrevented();
  if (return_value) {
    UseCounter::Count(ExecutionContext::From(script_state),
                      WebFeature::kEventGetReturnValueTrue);
  } else {
    UseCounter::Count(ExecutionContext::From(script_state),
                      WebFeature::kEventGetReturnValueFalse);
  }
  return return_value;
}

void Event::setLegacyReturnValue(ScriptState* script_state, bool return_value) {
  if (return_value) {
    UseCounter::Count(ExecutionContext::From(script_state),
                      WebFeature::kEventSetReturnValueTrue);
  } else {
    UseCounter::Count(ExecutionContext::From(script_state),
                      WebFeature::kEventSetReturnValueFalse);
  }
  SetDefaultPrevented(!return_value);
}

const AtomicString& Event::InterfaceName() const {
  return EventNames::Event;
}

bool Event::HasInterface(const AtomicString& name) const {
  return InterfaceName() == name;
}

bool Event::IsUIEvent() const {
  return false;
}

bool Event::IsMouseEvent() const {
  return false;
}

bool Event::IsFocusEvent() const {
  return false;
}

bool Event::IsKeyboardEvent() const {
  return false;
}

bool Event::IsTouchEvent() const {
  return false;
}

bool Event::IsGestureEvent() const {
  return false;
}

bool Event::IsWheelEvent() const {
  return false;
}

bool Event::IsPointerEvent() const {
  return false;
}

bool Event::IsInputEvent() const {
  return false;
}

bool Event::IsDragEvent() const {
  return false;
}

bool Event::IsCompositionEvent() const {
  return false;
}

bool Event::IsActivateInvisibleEvent() const {
  return false;
}

bool Event::IsClipboardEvent() const {
  return false;
}

bool Event::IsBeforeTextInsertedEvent() const {
  return false;
}

bool Event::IsBeforeUnloadEvent() const {
  return false;
}

bool Event::IsErrorEvent() const {
  return false;
}

void Event::preventDefault() {
  if (handling_passive_ != PassiveMode::kNotPassive &&
      handling_passive_ != PassiveMode::kNotPassiveDefault) {
    prevent_default_called_during_passive_ = true;

    const LocalDOMWindow* window =
        event_path_ ? event_path_->GetWindowEventContext().Window() : nullptr;
    if (window && handling_passive_ == PassiveMode::kPassive) {
      window->PrintErrorMessage(
          "Unable to preventDefault inside passive event listener invocation.");
    }
    return;
  }

  if (cancelable_)
    default_prevented_ = true;
  else
    prevent_default_called_on_uncancelable_event_ = true;
}

void Event::SetTarget(EventTarget* target) {
  if (target_ == target)
    return;

  target_ = target;
  if (target_)
    ReceivedTarget();
}

void Event::DoneDispatchingEventAtCurrentTarget() {
  SetExecutedListenerOrDefaultAction();
}

void Event::SetRelatedTargetIfExists(EventTarget* related_target) {
  if (IsMouseEvent()) {
    ToMouseEvent(this)->SetRelatedTarget(related_target);
  } else if (IsPointerEvent()) {
    ToPointerEvent(this)->SetRelatedTarget(related_target);
  } else if (IsFocusEvent()) {
    ToFocusEvent(this)->SetRelatedTarget(related_target);
  }
}

void Event::ReceivedTarget() {}

void Event::SetUnderlyingEvent(Event* ue) {
  // Prohibit creation of a cycle -- just do nothing in that case.
  for (Event* e = ue; e; e = e->UnderlyingEvent())
    if (e == this)
      return;
  underlying_event_ = ue;
}

void Event::InitEventPath(Node& node) {
  if (!event_path_) {
    event_path_ = new EventPath(node, this);
  } else {
    event_path_->InitializeWith(node, this);
  }
}

ScriptValue Event::path(ScriptState* script_state) const {
  return ScriptValue(
      script_state,
      ToV8(PathInternal(script_state, kNonEmptyAfterDispatch), script_state));
}

HeapVector<Member<EventTarget>> Event::composedPath(
    ScriptState* script_state) const {
  return PathInternal(script_state, kEmptyAfterDispatch);
}

void Event::SetHandlingPassive(PassiveMode mode) {
  handling_passive_ = mode;
  prevent_default_called_during_passive_ = false;
}

HeapVector<Member<EventTarget>> Event::PathInternal(ScriptState* script_state,
                                                    EventPathMode mode) const {
  if (target_)
    HostsUsingFeatures::CountHostOrIsolatedWorldHumanReadableName(
        script_state, *target_, HostsUsingFeatures::Feature::kEventPath);

  if (!current_target_) {
    DCHECK_EQ(Event::kNone, event_phase_);
    if (!event_path_) {
      // Before dispatching the event
      return HeapVector<Member<EventTarget>>();
    }
    DCHECK(!event_path_->IsEmpty());
    // After dispatching the event
    if (mode == kEmptyAfterDispatch)
      return HeapVector<Member<EventTarget>>();
    return event_path_->Last().GetTreeScopeEventContext().EnsureEventPath(
        *event_path_);
  }

  if (Node* node = current_target_->ToNode()) {
    DCHECK(event_path_);
    for (auto& context : event_path_->NodeEventContexts()) {
      if (node == context.GetNode())
        return context.GetTreeScopeEventContext().EnsureEventPath(*event_path_);
    }
    NOTREACHED();
  }

  if (LocalDOMWindow* window = current_target_->ToLocalDOMWindow()) {
    if (event_path_ && !event_path_->IsEmpty()) {
      return event_path_->TopNodeEventContext()
          .GetTreeScopeEventContext()
          .EnsureEventPath(*event_path_);
    }
    return HeapVector<Member<EventTarget>>(1, window);
  }

  return HeapVector<Member<EventTarget>>();
}

EventTarget* Event::currentTarget() const {
  if (!current_target_)
    return nullptr;
  Node* node = current_target_->ToNode();
  if (node && node->IsSVGElement()) {
    if (SVGElement* svg_element = ToSVGElement(node)->CorrespondingElement())
      return svg_element;
  }
  return current_target_.Get();
}

double Event::timeStamp(ScriptState* script_state) const {
  double time_stamp = 0;
  if (script_state && LocalDOMWindow::From(script_state)) {
    WindowPerformance* performance =
        DOMWindowPerformance::performance(*LocalDOMWindow::From(script_state));
    time_stamp =
        performance->MonotonicTimeToDOMHighResTimeStamp(platform_time_stamp_);
  }

  return time_stamp;
}

void Event::setCancelBubble(ScriptState* script_state, bool cancel) {
  if (cancel)
    propagation_stopped_ = true;
}

DispatchEventResult Event::DispatchEvent(EventDispatcher& dispatcher) {
  return dispatcher.Dispatch();
}

void Event::Trace(blink::Visitor* visitor) {
  visitor->Trace(current_target_);
  visitor->Trace(target_);
  visitor->Trace(underlying_event_);
  visitor->Trace(event_path_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
