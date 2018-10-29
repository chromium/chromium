/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights
 * reserved.
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
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_EVENTS_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_EVENTS_EVENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatch_result.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class DOMWrapperWorld;
class EventDispatcher;
class EventInit;
class EventPath;
class EventTarget;
class ScriptState;
class ScriptValue;

class CORE_EXPORT Event : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum class Bubbles {
    kYes,
    kNo,
  };

  enum class Cancelable {
    kYes,
    kNo,
  };

  enum PhaseType {
    kNone = 0,
    kCapturingPhase = 1,
    kAtTarget = 2,
    kBubblingPhase = 3
  };

  enum RailsMode {
    kRailsModeFree = 0,
    kRailsModeHorizontal = 1,
    kRailsModeVertical = 2
  };

  enum class ComposedMode {
    kComposed,
    kScoped,
  };

  enum class PassiveMode {
    // Not passive, default initialized.
    kNotPassiveDefault,
    // Not passive, explicitly specified.
    kNotPassive,
    // Passive, explicitly specified.
    kPassive,
    // Passive, not explicitly specified and forced due to document level
    // listener.
    kPassiveForcedDocumentLevel,
    // Passive, default initialized.
    kPassiveDefault,
  };

  static Event* Create() { return new Event; }

  static Event* Create(const AtomicString& type) {
    return new Event(type, Bubbles::kNo, Cancelable::kNo);
  }
  static Event* CreateCancelable(const AtomicString& type) {
    return new Event(type, Bubbles::kNo, Cancelable::kYes);
  }
  static Event* CreateBubble(const AtomicString& type) {
    return new Event(type, Bubbles::kYes, Cancelable::kNo);
  }
  static Event* CreateCancelableBubble(const AtomicString& type) {
    return new Event(type, Bubbles::kYes, Cancelable::kYes);
  }

  static Event* Create(const AtomicString& type, const EventInit& initializer) {
    return new Event(type, initializer);
  }

  ~Event() override;

  void initEvent(const AtomicString& type, bool bubbles, bool cancelable);
  void initEvent(const AtomicString& event_type_arg,
                 bool bubbles_arg,
                 bool cancelable_arg,
                 EventTarget* related_target);

  const AtomicString& type() const { return type_; }
  void SetType(const AtomicString& type) { type_ = type; }

  EventTarget* target() const { return target_.Get(); }
  void SetTarget(EventTarget*);

  EventTarget* currentTarget() const;
  void SetCurrentTarget(EventTarget* current_target) {
    current_target_ = current_target;
  }

  // This callback is invoked when an event listener has been dispatched
  // at the current target. It should only be used to influence UMA metrics
  // and not change functionality since observing the presence of listeners
  // is dangerous.
  virtual void DoneDispatchingEventAtCurrentTarget();

  void SetRelatedTargetIfExists(EventTarget* related_target);

  unsigned short eventPhase() const { return event_phase_; }
  void SetEventPhase(unsigned short event_phase) { event_phase_ = event_phase; }

  void SetFireOnlyCaptureListenersAtTarget(
      bool fire_only_capture_listeners_at_target) {
    DCHECK_EQ(event_phase_, kAtTarget);
    fire_only_capture_listeners_at_target_ =
        fire_only_capture_listeners_at_target;
  }

  void SetFireOnlyNonCaptureListenersAtTarget(
      bool fire_only_non_capture_listeners_at_target) {
    DCHECK_EQ(event_phase_, kAtTarget);
    fire_only_non_capture_listeners_at_target_ =
        fire_only_non_capture_listeners_at_target;
  }

  bool FireOnlyCaptureListenersAtTarget() const {
    return fire_only_capture_listeners_at_target_;
  }
  bool FireOnlyNonCaptureListenersAtTarget() const {
    return fire_only_non_capture_listeners_at_target_;
  }

  bool bubbles() const { return bubbles_; }
  bool cancelable() const { return cancelable_; }
  bool composed() const { return composed_; }
  bool IsScopedInV0() const;

  // Event creation timestamp in milliseconds. It returns a DOMHighResTimeStamp
  // using the platform timestamp (see |platform_time_stamp_|).
  // For more info see http://crbug.com/160524
  double timeStamp(ScriptState*) const;
  TimeTicks PlatformTimeStamp() const { return platform_time_stamp_; }

  void stopPropagation() { propagation_stopped_ = true; }
  void SetStopPropagation(bool stop_propagation) {
    propagation_stopped_ = stop_propagation;
  }
  void stopImmediatePropagation() { immediate_propagation_stopped_ = true; }
  void SetStopImmediatePropagation(bool stop_immediate_propagation) {
    immediate_propagation_stopped_ = stop_immediate_propagation;
  }

  // IE Extensions
  EventTarget* srcElement() const {
    return target();
  }  // MSIE extension - "the object that fired the event"

  bool legacyReturnValue(ScriptState*) const;
  void setLegacyReturnValue(ScriptState*, bool return_value);

  virtual const AtomicString& InterfaceName() const;
  bool HasInterface(const AtomicString&) const;

  // These events are general classes of events.
  virtual bool IsUIEvent() const;
  virtual bool IsMouseEvent() const;
  virtual bool IsFocusEvent() const;
  virtual bool IsKeyboardEvent() const;
  virtual bool IsTouchEvent() const;
  virtual bool IsGestureEvent() const;
  virtual bool IsWheelEvent() const;
  virtual bool IsPointerEvent() const;
  virtual bool IsInputEvent() const;
  virtual bool IsCompositionEvent() const;

  // Drag events are a subset of mouse events.
  virtual bool IsDragEvent() const;

  // These events lack a DOM interface.
  virtual bool IsClipboardEvent() const;
  virtual bool IsBeforeTextInsertedEvent() const;

  virtual bool IsBeforeUnloadEvent() const;
  virtual bool IsErrorEvent() const;

  virtual bool IsActivateInvisibleEvent() const;

  bool PropagationStopped() const {
    return propagation_stopped_ || immediate_propagation_stopped_;
  }
  bool ImmediatePropagationStopped() const {
    return immediate_propagation_stopped_;
  }
  bool WasInitialized() { return was_initialized_; }

  bool defaultPrevented() const { return default_prevented_; }
  virtual void preventDefault();
  void SetDefaultPrevented(bool default_prevented) {
    default_prevented_ = default_prevented;
  }

  bool DefaultHandled() const { return default_handled_; }
  void SetDefaultHandled() { default_handled_ = true; }

  bool cancelBubble(ScriptState* = nullptr) const {
    return PropagationStopped();
  }
  void setCancelBubble(ScriptState*, bool);

  Event* UnderlyingEvent() const { return underlying_event_.Get(); }
  void SetUnderlyingEvent(Event*);

  bool HasEventPath() { return event_path_; }
  EventPath& GetEventPath() {
    DCHECK(event_path_);
    return *event_path_;
  }
  void InitEventPath(Node&);

  ScriptValue path(ScriptState*) const;
  HeapVector<Member<EventTarget>> composedPath(ScriptState*) const;

  bool IsBeingDispatched() const { return eventPhase(); }

  // Events that must not leak across isolated world, similar to how
  // ErrorEvent behaves, can override this method.
  virtual bool CanBeDispatchedInWorld(const DOMWrapperWorld&) const {
    return true;
  }

  bool isTrusted() const { return is_trusted_; }
  void SetTrusted(bool value) { is_trusted_ = value; }

  void SetComposed(bool composed) {
    DCHECK(!IsBeingDispatched());
    composed_ = composed;
  }

  void SetHandlingPassive(PassiveMode);

  bool PreventDefaultCalledDuringPassive() const {
    return prevent_default_called_during_passive_;
  }

  bool PreventDefaultCalledOnUncancelableEvent() const {
    return prevent_default_called_on_uncancelable_event_;
  }

  bool executedListenerOrDefaultAction() const {
    return executed_listener_or_default_action_;
  }

  void SetExecutedListenerOrDefaultAction() {
    executed_listener_or_default_action_ = true;
  }

  bool LegacyDidListenersThrow() const {
    return legacy_did_listeners_throw_flag_;
  }

  void LegacySetDidListenersThrowFlag() {
    legacy_did_listeners_throw_flag_ = true;
  }

  virtual DispatchEventResult DispatchEvent(EventDispatcher&);

  void Trace(blink::Visitor*) override;

 protected:
  Event();
  Event(const AtomicString& type,
        Bubbles,
        Cancelable,
        ComposedMode,
        TimeTicks platform_time_stamp);
  Event(const AtomicString& type,
        Bubbles,
        Cancelable,
        TimeTicks platform_time_stamp);
  Event(const AtomicString& type,
        Bubbles,
        Cancelable,
        ComposedMode = ComposedMode::kScoped);
  Event(const AtomicString& type,
        const EventInit&,
        TimeTicks platform_time_stamp);
  Event(const AtomicString& type, const EventInit& init)
      : Event(type, init, CurrentTimeTicks()) {}

  virtual void ReceivedTarget();

  void SetBubbles(bool bubble) { bubbles_ = bubble; }

  PassiveMode HandlingPassive() const { return handling_passive_; }

 private:
  enum EventPathMode { kEmptyAfterDispatch, kNonEmptyAfterDispatch };

  HeapVector<Member<EventTarget>> PathInternal(ScriptState*,
                                               EventPathMode) const;

  AtomicString type_;
  unsigned bubbles_ : 1;
  unsigned cancelable_ : 1;
  unsigned composed_ : 1;
  unsigned is_event_type_scoped_in_v0_ : 1;

  unsigned propagation_stopped_ : 1;
  unsigned immediate_propagation_stopped_ : 1;
  unsigned default_prevented_ : 1;
  unsigned default_handled_ : 1;
  unsigned was_initialized_ : 1;
  unsigned is_trusted_ : 1;
  // Only if at least one listeners or default actions are executed on an event
  // does Event Timing report it.
  unsigned executed_listener_or_default_action_ : 1;

  // Whether preventDefault was called when |handling_passive_| is
  // true. This field is reset on each call to SetHandlingPassive.
  unsigned prevent_default_called_during_passive_ : 1;
  // Whether preventDefault was called on uncancelable event.
  unsigned prevent_default_called_on_uncancelable_event_ : 1;

  // Whether any of listeners have thrown an exception or not.
  // Corresponds to |legacyOutputDidListenersThrowFlag| in DOM standard.
  // https://dom.spec.whatwg.org/#dispatching-events
  // https://dom.spec.whatwg.org/#concept-event-listener-inner-invoke
  unsigned legacy_did_listeners_throw_flag_ : 1;

  // This fields are effective only when
  // CallCaptureListenersAtCapturePhaseAtShadowHosts runtime flag is enabled.
  unsigned fire_only_capture_listeners_at_target_ : 1;
  unsigned fire_only_non_capture_listeners_at_target_ : 1;

  PassiveMode handling_passive_;
  unsigned short event_phase_;
  Member<EventTarget> current_target_;
  Member<EventTarget> target_;
  Member<Event> underlying_event_;
  Member<EventPath> event_path_;
  // The monotonic platform time in seconds, for input events it is the
  // event timestamp provided by the host OS and reported in the original
  // WebInputEvent instance.
  TimeTicks platform_time_stamp_;
};

#define DEFINE_EVENT_TYPE_CASTS(typeName)                          \
  DEFINE_TYPE_CASTS(typeName, Event, event, event->Is##typeName(), \
                    event.Is##typeName())

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_EVENTS_EVENT_H_
