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

#include "third_party/blink/renderer/bindings/core/v8/scheduled_action.h"

#include "third_party/blink/renderer/bindings/core/v8/binding_security.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

ScheduledAction* ScheduledAction::Create(ScriptState* script_state,
                                         ExecutionContext* target,
                                         const ScriptValue& handler,
                                         const Vector<ScriptValue>& arguments) {
  DCHECK(handler.IsFunction());
  if (!script_state->World().IsWorkerWorld()) {
    if (!BindingSecurity::ShouldAllowAccessToFrame(
            EnteredDOMWindow(script_state->GetIsolate()),
            To<Document>(target)->GetFrame(),
            BindingSecurity::ErrorReportOption::kDoNotReport)) {
      UseCounter::Count(target, WebFeature::kScheduledActionIgnored);
      return new ScheduledAction(script_state);
    }
  }
  return new ScheduledAction(script_state, handler, arguments);
}

ScheduledAction* ScheduledAction::Create(ScriptState* script_state,
                                         ExecutionContext* target,
                                         const String& handler) {
  if (!script_state->World().IsWorkerWorld()) {
    if (!BindingSecurity::ShouldAllowAccessToFrame(
            EnteredDOMWindow(script_state->GetIsolate()),
            To<Document>(target)->GetFrame(),
            BindingSecurity::ErrorReportOption::kDoNotReport)) {
      UseCounter::Count(target, WebFeature::kScheduledActionIgnored);
      return new ScheduledAction(script_state);
    }
  }
  return new ScheduledAction(script_state, handler);
}

ScheduledAction::~ScheduledAction() {
  // Verify that owning DOMTimer has eagerly disposed.
  DCHECK(info_.IsEmpty());
}

void ScheduledAction::Dispose() {
  code_ = String();
  info_.Clear();
  function_.Clear();
  script_state_->Reset();
  script_state_.Clear();
}

void ScheduledAction::Trace(blink::Visitor* visitor) {
  visitor->Trace(script_state_);
}

void ScheduledAction::Execute(ExecutionContext* context) {
  if (!script_state_->ContextIsValid()) {
    DVLOG(1) << "ScheduledAction::execute " << this << ": context is empty";
    return;
  }
  // ExecutionContext::CanExecuteScripts() relies on the current context to
  // determine if it is allowed. Enter the scope here.
  ScriptState::Scope scope(script_state_->Get());

  if (auto* document = DynamicTo<Document>(context)) {
    LocalFrame* frame = document->GetFrame();
    if (!frame) {
      DVLOG(1) << "ScheduledAction::execute " << this << ": no frame";
      return;
    }
    if (!context->CanExecuteScripts(kAboutToExecuteScript)) {
      DVLOG(1) << "ScheduledAction::execute " << this
               << ": frame can not execute scripts";
      return;
    }
    Execute(frame);
  } else {
    DVLOG(1) << "ScheduledAction::execute " << this << ": worker scope";
    Execute(To<WorkerGlobalScope>(context));
  }
}

ScheduledAction::ScheduledAction(ScriptState* script_state,
                                 const ScriptValue& function,
                                 const Vector<ScriptValue>& arguments)
    : ScheduledAction(script_state) {
  DCHECK(function.IsFunction());
  function_.Set(script_state->GetIsolate(),
                v8::Local<v8::Function>::Cast(function.V8Value()));
  info_.ReserveCapacity(arguments.size());
  for (const ScriptValue& argument : arguments)
    info_.Append(argument.V8Value());
}

ScheduledAction::ScheduledAction(ScriptState* script_state, const String& code)
    : ScheduledAction(script_state) {
  code_ = code;
}

ScheduledAction::ScheduledAction(ScriptState* script_state)
    : script_state_(ScriptStateProtectingContext::Create(script_state)),
      info_(script_state->GetIsolate()) {}

void ScheduledAction::Execute(LocalFrame* frame) {
  DCHECK(script_state_->ContextIsValid());

  TRACE_EVENT0("v8", "ScheduledAction::execute");
  if (!function_.IsEmpty()) {
    DVLOG(1) << "ScheduledAction::execute " << this << ": have function";
    v8::Local<v8::Function> function =
        function_.NewLocal(script_state_->GetIsolate());
    ScriptState* script_state_for_func =
        ScriptState::From(function->CreationContext());
    if (!script_state_for_func->ContextIsValid()) {
      DVLOG(1) << "ScheduledAction::execute " << this
               << ": function's context is empty";
      return;
    }
    Vector<v8::Local<v8::Value>> info;
    CreateLocalHandlesForArgs(&info);
    V8ScriptRunner::CallFunction(
        function, frame->GetDocument(), script_state_->GetContext()->Global(),
        info.size(), info.data(), script_state_->GetIsolate());
  } else {
    DVLOG(1) << "ScheduledAction::execute " << this
             << ": executing from source";
    // We're using |kSharableCrossOrigin| to keep the existing behavior, but
    // this causes failures on
    // wpt/html/webappapis/scripting/processing-model-2/compile-error-cross-origin-setTimeout.html
    // and friends.
    frame->GetScriptController().ExecuteScriptAndReturnValue(
        script_state_->GetContext(),
        ScriptSourceCode(code_,
                         ScriptSourceLocationType::kEvalForScheduledAction),
        KURL(), kSharableCrossOrigin);
  }

  // The frame might be invalid at this point because JavaScript could have
  // released it.
}

void ScheduledAction::Execute(WorkerGlobalScope* worker) {
  DCHECK(worker->GetThread()->IsCurrentThread());

  if (!script_state_->ContextIsValid()) {
    DVLOG(1) << "ScheduledAction::execute " << this << ": context is empty";
    return;
  }

  if (!function_.IsEmpty()) {
    ScriptState::Scope scope(script_state_->Get());
    v8::Local<v8::Function> function =
        function_.NewLocal(script_state_->GetIsolate());
    ScriptState* script_state_for_func =
        ScriptState::From(function->CreationContext());
    if (!script_state_for_func->ContextIsValid()) {
      DVLOG(1) << "ScheduledAction::execute " << this
               << ": function's context is empty";
      return;
    }
    Vector<v8::Local<v8::Value>> info;
    CreateLocalHandlesForArgs(&info);
    V8ScriptRunner::CallFunction(
        function, worker, script_state_->GetContext()->Global(), info.size(),
        info.data(), script_state_->GetIsolate());
  } else {
    // We're using |kSharableCrossOrigin| to keep the existing behavior, but
    // this causes failures on
    // wpt/html/webappapis/scripting/processing-model-2/compile-error-cross-origin-setTimeout.html
    // and friends.
    worker->ScriptController()->Evaluate(
        ScriptSourceCode(code_,
                         ScriptSourceLocationType::kEvalForScheduledAction),
        kSharableCrossOrigin);
  }
}

void ScheduledAction::CreateLocalHandlesForArgs(
    Vector<v8::Local<v8::Value>>* handles) {
  wtf_size_t handle_count = SafeCast<wtf_size_t>(info_.Size());
  handles->ReserveCapacity(handle_count);
  for (wtf_size_t i = 0; i < handle_count; ++i)
    handles->push_back(info_.Get(i));
}

}  // namespace blink
