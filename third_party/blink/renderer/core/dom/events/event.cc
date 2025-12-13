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
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/focus_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/core/timing/worker_global_scope_performance.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

Event::Event() : Event(g_empty_atom, Bubbles::kNo, Cancelable::kNo) {
  was_initialized_ = false;
}

Event::Event(const AtomicString& event_type,
             Bubbles bubbles,
             Cancelable cancelable,
             base::TimeTicks platform_time_stamp)
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
            base::TimeTicks::Now()) {}

Event::Event(const AtomicString& event_type,
             Bubbles bubbles,
             Cancelable cancelable,
             ComposedMode composed_mode,
             base::TimeTicks platform_time_stamp)
    : type_(event_type),
      bubbles_(bubbles == Bubbles::kYes),
      cancelable_(cancelable == Cancelable::kYes),
      composed_(composed_mode == ComposedMode::kComposed),
      propagation_stopped_(false),
      immediate_propagation_stopped_(false),
      default_prevented_(false),
      default_handled_(false),
      was_initialized_(true),
      is_trusted_(false),
      prevent_default_called_on_uncancelable_event_(false),
      legacy_did_listeners_throw_flag_(false),
      fire_only_capture_listeners_at_target_(false),
      fire_only_non_capture_listeners_at_target_(false),
      copy_event_path_from_underlying_event_(false),
      handling_passive_(PassiveMode::kNotPassiveDefault),
      event_phase_(Event::PhaseType::kNone),
      current_target_(nullptr),
      platform_time_stamp_(platform_time_stamp) {}

Event::Event(const AtomicString& event_type,
             const EventInit* initializer,
             base::TimeTicks platform_time_stamp)
    : Event(event_type,
            initializer->bubbles() ? Bubbles::kYes : Bubbles::kNo,
            initializer->cancelable() ? Cancelable::kYes : Cancelable::kNo,
            initializer->composed() ? ComposedMode::kComposed
                                    : ComposedMode::kScoped,
            platform_time_stamp) {}

Event::~Event() = default;

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
    // Don't allow already prevented events to be reset.
    if (!defaultPrevented())
      default_prevented_ = false;
  } else {
    UseCounter::Count(ExecutionContext::From(script_state),
                      WebFeature::kEventSetReturnValueFalse);
    preventDefault();
  }
}

const AtomicString& Event::InterfaceName() const {
  return event_interface_names::kEvent;
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

bool Event::IsHighlightPointerEvent() const {
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

bool Event::IsClipboardEvent() const {
  return false;
}

bool Event::IsBeforeTextInsertedEvent() const {
  return false;
}

bool Event::IsBeforeCreatePolicyEvent() const {
  return false;
}

bool Event::IsBeforeUnloadEvent() const {
  return false;
}

bool Event::IsErrorEvent() const {
  return false;
}

bool Event::IsPatchEvent() const {
  return false;
}

bool Event::IsRouteEvent() const {
  return false;
}

void Event::preventDefault() {
  if (handling_passive_ != PassiveMode::kNotPassive &&
      handling_passive_ != PassiveMode::kNotPassiveDefault) {

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

EventTarget* Event::target() const {
  DCHECK(!target_ || !target_->ToNode() ||
         !target_->ToNode()->IsPseudoElement())
      << "Event target should not be a pseudo-element, but got "
      << target_->ToNode()->DebugName();
  return target_.Get();
}

void Event::SetTarget(EventTarget* target) {
  if (target_ == target)
    return;

  target_ = target;
  if (target_)
    ReceivedTarget();
}

void Event::SetRelatedTargetIfExists(EventTarget* related_target) {
  if (auto* mouse_event = DynamicTo<MouseEvent>(this)) {
    mouse_event->SetRelatedTarget(related_target);
  } else if (auto* pointer_event = DynamicTo<PointerEvent>(this)) {
    pointer_event->SetRelatedTarget(related_target);
  } else if (auto* focus_event = DynamicTo<FocusEvent>(this)) {
    focus_event->SetRelatedTarget(related_target);
  }
}

void Event::ReceivedTarget() {}

Element* Event::Retarget(Element* element) const {
  CHECK(RuntimeEnabledFeatures::ImprovedSourceRetargetingEnabled());
  if (!element) {
    return nullptr;
  }
  if (EventTarget* current_target = currentTarget()) {
    if (auto* current_target_node = current_target->ToNode()) {
      return &current_target_node->GetTreeScope().Retarget(*element);
    }
  }
  // retarget against the topmost TreeScope if there isn't a current target.
  return &element->GetDocument().Retarget(*element);
}

void Event::SetUnderlyingEvent(const Event* ue) {
  // Prohibit creation of a cycle -- just do nothing in that case.
  for (const Event* e = ue; e; e = e->UnderlyingEvent())
    if (e == this)
      return;
  underlying_event_ = ue;
}

void Event::InitEventPath(Node& node) {
  if (copy_event_path_from_underlying_event_) {
    event_path_ = underlying_event_->GetEventPath();
  } else if (!event_path_) {
    event_path_ = MakeGarbageCollected<EventPath>(node, this);
  } else {
    event_path_->InitializeWith(node, this);
  }
}

bool Event::IsFullyTrusted() const {
  const Event* event = this;
  while (event) {
    if (!event->isTrusted()) {
      return false;
    }
    event = event->UnderlyingEvent();
  }
  return true;
}

void Event::SetHandlingPassive(PassiveMode mode) {
  handling_passive_ = mode;
}

HeapVector<Member<EventTarget>> Event::composedPath(
    ScriptState* script_state) const {
  if (!current_target_) {
    DCHECK_EQ(Event::PhaseType::kNone, event_phase_);
    if (!event_path_) {
      // Before dispatching the event
      return HeapVector<Member<EventTarget>>();
    }
    DCHECK(!event_path_->IsEmpty());
    // After dispatching the event
    return HeapVector<Member<EventTarget>>();
  }

  if (Node* node = current_target_->ToNode()) {
    DCHECK(event_path_);
    for (auto& context : event_path_->NodeEventContexts()) {
      if (node == context.GetNode()) {
        return HeapVector<Member<EventTarget>>(
            context.GetTreeScopeEventContext().EnsureEventPath(*event_path_));
      }
    }
    NOTREACHED();
  }
  LocalDOMWindow* window = current_target_->ToLocalDOMWindow();
  if (window && event_path_ && !event_path_->IsEmpty()) {
    return HeapVector<Member<EventTarget>>(event_path_->TopNodeEventContext()
                                               .GetTreeScopeEventContext()
                                               .EnsureEventPath(*event_path_));
  }

  if (RuntimeEnabledFeatures::ComposedPathReturnTargetBeingDispatchedEnabled()
          ? IsBeingDispatched()
          : !!window) {
    return HeapVector<Member<EventTarget>>(1, current_target_);
  }

  return HeapVector<Member<EventTarget>>();
}

EventTarget* Event::currentTarget() const {
  if (!current_target_)
    return nullptr;
  if (auto* curr_svg_element =
          DynamicTo<SVGElement>(current_target_->ToNode())) {
    if (SVGElement* svg_element = curr_svg_element->CorrespondingElement())
      return svg_element;
  }
  return current_target_.Get();
}

double Event::timeStamp(ScriptState* script_state) const {
  if (!script_state) {
    return 0;
  }

  if (auto* window = LocalDOMWindow::From(script_state)) {
    Performance* performance = DOMWindowPerformance::performance(*window);
    return performance->MonotonicTimeToDOMHighResTimeStamp(
        platform_time_stamp_);
  } else if (auto* worker = DynamicTo<WorkerGlobalScope>(
                 ExecutionContext::From(script_state))) {
    Performance* performance =
        WorkerGlobalScopePerformance::performance(*worker);
    return performance->MonotonicTimeToDOMHighResTimeStamp(
        platform_time_stamp_);
  }

  return 0;
}

void Event::setCancelBubble(ScriptState* script_state, bool cancel) {
  if (cancel)
    propagation_stopped_ = true;
}

DispatchEventResult Event::DispatchEvent(EventDispatcher& dispatcher) {
  return dispatcher.Dispatch();
}

void Event::Trace(Visitor* visitor) const {
  visitor->Trace(current_target_);
  visitor->Trace(target_);
  visitor->Trace(pseudo_element_target_);
  visitor->Trace(underlying_event_);
  visitor->Trace(event_path_);
  ScriptWrappable::Trace(visitor);
}

CSSPseudoElement* Event::pseudoTarget() const {
  if (!RuntimeEnabledFeatures::CSSPseudoElementInterfaceEnabled()) {
    return nullptr;
  }
  PseudoElement* pseudo_element_target = PseudoElementTarget();
  if (!pseudo_element_target) {
    return nullptr;
  }

  // Boundary events like mouseover/mouseout/pointerover/pointerout and
  // related enter/leave events do not currently define a pseudoTarget.
  // Return null for those event types.
  if (IsMouseEvent() || IsPointerEvent()) {
    if (type() == event_type_names::kMouseover ||
        type() == event_type_names::kMouseout ||
        type() == event_type_names::kMouseenter ||
        type() == event_type_names::kMouseleave ||
        type() == event_type_names::kPointerover ||
        type() == event_type_names::kPointerout ||
        type() == event_type_names::kPointerenter ||
        type() == event_type_names::kPointerleave) {
      return nullptr;
    }
  }

  Element& target_element = *To<Element>(target()->ToNode());
  return target_element.EnsureCSSPseudoElement(
      pseudo_element_target->GetPseudoId());
}

}  // namespace blink
