// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/idle/idle_detector.h"

#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/idle/idle_manager.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_idle_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_screen_idle_state.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_user_idle_state.h"
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

class IdleDetector::StartAbortAlgorithm final : public AbortSignal::Algorithm {
 public:
  explicit StartAbortAlgorithm(IdleDetector* idle_detector)
      : idle_detector_(idle_detector) {}
  ~StartAbortAlgorithm() override = default;

  void Run() override { idle_detector_->Abort(); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(idle_detector_);
    Algorithm::Trace(visitor);
  }

 private:
  Member<IdleDetector> idle_detector_;
};

IdleDetector* IdleDetector::Create(ScriptState* script_state) {
  return MakeGarbageCollected<IdleDetector>(
      ExecutionContext::From(script_state));
}

IdleDetector::IdleDetector(ExecutionContext* context)
    : ActiveScriptWrappable<IdleDetector>({}),
      ExecutionContextLifecycleObserver(context),
      task_runner_(context->GetTaskRunner(TaskType::kMiscPlatformAPI)),
      timer_(task_runner_, this, &IdleDetector::DispatchUserIdleEvent),
      receiver_(this, context) {}

IdleDetector::~IdleDetector() = default;

const AtomicString& IdleDetector::InterfaceName() const {
  return event_target_names::kIdleDetector;
}

ExecutionContext* IdleDetector::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

bool IdleDetector::HasPendingActivity() const {
  // This object should be considered active as long as there are registered
  // event listeners.
  return GetExecutionContext() && HasEventListeners();
}

std::optional<V8UserIdleState> IdleDetector::userState() const {
  if (!has_state_) {
    return std::nullopt;
  }

  return user_idle_ ? V8UserIdleState(V8UserIdleState::Enum::kIdle)
                    : V8UserIdleState(V8UserIdleState::Enum::kActive);
}

std::optional<V8ScreenIdleState> IdleDetector::screenState() const {
  if (!has_state_) {
    return std::nullopt;
  }

  return screen_locked_ ? V8ScreenIdleState(V8ScreenIdleState::Enum::kLocked)
                        : V8ScreenIdleState(V8ScreenIdleState::Enum::kUnlocked);
}

// static
ScriptPromise<V8PermissionState> IdleDetector::requestPermission(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Execution context is detached.");
    return EmptyPromise();
  }

  auto* context = ExecutionContext::From(script_state);
  return IdleManager::From(context)->RequestPermission(script_state,
                                                       exception_state);
}

ScriptPromise<IDLUndefined> IdleDetector::start(
    ScriptState* script_state,
    const IdleOptions* options,
    ExceptionState& exception_state) {
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Execution context is detached.");
    return EmptyPromise();
  }

  ExecutionContext* context = ExecutionContext::From(script_state);
  DCHECK(context->IsContextThread());

  if (!context->IsFeatureEnabled(
          mojom::blink::PermissionsPolicyFeature::kIdleDetection,
          ReportOptions::kReportOnFailure)) {
    exception_state.ThrowSecurityError(kFeaturePolicyBlocked);
    return EmptyPromise();
  }

  if (receiver_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Idle detector is already started.");
    return EmptyPromise();
  }

  if (options->hasThreshold()) {
    auto threshold = base::Milliseconds(options->threshold());
    if (threshold < kMinimumThreshold) {
      exception_state.ThrowTypeError("Minimum threshold is 1 minute.");
      return EmptyPromise();
    }
    threshold_ = threshold;
  }

  signal_ = options->getSignalOr(nullptr);
  if (signal_) {
    if (signal_->aborted()) {
      return ScriptPromise<IDLUndefined>::Reject(script_state,
                                                 signal_->reason(script_state));
    }
    // If there was a previous algorithm, it should have been removed when we
    // reached the "stopped" state.
    DCHECK(!abort_handle_);
    abort_handle_ =
        signal_->AddAlgorithm(MakeGarbageCollected<StartAbortAlgorithm>(this));
  }

  mojo::PendingRemote<mojom::blink::IdleMonitor> remote;
  receiver_.Bind(remote.InitWithNewPipeAndPassReceiver(), task_runner_);
  receiver_.set_disconnect_handler(WTF::BindOnce(
      &IdleDetector::OnMonitorDisconnected, WrapWeakPersistent(this)));

  resolver_ = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto promise = resolver_->Promise();
  IdleManager::From(context)->AddMonitor(
      std::move(remote),
      WTF::BindOnce(&IdleDetector::OnAddMonitor, WrapWeakPersistent(this),
                    WrapPersistent(resolver_.Get())));
  return promise;
}

void IdleDetector::SetTaskRunnerForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::TickClock* tick_clock) {
  task_runner_ = std::move(task_runner);
  timer_.SetTaskRunnerForTesting(task_runner_, tick_clock);
}

void IdleDetector::Abort() {
  if (resolver_) {
    ScriptState* script_state = resolver_->GetScriptState();
    if (IsInParallelAlgorithmRunnable(resolver_->GetExecutionContext(),
                                      script_state)) {
      ScriptState::Scope script_state_scope(script_state);
      resolver_->Reject(signal_->reason(script_state));
    }
  }
  Clear();
}

void IdleDetector::OnMonitorDisconnected() {
  ScriptState* resolver_script_state(nullptr);

  if (resolver_ && (resolver_script_state = resolver_->GetScriptState()) &&
      IsInParallelAlgorithmRunnable(resolver_->GetExecutionContext(),
                                    resolver_script_state)) {
    ScriptState::Scope script_state_scope(resolver_->GetScriptState());
    resolver_->Reject(V8ThrowDOMException::CreateOrDie(
        resolver_->GetScriptState()->GetIsolate(),
        DOMExceptionCode::kNotSupportedError, "Idle detection not available."));
  }
  Clear();
}

void IdleDetector::OnAddMonitor(ScriptPromiseResolver<IDLUndefined>* resolver,
                                IdleManagerError error,
                                mojom::blink::IdleStatePtr state) {
  if (resolver_ != resolver) {
    // Starting the detector was aborted so `resolver_` has already been used
    // and `receiver_` has already been reset.
    return;
  }

  ScriptState* resolver_script_state = resolver_->GetScriptState();
  if (!IsInParallelAlgorithmRunnable(resolver_->GetExecutionContext(),
                                     resolver_script_state)) {
    resolver_ = nullptr;
    return;
  }
  ScriptState::Scope script_state_scope(resolver_script_state);

  switch (error) {
    case IdleManagerError::kPermissionDisabled:
      resolver_->Reject(
          V8ThrowDOMException::CreateOrDie(resolver_script_state->GetIsolate(),
                                           DOMExceptionCode::kNotAllowedError,
                                           "Idle detection permission denied"));
      resolver_ = nullptr;
      break;
    case IdleManagerError::kSuccess:
      DCHECK(state);
      resolver_->Resolve();
      resolver_ = nullptr;

      // This call may execute script if it dispatches an event.
      Update(std::move(state), /*is_overridden_by_devtools=*/false);
      break;
  }
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
  visitor->Trace(abort_handle_);
  visitor->Trace(resolver_);
  visitor->Trace(receiver_);
  EventTarget::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  ActiveScriptWrappable::Trace(visitor);
}

void IdleDetector::ContextDestroyed() {
  Clear();
}

void IdleDetector::Clear() {
  if (abort_handle_) {
    CHECK(signal_);
    signal_->RemoveAlgorithm(abort_handle_);
  }
  resolver_ = nullptr;
  abort_handle_ = nullptr;
  has_state_ = false;
  receiver_.reset();
}

}  // namespace blink
