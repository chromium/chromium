/*
 * Copyright (C) 2009, 2012 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"

#include <memory>

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/referrer_script_info.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_code_cache.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_initializer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/events/error_event.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/inspector/worker_thread_debugger.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/shared_worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/bindings/origin_trial_features.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_wrapper.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/bindings/wrapper_type_info.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "v8/include/v8.h"

namespace blink {

WorkerOrWorkletScriptController::WorkerOrWorkletScriptController(
    WorkerOrWorkletGlobalScope* global_scope,
    v8::Isolate* isolate)
    : global_scope_(global_scope),
      isolate_(isolate),
      rejected_promises_(RejectedPromises::Create()) {
  DCHECK(isolate);
  world_ =
      DOMWrapperWorld::Create(isolate, DOMWrapperWorld::WorldType::kWorker);
}

WorkerOrWorkletScriptController::~WorkerOrWorkletScriptController() {
  DCHECK(!rejected_promises_);
}

void WorkerOrWorkletScriptController::Dispose() {
  rejected_promises_->Dispose();
  rejected_promises_ = nullptr;

  DisposeContextIfNeeded();
  world_->Dispose();
}

void WorkerOrWorkletScriptController::DisposeContextIfNeeded() {
  if (!IsContextInitialized())
    return;

  if (!global_scope_->IsMainThreadWorkletGlobalScope()) {
    ScriptState::Scope scope(script_state_);
    WorkerThreadDebugger* debugger = WorkerThreadDebugger::From(isolate_);
    debugger->ContextWillBeDestroyed(global_scope_->GetThread(),
                                     script_state_->GetContext());
  }

  {
    ScriptState::Scope scope(script_state_);
    v8::Local<v8::Context> context = script_state_->GetContext();
    // After disposing the world, all Blink->V8 references are gone. Blink
    // stand-alone GCs may collect the WorkerOrWorkletGlobalScope because there
    // are no more roots (V8->Blink references that are actually found by
    // iterating Blink->V8 references). Clear the back pointers to avoid
    // referring to cleared memory on the next GC in case the JS wrapper objects
    // survived.
    v8::Local<v8::Object> global_proxy_object = context->Global();
    v8::Local<v8::Object> global_object =
        global_proxy_object->GetPrototype().As<v8::Object>();
    DCHECK(!global_object.IsEmpty());
    V8DOMWrapper::ClearNativeInfo(isolate_, global_object);
    V8DOMWrapper::ClearNativeInfo(isolate_, global_proxy_object);

    // This detaches v8::MicrotaskQueue pointer from v8::Context, so that we can
    // destroy EventLoop safely.
    context->DetachGlobal();
  }

  script_state_->DisposePerContextData();
  script_state_->DissociateContext();
}

void WorkerOrWorkletScriptController::Initialize(const KURL& url_for_debugger) {
  v8::HandleScope handle_scope(isolate_);

  DCHECK(!IsContextInitialized());

  // Create a new v8::Context with the worker/worklet as the global object
  // (aka the inner global).
  auto* script_wrappable = static_cast<ScriptWrappable*>(global_scope_);
  const WrapperTypeInfo* wrapper_type_info =
      script_wrappable->GetWrapperTypeInfo();
  v8::Local<v8::FunctionTemplate> global_interface_template =
      wrapper_type_info->GetV8ClassTemplate(isolate_, *world_)
          .As<v8::FunctionTemplate>();
  DCHECK(!global_interface_template.IsEmpty());
  v8::Local<v8::ObjectTemplate> global_template =
      global_interface_template->InstanceTemplate();
  v8::Local<v8::Context> context;
  {
    // Initialize V8 extensions before creating the context.
    v8::ExtensionConfiguration extension_configuration =
        ScriptController::ExtensionsFor(global_scope_);

    v8::MicrotaskQueue* microtask_queue = global_scope_->GetMicrotaskQueue();

    V8PerIsolateData::UseCounterDisabledScope use_counter_disabled(
        V8PerIsolateData::From(isolate_));
    context = v8::Context::New(isolate_, &extension_configuration,
                               global_template, v8::MaybeLocal<v8::Value>(),
                               v8::DeserializeInternalFieldsCallback(),
                               microtask_queue);
  }
  DCHECK(!context.IsEmpty());

  script_state_ =
      MakeGarbageCollected<ScriptState>(context, world_, global_scope_);

  ScriptState::Scope scope(script_state_);

  // Associate the global proxy object, the global object and the worker
  // instance (C++ object) as follows.
  //
  //   global proxy object <====> worker or worklet instance
  //                               ^
  //                               |
  //   global object       --------+
  //
  // Per HTML spec, there is no corresponding object for workers to WindowProxy.
  // However, V8 always creates the global proxy object, we associate these
  // objects in the same manner as WindowProxy and Window.
  //
  // a) worker or worklet instance --> global proxy object
  // As we shouldn't expose the global object to author scripts, we map the
  // worker or worklet instance to the global proxy object.
  // b) global proxy object --> worker or worklet instance
  // Blink's callback functions are called by V8 with the global proxy object,
  // we need to map the global proxy object to the worker or worklet instance.
  // c) global object --> worker or worklet instance
  // The global proxy object is NOT considered as a wrapper object of the
  // worker or worklet instance because it's not an instance of
  // v8::FunctionTemplate of worker or worklet, especially note that
  // v8::Object::FindInstanceInPrototypeChain skips the global proxy object.
  // Thus we need to map the global object to the worker or worklet instance.

  // The global proxy object.  Note this is not the global object.
  v8::Local<v8::Object> global_proxy = context->Global();
  v8::Local<v8::Object> associated_wrapper =
      V8DOMWrapper::AssociateObjectWithWrapper(isolate_, script_wrappable,
                                               wrapper_type_info, global_proxy);
  CHECK(global_proxy == associated_wrapper);

  // The global object, aka worker/worklet wrapper object.
  v8::Local<v8::Object> global_object =
      global_proxy->GetPrototype().As<v8::Object>();
  V8DOMWrapper::SetNativeInfo(isolate_, global_object, wrapper_type_info,
                              script_wrappable);

  if (global_scope_->IsMainThreadWorkletGlobalScope()) {
    // Set the human readable name for the world.
    DCHECK(!global_scope_->Name().IsEmpty());
    world_->SetNonMainWorldHumanReadableName(world_->GetWorldId(),
                                             global_scope_->Name());
  } else {
    // Name new context for debugging. For main thread worklet global scopes
    // this is done once the context is initialized.
    WorkerThreadDebugger* debugger = WorkerThreadDebugger::From(isolate_);
    debugger->ContextCreated(global_scope_->GetThread(), url_for_debugger,
                             context);
  }

  if (!disable_eval_pending_.IsEmpty()) {
    DisableEvalInternal(disable_eval_pending_);
    disable_eval_pending_ = String();
  }

  // This is a workaround for worker with on-the-main-thread script fetch and
  // worklets.
  // - For workers with off-the-main-thread worker script fetch,
  //   PrepareForEvaluation() is called in WorkerGlobalScope::Initialize() after
  //   top-level worker script fetch and before script evaluation.
  // - For workers with on-the-main-thread worker script fetch, it's too early
  //   to call PrepareForEvaluation() in WorkerGlobalScope::Initialize() because
  //   it's called immediately after WorkerGlobalScope's constructor, that is,
  //   before WorkerOrWorkletScriptController::Initialize(). Therefore, we
  //   ignore the first call of PrepareForEvaluation() from
  //   WorkerGlobalScope::Initialize(), and call it here again.
  // TODO(https://crbug.com/835717): Remove this workaround once
  // off-the-main-thread worker script fetch is enabled by default for dedicated
  // workers.
  //
  // - For worklets, there is no appropriate timing to call
  //   PrepareForEvaluation() other than here because worklets have various
  //   initialization sequences depending on thread model (on-main-thread vs.
  //   off-main-thread) and unique script fetch (fetching a top-level script per
  //   addModule() call in JS).
  // TODO(nhiroki): Unify worklet initialization sequences, and move this to an
  // appropriate place.
  if ((global_scope_->IsWorkerGlobalScope() &&
       To<WorkerGlobalScope>(global_scope_.Get())
           ->IsOffMainThreadScriptFetchDisabled()) ||
      global_scope_->IsWorkletGlobalScope()) {
    // This should be called after origin trial tokens are applied for
    // OriginTrialContext in WorkerGlobalScope::Initialize() to install origin
    // trial features in JavaScript's global object. Workers with
    // on-the-main-thread script fetch and worklets apply origin trial tokens
    // before WorkerOrWorkletScriptController::initialize(), so it's safe to
    // call this here.
    PrepareForEvaluation();
  }
}

void WorkerOrWorkletScriptController::PrepareForEvaluation() {
  if (!IsContextInitialized()) {
    // For workers with on-the-main-thread worker script fetch, this can be
    // called before WorkerOrWorkletScriptController::Initialize() via
    // WorkerGlobalScope creation function. In this case, PrepareForEvaluation()
    // calls this function again. See comments in PrepareForEvaluation().
    DCHECK(global_scope_->IsWorkerGlobalScope());
    DCHECK(To<WorkerGlobalScope>(global_scope_.Get())
               ->IsOffMainThreadScriptFetchDisabled());
    return;
  }
  DCHECK(!is_ready_to_evaluate_);
  is_ready_to_evaluate_ = true;

  v8::HandleScope handle_scope(isolate_);

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_INTERFACE)
  V8PerContextData* per_context_data = script_state_->PerContextData();
  ignore_result(per_context_data->ConstructorForType(
      global_scope_->GetWrapperTypeInfo()));
#else   // USE_BLINK_V8_BINDING_NEW_IDL_INTERFACE
  ScriptState::Scope scope(script_state_);
  v8::Local<v8::Context> context = script_state_->GetContext();

  auto* script_wrappable = static_cast<ScriptWrappable*>(global_scope_);
  const WrapperTypeInfo* wrapper_type_info =
      script_wrappable->GetWrapperTypeInfo();

  // All interfaces must be registered to V8PerContextData.
  // So we explicitly call constructorForType for the global object.
  // This should be called after OriginTrialContext::AddTokens() in
  // WorkerGlobalScope::Initialize() to install origin trial features.
  V8PerContextData::From(context)->ConstructorForType(wrapper_type_info);

  v8::Local<v8::Object> global_object =
      context->Global()->GetPrototype().As<v8::Object>();
  DCHECK(!global_object.IsEmpty());

  v8::Local<v8::FunctionTemplate> global_interface_template =
      wrapper_type_info->DomTemplate(isolate_, *world_);
  DCHECK(!global_interface_template.IsEmpty());

  wrapper_type_info->InstallConditionalFeatures(
      context, *world_, global_object, v8::Local<v8::Object>(),
      v8::Local<v8::Function>(), global_interface_template);
#endif  // USE_BLINK_V8_BINDING_NEW_IDL_INTERFACE
}

void WorkerOrWorkletScriptController::DisableEvalInternal(
    const String& error_message) {
  DCHECK(IsContextInitialized());
  DCHECK(!error_message.IsEmpty());

  ScriptState::Scope scope(script_state_);
  script_state_->GetContext()->AllowCodeGenerationFromStrings(false);
  script_state_->GetContext()->SetErrorMessageForCodeGenerationFromStrings(
      V8String(isolate_, error_message));
}

// https://html.spec.whatwg.org/C/#run-a-classic-script
ScriptEvaluationResult WorkerOrWorkletScriptController::EvaluateAndReturnValue(
    const ScriptSourceCode& source_code,
    SanitizeScriptErrors sanitize_script_errors,
    mojom::blink::V8CacheOptions v8_cache_options,
    V8ScriptRunner::RethrowErrorsOption rethrow_errors) {
  if (IsExecutionForbidden())
    return ScriptEvaluationResult::FromClassicNotRun();

  DCHECK(IsContextInitialized());
  DCHECK(is_ready_to_evaluate_);

  // TODO(crbug/1114994): Plumb this from ClassicScript.
  const KURL base_url = source_code.Url();

  // Use default ReferrerScriptInfo here, as
  // - A work{er,let} script doesn't have a nonce, and
  // - a work{er,let} script is always "not parser inserted".
  // TODO(crbug/1114989): Plumb ScriptFetchOptions from ClassicScript.
  ScriptEvaluationResult result = V8ScriptRunner::CompileAndRunScript(
      isolate_, script_state_, global_scope_, source_code, base_url,
      sanitize_script_errors, ScriptFetchOptions(), v8_cache_options,
      std::move(rethrow_errors));

  if (result.GetResultType() == ScriptEvaluationResult::ResultType::kAborted)
    ForbidExecution();
  else
    CHECK(!IsExecutionForbidden());

  return result;
}

void WorkerOrWorkletScriptController::ForbidExecution() {
  DCHECK(global_scope_->IsContextThread());
  execution_forbidden_ = true;
}

bool WorkerOrWorkletScriptController::IsExecutionForbidden() const {
  DCHECK(global_scope_->IsContextThread());
  return execution_forbidden_;
}

void WorkerOrWorkletScriptController::DisableEval(const String& error_message) {
  DCHECK(!error_message.IsEmpty());
  // Currently, this can be called before or after
  // WorkerOrWorkletScriptController::Initialize() because of messy
  // worker/worklet initialization sequences. Tidy them up after
  // off-the-main-thread worker script fetch is enabled by default, make
  // sure to call WorkerOrWorkletScriptController::DisableEval() after
  // WorkerOrWorkletScriptController::Initialize(), and remove
  // |disable_eval_pending_| logic (https://crbug.com/960770).
  if (IsContextInitialized()) {
    DisableEvalInternal(error_message);
    return;
  }
  // `eval()` will actually be disabled on
  // WorkerOrWorkletScriptController::Initialize() to be called from
  // WorkerThread::InitializeOnWorkerThread() immediately and synchronously
  // after returning here. Keep the error message until that time.
  DCHECK(disable_eval_pending_.IsEmpty());
  disable_eval_pending_ = error_message;
}

void WorkerOrWorkletScriptController::Trace(Visitor* visitor) const {
  visitor->Trace(global_scope_);
  visitor->Trace(script_state_);
}

}  // namespace blink
