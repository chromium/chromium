// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/compute_pressure/compute_pressure_observer.h"

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/compute_pressure/compute_pressure.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_compute_pressure_observer_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_compute_pressure_observer_update.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

ComputePressureObserver::ComputePressureObserver(
    ExecutionContext* execution_context,
    V8ComputePressureUpdateCallback* observer_callback,
    ComputePressureObserverOptions* normalized_options)
    : ExecutionContextLifecycleStateObserver(execution_context),
      observer_callback_(observer_callback),
      normalized_options_(normalized_options),
      compute_pressure_host_(execution_context),
      receiver_(this, execution_context) {
  execution_context->GetBrowserInterfaceBroker().GetInterface(
      compute_pressure_host_.BindNewPipeAndPassReceiver(
          execution_context->GetTaskRunner(TaskType::kUserInteraction)));
  // ExecutionContextLifecycleStateObserver.
  UpdateStateIfNeeded();
}

ComputePressureObserver::~ComputePressureObserver() = default;

namespace {

// Validates a sorted array that specifies a quantization scheme.
//
// Returns false if the array is not a valid quantization scheme.
// `exception_state` is populated in this case.
bool ValidateThresholds(const Vector<double>& thresholds,
                        ExceptionState& exception_state) {
  double previous_threshold = 0.0;

  for (double threshold : thresholds) {
    if (threshold <= 0.0) {
      exception_state.ThrowTypeError("Thresholds must be greater than 0.0");
      return false;
    }

    if (threshold >= 1.0) {
      exception_state.ThrowTypeError("Thresholds must be less than 1.0");
      return false;
    }

    DCHECK_GE(threshold, previous_threshold) << "the thresholds are not sorted";
    if (threshold == previous_threshold) {
      exception_state.ThrowTypeError("Thresholds must be different");
      return false;
    }
    previous_threshold = threshold;
  }
  return true;
}

bool NormalizeObserverOptions(ComputePressureObserverOptions& options,
                              ExceptionState& exception_state) {
  Vector<double> cpu_utilization_thresholds =
      options.cpuUtilizationThresholds();
  if (cpu_utilization_thresholds.size() >
      mojom::blink::kMaxComputePressureCpuUtilizationThresholds) {
    cpu_utilization_thresholds.resize(
        mojom::blink::kMaxComputePressureCpuUtilizationThresholds);
  }
  std::sort(cpu_utilization_thresholds.begin(),
            cpu_utilization_thresholds.end());
  if (!ValidateThresholds(cpu_utilization_thresholds, exception_state)) {
    DCHECK(exception_state.HadException());
    return false;
  }
  options.setCpuUtilizationThresholds(cpu_utilization_thresholds);

  Vector<double> cpu_speed_thresholds = options.cpuSpeedThresholds();
  if (cpu_speed_thresholds.size() >
      mojom::blink::kMaxComputePressureCpuSpeedThresholds) {
    cpu_speed_thresholds.resize(
        mojom::blink::kMaxComputePressureCpuSpeedThresholds);
  }
  std::sort(cpu_speed_thresholds.begin(), cpu_speed_thresholds.end());
  if (!ValidateThresholds(cpu_speed_thresholds, exception_state)) {
    DCHECK(exception_state.HadException());
    return false;
  }
  options.setCpuSpeedThresholds(cpu_speed_thresholds);

  return true;
}

}  // namespace

// static
ComputePressureObserver* ComputePressureObserver::Create(
    ScriptState* script_state,
    V8ComputePressureUpdateCallback* callback,
    ComputePressureObserverOptions* options,
    ExceptionState& exception_state) {
  if (!NormalizeObserverOptions(*options, exception_state)) {
    DCHECK(exception_state.HadException());
    return nullptr;
  }

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  return MakeGarbageCollected<ComputePressureObserver>(execution_context,
                                                       callback, options);
}

ScriptPromise ComputePressureObserver::observe(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!compute_pressure_host_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "ComputePressureHost backend went away");
    return ScriptPromise();
  }

  if (receiver_.is_bound())
    return ScriptPromise::CastUndefined(script_state);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      ExecutionContext::From(script_state)
          ->GetTaskRunner(TaskType::kMiscPlatformAPI);

  auto mojo_options = mojom::blink::ComputePressureQuantization::New(
      normalized_options_->cpuUtilizationThresholds(),
      normalized_options_->cpuSpeedThresholds());

  compute_pressure_host_->AddObserver(
      receiver_.BindNewPipeAndPassRemote(std::move(task_runner)),
      std::move(mojo_options),
      WTF::Bind(&ComputePressureObserver::DidAddObserver,
                WrapWeakPersistent(this), WrapPersistent(resolver)));
  receiver_.set_disconnect_handler(
      WTF::Bind(&ComputePressureObserver::OnReceiverDisconnect,
                WrapWeakPersistent(this)));
  return resolver->Promise();
}

void ComputePressureObserver::stop(ScriptState* script_state) {
  receiver_.reset();
  return;
}

void ComputePressureObserver::Trace(blink::Visitor* visitor) const {
  visitor->Trace(observer_callback_);
  visitor->Trace(normalized_options_);
  visitor->Trace(compute_pressure_host_);
  visitor->Trace(receiver_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleStateObserver::Trace(visitor);
}

void ComputePressureObserver::OnUpdate(
    mojom::blink::ComputePressureStatePtr state) {
  auto* update = ComputePressureObserverUpdate::Create();
  update->setCpuUtilization(state->cpu_utilization);
  update->setCpuSpeed(state->cpu_speed);
  update->setOptions(normalized_options_);

  observer_callback_->InvokeAndReportException(this, update);
}

void ComputePressureObserver::ContextDestroyed() {
  receiver_.reset();
}

void ComputePressureObserver::ContextLifecycleStateChanged(
    mojom::blink::FrameLifecycleState state) {
  // TODO(https://crbug.com/1186433): Disconnect and re-establish a connection
  // when frozen or send a disconnect event.
}

void ComputePressureObserver::OnReceiverDisconnect() {
  receiver_.reset();
}

void ComputePressureObserver::DidAddObserver(
    ScriptPromiseResolver* resolver,
    mojom::blink::ComputePressureStatus status) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state);

  switch (status) {
    case mojom::blink::ComputePressureStatus::kOk:
      break;
    case mojom::blink::ComputePressureStatus::kNotSupported:
      resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
          script_state->GetIsolate(), DOMExceptionCode::kNotSupportedError,
          "Not available on this platform."));
      return;
    case mojom::blink::ComputePressureStatus::kSecurityError:
      resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
          script_state->GetIsolate(), DOMExceptionCode::kSecurityError,
          "Security error. Make sure the page is visible."));
      return;
  }

  resolver->Resolve();
}

}  // namespace blink
