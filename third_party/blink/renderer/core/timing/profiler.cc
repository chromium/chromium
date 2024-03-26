// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/profiler.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/profiler_group.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"

namespace blink {

Profiler* Profiler::Create(ScriptState* script_state,
                           const ProfilerInitOptions* options,
                           ExceptionState& exception_state) {
  auto* execution_context = ExecutionContext::From(script_state);
  DCHECK(execution_context);

  Performance* performance = nullptr;
  bool can_profile = false;
  if (LocalDOMWindow* window = LocalDOMWindow::From(script_state)) {
    can_profile = ProfilerGroup::CanProfile(window, &exception_state,
                                            ReportOptions::kReportOnFailure);
    performance = DOMWindowPerformance::performance(*window);
  }

  if (!can_profile) {
    DCHECK(exception_state.HadException());
    return nullptr;
  }

  DCHECK(performance);

  auto* profiler_group = ProfilerGroup::From(script_state->GetIsolate());
  DCHECK(profiler_group);

  auto* profiler = profiler_group->CreateProfiler(
      script_state, *options, performance->GetTimeOriginInternal(),
      exception_state);
  if (exception_state.HadException())
    return nullptr;

  return profiler;
}

void Profiler::Trace(Visitor* visitor) const {
  visitor->Trace(profiler_group_);
  visitor->Trace(script_state_);
  EventTarget::Trace(visitor);
}

void Profiler::DisposeAsync() {
  if (profiler_group_) {
    // It's safe to touch |profiler_group_| in Profiler's pre-finalizer as
    // |profiler_group_| is guaranteed to outlive the Profiler, if set. This is
    // due to ProfilerGroup nulling out this field for all attached Profilers
    // prior to destruction.
    profiler_group_->CancelProfilerAsync(script_state_, this);
    profiler_group_ = nullptr;
  }
}

const AtomicString& Profiler::InterfaceName() const {
  return event_target_names::kProfiler;
}

ExecutionContext* Profiler::GetExecutionContext() const {
  return ExecutionContext::From(script_state_);
}

ScriptPromise<ProfilerTrace> Profiler::stop(ScriptState* script_state) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<ProfilerTrace>>(script_state);
  auto promise = resolver->Promise();

  if (!stopped()) {
    // Ensure that we don't synchronously invoke script when resolving
    // (crbug.com/1119865).
    ScriptForbiddenScope forbid_script;
    DCHECK(profiler_group_);
    profiler_group_->StopProfiler(script_state, this, resolver);
    profiler_group_ = nullptr;
  } else {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, "Profiler already stopped."));
  }

  return promise;
}

}  // namespace blink
