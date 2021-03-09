// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/app_history/app_history_navigate_event.h"

#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/core/app_history/app_history_navigate_event_init.h"
#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"

namespace blink {

class RespondWithFunction final : public ScriptFunction {
 public:
  enum class ResolveType {
    kFulfill,
    kReject,
  };

  static v8::Local<v8::Function> CreateFunction(
      ScriptState* script_state,
      AppHistoryNavigateEvent* event) {
    return MakeGarbageCollected<RespondWithFunction>(script_state, event)
        ->BindToV8Function();
  }

  RespondWithFunction(ScriptState* script_state, AppHistoryNavigateEvent* event)
      : ScriptFunction(script_state), event_(event) {}

  void Trace(Visitor* visitor) const final {
    ScriptFunction::Trace(visitor);
    visitor->Trace(event_);
  }

 private:
  ScriptValue Call(ScriptValue) final {
    DCHECK(event_);
    event_ = nullptr;
    return ScriptValue();
  }

  Member<AppHistoryNavigateEvent> event_;
};

AppHistoryNavigateEvent::AppHistoryNavigateEvent(
    ExecutionContext* context,
    const AtomicString& type,
    AppHistoryNavigateEventInit* init)
    : Event(type, init),
      ExecutionContextClient(context),
      can_respond_(init->canRespond()),
      hash_change_(init->hashChange()),
      form_data_(init->formData()) {
  DCHECK(IsA<LocalDOMWindow>(context));
}

void AppHistoryNavigateEvent::respondWith(ScriptState* script_state,
                                          ScriptPromise newNavigationAction,
                                          ExceptionState& exception_state) {
  if (!isTrusted()) {
    exception_state.ThrowSecurityError(
        "respondWith may only be called on a "
        "trusted event during event dispatch");
    return;
  }

  if (!can_respond_) {
    exception_state.ThrowSecurityError(
        "A navigation with URL '" + url_.ElidedString() +
        "' cannot be intercepted by respondWith in a window with origin '" +
        DomWindow()->GetSecurityOrigin()->ToString() + "' and URL '" +
        DomWindow()->Url().ElidedString() + "'.");
    return;
  }

  if (!IsBeingDispatched() || defaultPrevented()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "respondWith may only be called during "
                                      "the first dispatch of this event");
    return;
  }

  preventDefault();
  newNavigationAction.Then(
      RespondWithFunction::CreateFunction(script_state, this),
      RespondWithFunction::CreateFunction(script_state, this));

  DocumentLoader* loader = DomWindow()->document()->Loader();
  loader->RunURLAndHistoryUpdateSteps(url_, state_object_, frame_load_type_);
}

const AtomicString& AppHistoryNavigateEvent::InterfaceName() const {
  return event_interface_names::kAppHistoryNavigateEvent;
}

void AppHistoryNavigateEvent::Trace(Visitor* visitor) const {
  Event::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(form_data_);
}

}  // namespace blink
