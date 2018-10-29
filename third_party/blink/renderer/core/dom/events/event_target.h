/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_EVENTS_EVENT_TARGET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_EVENTS_EVENT_TARGET_H_

#include <memory>

#include "base/macros.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/add_event_listener_options_resolved.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatch_result.h"
#include "third_party/blink/renderer/core/dom/events/event_listener_map.h"
#include "third_party/blink/renderer/core/event_names.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class AddEventListenerOptionsOrBoolean;
class DOMWindow;
class Event;
class EventListenerOptionsOrBoolean;
class ExceptionState;
class ExecutionContext;
class LocalDOMWindow;
class MessagePort;
class Node;
class ScriptState;
class ServiceWorker;

struct FiringEventIterator {
  DISALLOW_NEW();
  FiringEventIterator(const AtomicString& event_type,
                      wtf_size_t& iterator,
                      wtf_size_t& end)
      : event_type(event_type), iterator(iterator), end(end) {}

  const AtomicString& event_type;
  wtf_size_t& iterator;
  wtf_size_t& end;
};
using FiringEventIteratorVector = Vector<FiringEventIterator, 1>;

class CORE_EXPORT EventTargetData final
    : public GarbageCollectedFinalized<EventTargetData> {
 public:
  EventTargetData();
  ~EventTargetData();

  void Trace(blink::Visitor*);

  EventListenerMap event_listener_map;
  std::unique_ptr<FiringEventIteratorVector> firing_event_iterators;
  DISALLOW_COPY_AND_ASSIGN(EventTargetData);
};

// All DOM event targets extend EventTarget. The spec is defined here:
// https://dom.spec.whatwg.org/#interface-eventtarget
// EventTarget objects allow us to add and remove an event
// listeners of a specific event type. Each EventTarget object also represents
// the target to which an event is dispatched when something has occurred.
// All nodes are EventTargets, some other event targets include: XMLHttpRequest,
// AudioNode and AudioContext.

// To make your class an EventTarget, follow these steps:
// - Make your IDL interface inherit from EventTarget.
// - Inherit from EventTargetWithInlineData (only in rare cases should you
//   use EventTarget directly).
// - In your class declaration, EventTargetWithInlineData must come first in
//   the base class list. If your class is non-final, classes inheriting from
//   your class need to come first, too.
// - If you added an onfoo attribute, use DEFINE_ATTRIBUTE_EVENT_LISTENER(foo)
//   in your class declaration. Add "attribute EventHandler onfoo;" to the IDL
//   file.
// - Override EventTarget::interfaceName() and getExecutionContext(). The former
//   will typically return EventTargetNames::YourClassName. The latter will
//   return PausableObject::executionContext (if you are an
//   PausableObject)
//   or the document you're in.
// - Your trace() method will need to call EventTargetWithInlineData::trace
//   depending on the base class of your class.
// - EventTargets do not support EAGERLY_FINALIZE. You need to use
//   a pre-finalizer instead.
class CORE_EXPORT EventTarget : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~EventTarget() override;

  virtual const AtomicString& InterfaceName() const = 0;
  virtual ExecutionContext* GetExecutionContext() const = 0;

  virtual Node* ToNode();
  virtual const DOMWindow* ToDOMWindow() const;
  virtual const LocalDOMWindow* ToLocalDOMWindow() const;
  virtual LocalDOMWindow* ToLocalDOMWindow();
  virtual MessagePort* ToMessagePort();
  virtual ServiceWorker* ToServiceWorker();

  static EventTarget* Create(ScriptState*);

  bool addEventListener(const AtomicString& event_type,
                        EventListener*,
                        bool use_capture = false);
  bool addEventListener(const AtomicString& event_type,
                        EventListener*,
                        const AddEventListenerOptionsOrBoolean&);
  bool addEventListener(const AtomicString& event_type,
                        EventListener*,
                        AddEventListenerOptionsResolved&);

  bool removeEventListener(const AtomicString& event_type,
                           const EventListener*,
                           bool use_capture = false);
  bool removeEventListener(const AtomicString& event_type,
                           const EventListener*,
                           const EventListenerOptionsOrBoolean&);
  bool removeEventListener(const AtomicString& event_type,
                           const EventListener*,
                           EventListenerOptions&);
  virtual void RemoveAllEventListeners();

  DispatchEventResult DispatchEvent(Event&);

  void EnqueueEvent(Event&, TaskType);

  // dispatchEventForBindings is intended to only be called from
  // javascript originated calls. This method will validate and may adjust
  // the Event object before dispatching.
  bool dispatchEventForBindings(Event*, ExceptionState&);

  // Used for legacy "onEvent" attribute APIs.
  bool SetAttributeEventListener(const AtomicString& event_type,
                                 EventListener*);
  EventListener* GetAttributeEventListener(const AtomicString& event_type);

  bool HasEventListeners() const;
  bool HasEventListeners(const AtomicString& event_type) const;
  bool HasCapturingEventListeners(const AtomicString& event_type);
  EventListenerVector* GetEventListeners(const AtomicString& event_type);
  Vector<AtomicString> EventTypes();

  DispatchEventResult FireEventListeners(Event&);

  static DispatchEventResult GetDispatchEventResult(const Event&);

  virtual bool KeepEventInNode(const Event&) const { return false; }

  virtual bool IsWindowOrWorkerGlobalScope() const { return false; }

  // Returns true if the target is window, window.document, or
  // window.document.body.
  bool IsTopLevelNode();

 protected:
  EventTarget();

  virtual bool AddEventListenerInternal(const AtomicString& event_type,
                                        EventListener*,
                                        const AddEventListenerOptionsResolved&);
  virtual bool RemoveEventListenerInternal(const AtomicString& event_type,
                                           const EventListener*,
                                           const EventListenerOptions&);

  // Called when an event listener has been successfully added.
  virtual void AddedEventListener(const AtomicString& event_type,
                                  RegisteredEventListener&);

  // Called when an event listener is removed. The original registration
  // parameters of this event listener are available to be queried.
  virtual void RemovedEventListener(const AtomicString& event_type,
                                    const RegisteredEventListener&);

  virtual DispatchEventResult DispatchEventInternal(Event&);

  // Subclasses should likely not override these themselves; instead, they
  // should subclass EventTargetWithInlineData.
  virtual EventTargetData* GetEventTargetData() = 0;
  virtual EventTargetData& EnsureEventTargetData() = 0;

 private:
  LocalDOMWindow* ExecutingWindow();
  void SetDefaultAddEventListenerOptions(const AtomicString& event_type,
                                         EventListener*,
                                         AddEventListenerOptionsResolved&);

  RegisteredEventListener* GetAttributeRegisteredEventListener(
      const AtomicString& event_type);

  bool FireEventListeners(Event&, EventTargetData*, EventListenerVector&);
  void CountLegacyEvents(const AtomicString& legacy_type_name,
                         EventListenerVector*,
                         EventListenerVector*);

  void DispatchEnqueuedEvent(Event*, ExecutionContext*);

  friend class EventListenerIterator;
};

class CORE_EXPORT EventTargetWithInlineData : public EventTarget {
 public:
  ~EventTargetWithInlineData() override = default;

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(event_target_data_);
    EventTarget::Trace(visitor);
  }

 protected:
  EventTargetData* GetEventTargetData() final { return &event_target_data_; }
  EventTargetData& EnsureEventTargetData() final { return event_target_data_; }

 private:
  // EventTargetData is a GCed object, so it should not be used as a part of
  // object. However, we intentionally use it as a part of object for
  // performance, assuming that no one extracts a pointer of
  // EventTargetWithInlineData::event_target_data_ and store it to a Member etc.
  GC_PLUGIN_IGNORE("513199") EventTargetData event_target_data_;
};

// FIXME: These macros should be split into separate DEFINE and DECLARE
// macros to avoid causing so many header includes.
#define DEFINE_ATTRIBUTE_EVENT_LISTENER(attribute)                  \
  EventListener* on##attribute() {                                  \
    return GetAttributeEventListener(EventTypeNames::attribute);    \
  }                                                                 \
  void setOn##attribute(EventListener* listener) {                  \
    SetAttributeEventListener(EventTypeNames::attribute, listener); \
  }

#define DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(attribute)                    \
  static EventListener* on##attribute(EventTarget& eventTarget) {            \
    return eventTarget.GetAttributeEventListener(EventTypeNames::attribute); \
  }                                                                          \
  static void setOn##attribute(EventTarget& eventTarget,                     \
                               EventListener* listener) {                    \
    eventTarget.SetAttributeEventListener(EventTypeNames::attribute,         \
                                          listener);                         \
  }

#define DEFINE_WINDOW_ATTRIBUTE_EVENT_LISTENER(attribute)                    \
  EventListener* on##attribute() {                                           \
    return GetDocument().GetWindowAttributeEventListener(                    \
        EventTypeNames::attribute);                                          \
  }                                                                          \
  void setOn##attribute(EventListener* listener) {                           \
    GetDocument().SetWindowAttributeEventListener(EventTypeNames::attribute, \
                                                  listener);                 \
  }

#define DEFINE_STATIC_WINDOW_ATTRIBUTE_EVENT_LISTENER(attribute)             \
  static EventListener* on##attribute(EventTarget& eventTarget) {            \
    if (Node* node = eventTarget.ToNode())                                   \
      return node->GetDocument().GetWindowAttributeEventListener(            \
          EventTypeNames::attribute);                                        \
    DCHECK(eventTarget.ToLocalDOMWindow());                                  \
    return eventTarget.GetAttributeEventListener(EventTypeNames::attribute); \
  }                                                                          \
  static void setOn##attribute(EventTarget& eventTarget,                     \
                               EventListener* listener) {                    \
    if (Node* node = eventTarget.ToNode())                                   \
      node->GetDocument().SetWindowAttributeEventListener(                   \
          EventTypeNames::attribute, listener);                              \
    else {                                                                   \
      DCHECK(eventTarget.ToLocalDOMWindow());                                \
      eventTarget.SetAttributeEventListener(EventTypeNames::attribute,       \
                                            listener);                       \
    }                                                                        \
  }

#define DEFINE_MAPPED_ATTRIBUTE_EVENT_LISTENER(attribute, eventName) \
  EventListener* on##attribute() {                                   \
    return GetAttributeEventListener(EventTypeNames::eventName);     \
  }                                                                  \
  void setOn##attribute(EventListener* listener) {                   \
    SetAttributeEventListener(EventTypeNames::eventName, listener);  \
  }

DISABLE_CFI_PERF
inline bool EventTarget::HasEventListeners() const {
  // FIXME: We should have a const version of eventTargetData.
  if (const EventTargetData* d =
          const_cast<EventTarget*>(this)->GetEventTargetData())
    return !d->event_listener_map.IsEmpty();
  return false;
}

DISABLE_CFI_PERF
inline bool EventTarget::HasEventListeners(
    const AtomicString& event_type) const {
  // FIXME: We should have const version of eventTargetData.
  if (const EventTargetData* d =
          const_cast<EventTarget*>(this)->GetEventTargetData())
    return d->event_listener_map.Contains(event_type);
  return false;
}

inline bool EventTarget::HasCapturingEventListeners(
    const AtomicString& event_type) {
  EventTargetData* d = GetEventTargetData();
  if (!d)
    return false;
  return d->event_listener_map.ContainsCapturing(event_type);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_EVENTS_EVENT_TARGET_H_
