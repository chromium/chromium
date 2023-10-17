// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/view_transition/dom_view_transition.h"

#include "third_party/blink/renderer/bindings/core/v8/capture_source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_property.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
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

DOMViewTransition::DOMViewTransition(
    ExecutionContext& execution_context,
    ViewTransition& view_transition,
    ScriptState& script_state,
    V8ViewTransitionCallback* update_dom_callback)
    : ActiveScriptWrappable<DOMViewTransition>({}),
      execution_context_(&execution_context),
      view_transition_{&view_transition},
      script_state_(&script_state),
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

void DOMViewTransition::skipTransition() {
  view_transition_->SkipTransition();
}

ScriptPromise DOMViewTransition::finished() const {
  return finished_promise_property_->Promise(script_state_->World());
}

ScriptPromise DOMViewTransition::ready() const {
  return ready_promise_property_->Promise(script_state_->World());
}

ScriptPromise DOMViewTransition::updateCallbackDone() const {
  return dom_updated_promise_property_->Promise(script_state_->World());
}

void DOMViewTransition::DidSkipTransition(
    ViewTransition::PromiseResponse response) {
  CHECK_NE(response, ViewTransition::PromiseResponse::kResolve);

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
  dom_callback_result_ = DOMCallbackResult::kRunning;

  v8::Maybe<ScriptPromise> result = v8::Nothing<ScriptPromise>();
  ScriptState* script_state = nullptr;

  if (update_dom_callback_) {
    script_state = update_dom_callback_->CallbackRelevantScriptState();
    result = update_dom_callback_->Invoke(nullptr);

    // If the callback couldn't be run for some reason, treat it as an empty
    // promise rejected with an abort exception.
    if (result.IsNothing()) {
      auto value = ScriptValue::From(
          script_state, MakeGarbageCollected<DOMException>(
                            DOMExceptionCode::kAbortError, kAbortedMessage));
      result = v8::Just(ScriptPromise::Reject(script_state, value));
    }
  } else {
    // It's ok to use the main world here since we're only using it to call
    // DOMChangeFinishedCallback which doesn't use the script state or execute
    // any script.
    script_state =
        ToScriptState(execution_context_, DOMWrapperWorld::MainWorld());
    ScriptState::Scope scope(script_state);

    // If there's no callback provided, treat the same as an empty promise
    // resolved without a value.
    result = v8::Just(ScriptPromise::CastUndefined(script_state));
  }

  // Note, the DOMChangeFinishedCallback will be invoked asynchronously.
  ScriptState::Scope scope(script_state);
  result.ToChecked().Then(
      MakeGarbageCollected<ScriptFunction>(
          script_state_,
          MakeGarbageCollected<DOMChangeFinishedCallback>(*this, true)),
      MakeGarbageCollected<ScriptFunction>(
          script_state_,
          MakeGarbageCollected<DOMChangeFinishedCallback>(*this, false)));
}

bool DOMViewTransition::HasPendingActivity() const {
  return !view_transition_->IsDone();
}

void DOMViewTransition::Trace(Visitor* visitor) const {
  visitor->Trace(execution_context_);
  visitor->Trace(view_transition_);
  visitor->Trace(script_state_);
  visitor->Trace(update_dom_callback_);
  visitor->Trace(finished_promise_property_);
  visitor->Trace(ready_promise_property_);
  visitor->Trace(dom_updated_promise_property_);

  ScriptWrappable::Trace(visitor);
}

void DOMViewTransition::AtMicrotask(ViewTransition::PromiseResponse response,
                                    PromiseProperty* property) {
  execution_context_->GetAgent()->event_loop()->EnqueueMicrotask(
      WTF::BindOnce(&DOMViewTransition::HandlePromise, WrapPersistent(this),
                    response, WrapPersistent(property)));
}

void DOMViewTransition::HandlePromise(ViewTransition::PromiseResponse response,
                                      PromiseProperty* property) {
  DCHECK_EQ(property->GetState(), PromiseProperty::State::kPending);
  if (!script_state_->ContextIsValid()) {
    return;
  }

  switch (response) {
    case ViewTransition::PromiseResponse::kResolve:
      property->ResolveWithUndefined();
      break;
    case ViewTransition::PromiseResponse::kRejectAbort: {
      ScriptState::Scope scope(script_state_);
      auto value = ScriptValue::From(
          script_state_, MakeGarbageCollected<DOMException>(
                             DOMExceptionCode::kAbortError, kAbortedMessage));
      property->Reject(value);
      break;
    }
    case ViewTransition::PromiseResponse::kRejectInvalidState: {
      ScriptState::Scope scope(script_state_);
      auto value = ScriptValue::From(
          script_state_,
          MakeGarbageCollected<DOMException>(
              DOMExceptionCode::kInvalidStateError, kInvalidStateMessage));
      property->Reject(value);
      break;
    }
    case ViewTransition::PromiseResponse::kRejectTimeout: {
      ScriptState::Scope scope(script_state_);
      auto value = ScriptValue::From(
          script_state_, MakeGarbageCollected<DOMException>(
                             DOMExceptionCode::kTimeoutError, kTimeoutMessage));
      property->Reject(value);
      break;
    }
  }
}

// DOMChangeFinishedCallback implementation.
DOMViewTransition::DOMChangeFinishedCallback::DOMChangeFinishedCallback(
    DOMViewTransition& dom_view_transition,
    bool success)
    : dom_view_transition_(&dom_view_transition), success_(success) {}

DOMViewTransition::DOMChangeFinishedCallback::~DOMChangeFinishedCallback() =
    default;

ScriptValue DOMViewTransition::DOMChangeFinishedCallback::Call(
    ScriptState* script_state,
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
