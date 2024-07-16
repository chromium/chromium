/*
 * Copyright (C) 2007-2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/scheduler/scheduled_action.h"

#include <optional>
#include <tuple>

#include "base/trace_event/trace_event.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/binding_security.h"
#include "third_party/blink/renderer/bindings/core/v8/script_evaluation_result.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_function.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

ScheduledAction::ScheduledAction(ScriptState* script_state,
                                 ExecutionContext& target,
                                 V8Function* handler,
                                 const HeapVector<ScriptValue>& arguments)
    : script_state_(
          MakeGarbageCollected<ScriptStateProtectingContext>(script_state)) {
  if (script_state->World().IsWorkerOrWorkletWorld() ||
      BindingSecurity::ShouldAllowAccessTo(
          EnteredDOMWindow(script_state->GetIsolate()),
          To<LocalDOMWindow>(&target))) {
    function_ = handler;
    arguments_ = arguments;
    auto* tracker =
        scheduler::TaskAttributionTracker::From(script_state->GetIsolate());
    if (tracker && script_state->World().IsMainWorld()) {
      function_->SetParentTask(tracker->RunningTask());
    }
  } else {
    UseCounter::Count(target, WebFeature::kScheduledActionIgnored);
  }
}

ScheduledAction::ScheduledAction(ScriptState* script_state,
                                 ExecutionContext& target,
                                 const String& handler)
    : script_state_(
          MakeGarbageCollected<ScriptStateProtectingContext>(script_state)) {
  if (script_state->World().IsWorkerOrWorkletWorld() ||
      BindingSecurity::ShouldAllowAccessTo(
          EnteredDOMWindow(script_state->GetIsolate()),
          To<LocalDOMWindow>(&target))) {
    code_ = handler;
    auto* tracker =
        scheduler::TaskAttributionTracker::From(script_state->GetIsolate());
    if (tracker && script_state->World().IsMainWorld()) {
      code_parent_task_ = tracker->RunningTask();
    }
  } else {
    UseCounter::Count(target, WebFeature::kScheduledActionIgnored);
  }
}

ScheduledAction::~ScheduledAction() {
  // Verify that owning DOMTimer has eagerly disposed.
  DCHECK(!script_state_);
  DCHECK(!function_);
  DCHECK(arguments_.empty());
  DCHECK(code_.IsNull());
}

void ScheduledAction::Dispose() {
  script_state_->Reset();
  script_state_.Clear();
  if (function_) {
    // setTimeout is pretty common and heavily used, and we need a special
    // optimization to let V8 Scavenger GC collect the function object as
    // soon as possible in order to reduce the memory usage.
    // See also https://crbug.com/919474 and https://crbug.com/919475 .
    //
    // This optimization is safe because this ScheduledAction *owns* |function_|
    // (i.e. no other objects reference |function_|) and this ScheduledAction
    // immediately discards |function_| (so never uses it).
    function_->DisposeV8FunctionImmediatelyToReduceMemoryFootprint();
    function_.Clear();
  }
  arguments_.clear();
  code_ = String();
}

void ScheduledAction::Execute(ExecutionContext* context) {
  if (!script_state_->ContextIsValid()) {
    DVLOG(1) << "ScheduledAction::execute " << this << ": context is empty";
    return;
  }
  ScriptState* script_state = script_state_->Get();

  {
    // ExecutionContext::CanExecuteScripts() relies on the current context to
    // determine if it is allowed. Enter the scope here.
    // TODO(crbug.com/1151165): Consider merging CanExecuteScripts() calls,
    // because once crbug.com/1111134 is done, CanExecuteScripts() will be
    // always called below inside
    // - InvokeAndReportException() => V8Function::Invoke() =>
    //   IsCallbackFunctionRunnable() and
    // - V8ScriptRunner::CompileAndRunScript().
    ScriptState::Scope scope(script_state);
    if (!context->CanExecuteScripts(kAboutToExecuteScript)) {
      DVLOG(1) << "ScheduledAction::execute " << this
               << ": window can not execute scripts";
      return;
    }

    // https://html.spec.whatwg.org/C/#timer-initialisation-steps
    if (function_) {
      DVLOG(1) << "ScheduledAction::execute " << this << ": have function";
      function_->InvokeAndReportException(context->ToScriptWrappable(),
                                          arguments_);
      return;
    }

    // We exit the scope here, because we enter v8::Context during the main
    // evaluation below.
  }

  // We create a TaskScope, to ensure code strings passed to ScheduledAction
  // APIs properly track their ancestor as the registering task.
  std::optional<scheduler::TaskAttributionTracker::TaskScope>
      task_attribution_scope;
  auto* tracker =
      scheduler::TaskAttributionTracker::From(script_state->GetIsolate());
  if (tracker && script_state->World().IsMainWorld()) {
    task_attribution_scope = tracker->CreateTaskScope(
        script_state, code_parent_task_,
        scheduler::TaskAttributionTracker::TaskScopeType::kScheduledAction);
  }

  // We use |SanitizeScriptErrors::kDoNotSanitize| because muted errors flag is
  // not set in https://html.spec.whatwg.org/C/#timer-initialisation-steps
  // TODO(crbug.com/1133238): Plumb base URL etc. from the initializing script.
  DVLOG(1) << "ScheduledAction::execute " << this << ": executing from source";
  ClassicScript* script =
      ClassicScript::Create(code_, KURL(), KURL(), ScriptFetchOptions(),
                            ScriptSourceLocationType::kEvalForScheduledAction,
                            SanitizeScriptErrors::kDoNotSanitize);
  script->RunScriptOnScriptState(script_state);
}

void ScheduledAction::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  visitor->Trace(function_);
  visitor->Trace(arguments_);
  visitor->Trace(code_parent_task_);
}

CallbackFunctionBase* ScheduledAction::CallbackFunction() {
  return function_.Get();
}

ScriptState* ScheduledAction::GetScriptState() {
  return script_state_->Get();
}

}  // namespace blink
