// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/idle/idle_detector.h"

#include <utility>

#include "base/time/time.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/idle/idle_manager.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_idle_options.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/modules/idle/idle_manager.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

using mojom::blink::IdleManagerError;

const char kAbortMessage[] = "Idle detection aborted.";
const char kFeaturePolicyBlocked[] =
    "Access to the feature \"idle-detection\" is disallowed by permissions "
    "policy.";

constexpr base::TimeDelta kMinimumThreshold = base::Seconds(60);
constexpr base::TimeDelta kUserInputThreshold =
    base::Milliseconds(mojom::blink::IdleManager::kUserInputThresholdMs);

static_assert(
    kMinimumThreshold >= kUserInputThreshold,
    "Browser threshold can't be less than the minimum allowed by the API");

}  // namespace

IdleDetector* IdleDetector::Create(ScriptState* script_state) {
  return MakeGarbageCollected<IdleDetector>(
      ExecutionContext::From(script_state));
}

IdleDetector::IdleDetector(ExecutionContext* context)
    : ExecutionContextClient(context),
      task_runner_(context->GetTaskRunner(TaskType::kMiscPlatformAPI)),
      timer_(task_runner_, this, &IdleDetector::DispatchUserIdleEvent),
      receiver_(this, context) {}

IdleDetector::~IdleDetector() = default;

const AtomicString& IdleDetector::InterfaceName() const {
  return event_target_names::kIdleDetector;
}

ExecutionContext* IdleDetector::GetExecutionContext() const {
  return ExecutionContextClient::GetExecutionContext();
}

bool IdleDetector::HasPendingActivity() const {
  // This object should be considered active as long as there are registered
  // event listeners.
  return GetExecutionContext() && HasEventListeners();
}

String IdleDetector::userState() const {
  if (!has_state_)
    return String();

  return user_idle_ ? "idle" : "active";
}

String IdleDetector::screenState() const {
  if (!has_state_)
    return String();

  return screen_locked_ ? "locked" : "unlocked";
}

// static
ScriptPromise IdleDetector::requestPermission(ScriptState* script_state,
                                              ExceptionState& exception_state) {
  if (!script_state->ContextIsValid())
    return ScriptPromise();

  auto* context = ExecutionContext::From(script_state);
  return IdleManager::From(context)->RequestPermission(script_state,
                                                       exception_state);
}

ScriptPromise IdleDetector::start(ScriptState* script_state,
                                  const IdleOptions* options,
                                  ExceptionState& exception_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  DCHECK(context->IsContextThread());

  if (!context->IsFeatureEnabled(
          mojom::blink::PermissionsPolicyFeature::kIdleDetection,
          ReportOptions::kReportOnFailure)) {
    exception_state.ThrowSecurityError(kFeaturePolicyBlocked);
    return ScriptPromise();
  }

  if (receiver_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Idle detector is already started.");
    return ScriptPromise();
  }

  if (options->hasThreshold()) {
    auto threshold = base::Milliseconds(options->threshold());
    if (threshold < kMinimumThreshold) {
      exception_state.ThrowTypeError("Minimum threshold is 1 minute.");
      return ScriptPromise();
    }
    threshold_ = threshold;
  }

  if (options->hasSignal()) {
    signal_ = options->signal();
    signal_->AddAlgorithm(WTF::Bind(&IdleDetector::Abort,
                                    WrapWeakPersistent(this),
                                    WrapWeakPersistent(signal_.Get())));
  }

  if (signal_ && signal_->aborted()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kAbortError,
                                      kAbortMessage);
    return ScriptPromise();
  }

  mojo::PendingRemote<mojom::blink::IdleMonitor> remote;
  receiver_.Bind(remote.InitWithNewPipeAndPassReceiver(), task_runner_);
  receiver_.set_disconnect_handler(WTF::Bind(
      &IdleDetector::OnMonitorDisconnected, WrapWeakPersistent(this)));

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  IdleManager::From(context)->AddMonitor(
      std::move(remote),
      WTF::Bind(&IdleDetector::OnAddMonitor, WrapWeakPersistent(this),
                WrapPersistent(resolver)));
  return promise;
}

void IdleDetector::SetTaskRunnerForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  task_runner_ = std::move(task_runner);
  timer_.MoveToNewTaskRunner(task_runner_);
}

void IdleDetector::Abort(AbortSignal* signal) {
  // There is no RemoveAlgorithm() method on AbortSignal so compare the signal
  // bound to this callback to the one last passed to start().
  if (signal_ != signal)
    return;

  if (resolver_) {
    resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kAbortError, kAbortMessage));
    resolver_ = nullptr;
  }

  has_state_ = false;
  receiver_.reset();
}

void IdleDetector::OnMonitorDisconnected() {
  if (resolver_) {
    resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError, "Idle detection not available."));
    resolver_ = nullptr;
  }

  has_state_ = false;
  receiver_.reset();
}

void IdleDetector::OnAddMonitor(ScriptPromiseResolver* resolver,
                                IdleManagerError error,
                                mojom::blink::IdleStatePtr state) {
  switch (error) {
    case IdleManagerError::kPermissionDisabled:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError,
          "Idle detection permission denied"));
      break;
    case IdleManagerError::kSuccess:
      DCHECK(state);
      resolver->Resolve();
      Update(std::move(state), /*is_overridden_by_devtools=*/false);
      break;
  }

  resolver_ = nullptr;
}

void IdleDetector::Update(mojom::blink::IdleStatePtr state,
                          bool is_overridden_by_devtools) {
  DCHECK(receiver_.is_bound());
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed())
    return;

  bool fire_event = false;
  if (!has_state_) {
    has_state_ = true;
    fire_event = true;
  }

  if (state->screen_locked != screen_locked_) {
    screen_locked_ = state->screen_locked;
    fire_event = true;
  }

  if (state->idle_time.has_value()) {
    DCHECK_GE(threshold_, kUserInputThreshold);
    if (!is_overridden_by_devtools &&
        threshold_ > kUserInputThreshold + *state->idle_time) {
      base::TimeDelta delay =
          threshold_ - kUserInputThreshold - *state->idle_time;
      timer_.StartOneShot(delay, FROM_HERE);

      // Normally this condition is unsatisfiable because state->idle_time
      // cannot move backwards but it can if the state was previously overridden
      // by DevTools.
      if (user_idle_) {
        user_idle_ = false;
        fire_event = true;
      }
    } else if (!user_idle_) {
      user_idle_ = true;
      fire_event = true;
    }
  } else {
    // The user is now active, so cancel any scheduled task to notify script
    // that the user is idle.
    timer_.Stop();

    if (user_idle_) {
      user_idle_ = false;
      fire_event = true;
    }
  }

  if (fire_event) {
    DispatchEvent(*Event::Create(event_type_names::kChange));
  }
}

void IdleDetector::DispatchUserIdleEvent(TimerBase*) {
  user_idle_ = true;
  DispatchEvent(*Event::Create(event_type_names::kChange));
}

void IdleDetector::Trace(Visitor* visitor) const {
  visitor->Trace(timer_);
  visitor->Trace(signal_);
  visitor->Trace(resolver_);
  visitor->Trace(receiver_);
  EventTargetWithInlineData::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  ActiveScriptWrappable::Trace(visitor);
}

}  // namespace blink
