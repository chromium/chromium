// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/app_history/app_history_navigate_event.h"

#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/core/app_history/app_history.h"
#include "third_party/blink/renderer/core/app_history/app_history_navigate_event_init.h"
#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/error_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"

namespace blink {

class NavigateCompletion final : public ScriptFunction {
 public:
  enum class ResolveType {
    kFulfill,
    kReject,
  };

  static v8::Local<v8::Function> CreateFunction(ScriptState* script_state,
                                                ResolveType type) {
    return MakeGarbageCollected<NavigateCompletion>(script_state, type)
        ->BindToV8Function();
  }

  NavigateCompletion(ScriptState* script_state, ResolveType type)
      : ScriptFunction(script_state),
        window_(LocalDOMWindow::From(script_state)),
        type_(type) {}

  void Trace(Visitor* visitor) const final {
    ScriptFunction::Trace(visitor);
    visitor->Trace(window_);
  }

 private:
  Event* InitEvent(ScriptValue value) const {
    if (type_ == ResolveType::kFulfill)
      return Event::Create(event_type_names::kNavigatesuccess);

    auto* isolate = window_->GetIsolate();
    v8::Local<v8::Message> message =
        v8::Exception::CreateMessage(isolate, value.V8Value());
    std::unique_ptr<SourceLocation> location =
        SourceLocation::FromMessage(isolate, message, window_);
    ErrorEvent* event = ErrorEvent::Create(
        ToCoreStringWithNullCheck(message->Get()), std::move(location), value,
        &DOMWrapperWorld::MainWorld());
    event->SetType(event_type_names::kNavigateerror);
    return event;
  }

  ScriptValue Call(ScriptValue value) final {
    DCHECK(window_);
    AppHistory::appHistory(*window_)->DispatchEvent(*InitEvent(value));
    window_ = nullptr;
    return ScriptValue();
  }

  Member<LocalDOMWindow> window_;
  ResolveType type_;
};

AppHistoryNavigateEvent::AppHistoryNavigateEvent(
    ExecutionContext* context,
    const AtomicString& type,
    AppHistoryNavigateEventInit* init)
    : Event(type, init),
      ExecutionContextClient(context),
      can_respond_(init->canRespond()),
      user_initiated_(init->userInitiated()),
      hash_change_(init->hashChange()),
      form_data_(init->formData()) {
  DCHECK(IsA<LocalDOMWindow>(context));
}

bool AppHistoryNavigateEvent::Fire(AppHistory* app_history,
                                   bool same_document) {
  app_history->DispatchEvent(*this);
  if (!DomWindow())
    return false;

  // If completion_promise_ is set, we will fire navigatesuccess/navigateerror
  // when completion_promise_ resolves/rejects:
  // * If respondWith() was called, completion_promise_ is already initialized
  //   with a JS-provided promise. Wait for that promise to resolve.
  // * If this is a same-document navigation and preventDefault() was not
  //   called, this navigation is being handled by the browser, and will
  //   complete synchronously. Set completion_promise_ to a resolved promise,
  //   which will cause navigatesuccess to fire on the next
  //   microtask.
  // * Otherwise, preventDefault() was called or the navigation is
  //   cross-document. Don't fire any completion event. In the preventDefault()
  //   case, the navigation was cancelled. In the cross-document case, any event
  //   handlers will be disconnected by the time the navigation has completed,
  //   so there's no way to listen for the completion event.
  if (!completion_promise_.IsEmpty() ||
      (!defaultPrevented() && same_document)) {
    auto* script_state = ToScriptStateForMainWorld(DomWindow()->GetFrame());
    ScriptState::Scope scope(script_state);
    if (completion_promise_.IsEmpty())
      completion_promise_ = ScriptPromise::CastUndefined(script_state);

    completion_promise_.Then(
        NavigateCompletion::CreateFunction(
            script_state, NavigateCompletion::ResolveType::kFulfill),
        NavigateCompletion::CreateFunction(
            script_state, NavigateCompletion::ResolveType::kReject));
  }
  return !defaultPrevented();
}

void AppHistoryNavigateEvent::respondWith(ScriptState* script_state,
                                          ScriptPromise newNavigationAction,
                                          ExceptionState& exception_state) {
  if (!DomWindow()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "respondWith may not be called in a "
                                      "detached window");
    return;
  }

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
  completion_promise_ = newNavigationAction;

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
  visitor->Trace(completion_promise_);
}

}  // namespace blink
