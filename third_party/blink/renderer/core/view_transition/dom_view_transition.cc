// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/view_transition/dom_view_transition.h"

#include "third_party/blink/renderer/bindings/core/v8/capture_source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_property.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/events/error_event.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"

namespace blink {

namespace {

const char kAbortedMessage[] = "Transition was skipped";
const char kInvalidStateMessage[] =
    "Transition was aborted because of invalid state";
const char kTimeoutMessage[] =
    "Transition was aborted because of timeout in DOM update";

}  // namespace

DOMViewTransition::DOMViewTransition(ExecutionContext& execution_context,
                                     ViewTransition& view_transition)
    : DOMViewTransition(execution_context,
                        view_transition,
                        /*update_dom_callback=*/nullptr) {
  if (view_transition.IsForNavigationOnNewDocument()) {
    // In a cross-document view transition, the DOM is "updated" by the
    // navigation so by the time we create this object (in the pagereveal
    // event), the update is complete.
    dom_updated_promise_property_->ResolveWithUndefined();
    dom_callback_result_ = DOMCallbackResult::kSucceeded;
  }
}

DOMViewTransition::DOMViewTransition(
    ExecutionContext& execution_context,
    ViewTransition& view_transition,
    V8ViewTransitionCallback* update_dom_callback)
    : ExecutionContextLifecycleObserver(&execution_context),
      execution_context_(&execution_context),
      view_transition_{&view_transition},
      update_dom_callback_(update_dom_callback),
      finished_promise_property_(
          MakeGarbageCollected<PromiseProperty>(execution_context_)),
      ready_promise_property_(
          MakeGarbageCollected<PromiseProperty>(execution_context_)),
      dom_updated_promise_property_(
          MakeGarbageCollected<PromiseProperty>(execution_context_)) {
  CHECK(execution_context_->GetAgent());
}

DOMViewTransition::~DOMViewTransition() = default;

void DOMViewTransition::ContextDestroyed() {
  execution_context_.Clear();
}

void DOMViewTransition::skipTransition() {
  view_transition_->SkipTransition();
}

ScriptPromise<IDLUndefined> DOMViewTransition::finished(
    ScriptState* script_state) const {
  return finished_promise_property_->Promise(script_state->World());
}

ScriptPromise<IDLUndefined> DOMViewTransition::ready(
    ScriptState* script_state) const {
  return ready_promise_property_->Promise(script_state->World());
}

ScriptPromise<IDLUndefined> DOMViewTransition::updateCallbackDone(
    ScriptState* script_state) const {
  return dom_updated_promise_property_->Promise(script_state->World());
}

void DOMViewTransition::DidSkipTransition(
    ViewTransition::PromiseResponse response) {
  CHECK_NE(response, ViewTransition::PromiseResponse::kResolve);

  if (!execution_context_) {
    return;
  }

  // If the ready promise has not yet been resolved, reject it.
  if (ready_promise_property_->GetState() == PromiseProperty::State::kPending) {
    AtMicrotask(response, ready_promise_property_);
  }

  // If we haven't run the dom change callback yet, schedule a task to do so.
  // The finished promise will propagate the result of the updateCallbackDone
  // promise when this callback runs.
  if (dom_callback_result_ == DOMCallbackResult::kNotInvoked) {
    execution_context_->GetTaskRunner(TaskType::kMiscPlatformAPI)
        ->PostTask(FROM_HERE,
                   WTF::BindOnce(&DOMViewTransition::InvokeDOMChangeCallback,
                                 WrapPersistent(this)));
  } else if (dom_callback_result_ == DOMCallbackResult::kFailed) {
    // If the DOM callback finished and there was a failure then the finished
    // promise should have been rejected with updateCallbackDone.
    CHECK_EQ(finished_promise_property_->GetState(),
             PromiseProperty::State::kRejected);
  } else if (dom_callback_result_ == DOMCallbackResult::kSucceeded) {
    // But if the callback was successful, we need to resolve the finished
    // promise while skipping the transition.
    AtMicrotask(ViewTransition::PromiseResponse::kResolve,
                finished_promise_property_);
  }
}

void DOMViewTransition::NotifyDOMCallbackFinished(bool success,
                                                  ScriptValue value) {
  CHECK_EQ(dom_callback_result_, DOMCallbackResult::kRunning);
  // Handle all promises which depend on this callback.
  if (success) {
    dom_updated_promise_property_->ResolveWithUndefined();

    // If we're already at the terminal state, the transition was skipped before
    // the callback finished. Also handle the finish promise.
    if (view_transition_->IsDone()) {
      finished_promise_property_->ResolveWithUndefined();
    }
  } else {
    dom_updated_promise_property_->Reject(value);

    // The ready promise rejects with the value of updateCallbackDone callback
    // if it's skipped because of an error in the callback.
    if (!view_transition_->IsDone()) {
      ready_promise_property_->Reject(value);
    }

    // If the domUpdate callback fails the transition is skipped. The finish
    // promise should mirror the result of updateCallbackDone.
    finished_promise_property_->Reject(value);
  }

  dom_callback_result_ =
      success ? DOMCallbackResult::kSucceeded : DOMCallbackResult::kFailed;
  view_transition_->NotifyDOMCallbackFinished(success);
}

void DOMViewTransition::DidStartAnimating() {
  AtMicrotask(ViewTransition::PromiseResponse::kResolve,
              ready_promise_property_);
}

void DOMViewTransition::DidFinishAnimating() {
  AtMicrotask(ViewTransition::PromiseResponse::kResolve,
              finished_promise_property_);
}

void DOMViewTransition::InvokeDOMChangeCallback() {
  CHECK_EQ(dom_callback_result_, DOMCallbackResult::kNotInvoked)
      << "UpdateDOM callback invoked multiple times.";

  if (!execution_context_) {
    return;
  }

  dom_callback_result_ = DOMCallbackResult::kRunning;

  ScriptPromise<IDLUndefined> result;

  // It's ok to use the main world when there is no callback, since we're only
  // using it to call DOMChangeFinishedCallback which doesn't use the script
  // state or execute any script.
  ScriptState* script_state =
      update_dom_callback_ ? update_dom_callback_->CallbackRelevantScriptState()
                           : ToScriptStateForMainWorld(execution_context_);
  ScriptState::Scope scope(script_state);

  if (update_dom_callback_) {
    v8::Maybe<ScriptPromise<IDLUndefined>> maybe_result =
        update_dom_callback_->Invoke(nullptr);

    // If the callback couldn't be run for some reason, treat it as an empty
    // promise rejected with an abort exception.
    if (maybe_result.IsNothing()) {
      result = ScriptPromise<IDLUndefined>::RejectWithDOMException(
          script_state, MakeGarbageCollected<DOMException>(
                            DOMExceptionCode::kAbortError, kAbortedMessage));
    } else {
      result = maybe_result.FromJust();
    }
  } else {
    // If there's no callback provided, treat the same as an empty promise
    // resolved without a value.
    result = ToResolvedUndefinedPromise(script_state);
  }

  // Note, the DOMChangeFinishedCallback will be invoked asynchronously.
  result.Then(MakeGarbageCollected<ScriptFunction>(
                  script_state,
                  MakeGarbageCollected<DOMChangeFinishedCallback>(*this, true)),
              MakeGarbageCollected<ScriptFunction>(
                  script_state, MakeGarbageCollected<DOMChangeFinishedCallback>(
                                    *this, false)));
}

void DOMViewTransition::Trace(Visitor* visitor) const {
  visitor->Trace(execution_context_);
  visitor->Trace(view_transition_);
  visitor->Trace(update_dom_callback_);
  visitor->Trace(finished_promise_property_);
  visitor->Trace(ready_promise_property_);
  visitor->Trace(dom_updated_promise_property_);

  ExecutionContextLifecycleObserver::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

void DOMViewTransition::AtMicrotask(ViewTransition::PromiseResponse response,
                                    PromiseProperty* property) {
  if (!execution_context_) {
    return;
  }
  execution_context_->GetAgent()->event_loop()->EnqueueMicrotask(
      WTF::BindOnce(&DOMViewTransition::HandlePromise, WrapPersistent(this),
                    response, WrapPersistent(property)));
}

void DOMViewTransition::HandlePromise(ViewTransition::PromiseResponse response,
                                      PromiseProperty* property) {
  if (!execution_context_) {
    return;
  }

  // It's possible for multiple fulfillment microtasks to be queued so
  // early-out if that's happened.
  if (property->GetState() != PromiseProperty::State::kPending) {
    return;
  }

  // The main world is used here only to create a ScriptValue. While the
  // promises may be accessed from other worlds (in the cross-document case, an
  // extension can add a `pagereveal` event listener) the promises are
  // fulfilled using ScriptPromiseProperty which tracks requests from each
  // world and clones the passed value if needed.
  ScriptState* main_world_script_state =
      ToScriptStateForMainWorld(execution_context_);

  if (!main_world_script_state) {
    return;
  }

  switch (response) {
    case ViewTransition::PromiseResponse::kResolve:
      property->ResolveWithUndefined();
      break;
    case ViewTransition::PromiseResponse::kRejectAbort: {
      ScriptState::Scope scope(main_world_script_state);
      auto value = ScriptValue::From(
          main_world_script_state,
          MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError,
                                             kAbortedMessage));
      property->Reject(value);
      break;
    }
    case ViewTransition::PromiseResponse::kRejectInvalidState: {
      ScriptState::Scope scope(main_world_script_state);
      auto value = ScriptValue::From(
          main_world_script_state,
          MakeGarbageCollected<DOMException>(
              DOMExceptionCode::kInvalidStateError, kInvalidStateMessage));
      property->Reject(value);
      break;
    }
    case ViewTransition::PromiseResponse::kRejectTimeout: {
      ScriptState::Scope scope(main_world_script_state);
      auto value = ScriptValue::From(
          main_world_script_state,
          MakeGarbageCollected<DOMException>(DOMExceptionCode::kTimeoutError,
                                             kTimeoutMessage));
      property->Reject(value);
      break;
    }
  }
}

ViewTransitionTypeSet* DOMViewTransition::types() const {
  return view_transition_->Types();
}

// DOMChangeFinishedCallback implementation.
DOMViewTransition::DOMChangeFinishedCallback::DOMChangeFinishedCallback(
    DOMViewTransition& dom_view_transition,
    bool success)
    : dom_view_transition_(&dom_view_transition), success_(success) {}

DOMViewTransition::DOMChangeFinishedCallback::~DOMChangeFinishedCallback() =
    default;

ScriptValue DOMViewTransition::DOMChangeFinishedCallback::Call(
    ScriptState*,
    ScriptValue value) {
  dom_view_transition_->NotifyDOMCallbackFinished(success_, std::move(value));
  return ScriptValue();
}

void DOMViewTransition::DOMChangeFinishedCallback::Trace(
    Visitor* visitor) const {
  ScriptFunction::Callable::Trace(visitor);
  visitor->Trace(dom_view_transition_);
}

}  // namespace blink
