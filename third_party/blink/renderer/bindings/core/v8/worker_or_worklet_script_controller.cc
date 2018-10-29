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
#include "third_party/blink/renderer/bindings/core/v8/initialize_v8_extras_binding.h"
#include "third_party/blink/renderer/bindings/core/v8/referrer_script_info.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_code_cache.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_initializer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/core/events/error_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/inspector/worker_thread_debugger.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/bindings/origin_trial_features.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_wrapper.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "third_party/blink/renderer/platform/bindings/wrapper_type_info.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "v8/include/v8.h"

namespace blink {

class WorkerOrWorkletScriptController::ExecutionState final {
  STACK_ALLOCATED();

 public:
  explicit ExecutionState(WorkerOrWorkletScriptController* controller)
      : had_exception(false),
        controller_(controller),
        outer_state_(controller->execution_state_) {
    controller_->execution_state_ = this;
  }

  ~ExecutionState() { controller_->execution_state_ = outer_state_; }

  bool had_exception;
  String error_message;
  std::unique_ptr<SourceLocation> location_;
  ScriptValue exception;
  Member<ErrorEvent> error_event_from_imported_script_;

  // A ExecutionState context is stack allocated by
  // WorkerOrWorkletScriptController::evaluate(), with the contoller using it
  // during script evaluation. To handle nested evaluate() uses,
  // ExecutionStates are chained together;
  // |m_outerState| keeps a pointer to the context object one level out
  // (or 0, if outermost.) Upon return from evaluate(), the
  // WorkerOrWorkletScriptController's ExecutionState is popped and the
  // previous one restored (see above dtor.)
  //
  // With Oilpan, |m_outerState| isn't traced. It'll be "up the stack"
  // and its fields will be traced when scanning the stack.
  Member<WorkerOrWorkletScriptController> controller_;
  ExecutionState* outer_state_;
};

WorkerOrWorkletScriptController* WorkerOrWorkletScriptController::Create(
    WorkerOrWorkletGlobalScope* global_scope,
    v8::Isolate* isolate) {
  return new WorkerOrWorkletScriptController(global_scope, isolate);
}

WorkerOrWorkletScriptController::WorkerOrWorkletScriptController(
    WorkerOrWorkletGlobalScope* global_scope,
    v8::Isolate* isolate)
    : global_scope_(global_scope),
      isolate_(isolate),
      execution_forbidden_(false),
      rejected_promises_(RejectedPromises::Create()),
      execution_state_(nullptr) {
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

  world_->Dispose();

  DisposeContextIfNeeded();
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

  script_state_->DisposePerContextData();
  script_state_->DissociateContext();
}

bool WorkerOrWorkletScriptController::InitializeContextIfNeeded(
    const String& human_readable_name,
    const KURL& url_for_debugger) {
  v8::HandleScope handle_scope(isolate_);

  if (IsContextInitialized())
    return true;

  // Create a new v8::Context with the worker/worklet as the global object
  // (aka the inner global).
  auto* script_wrappable = static_cast<ScriptWrappable*>(global_scope_);
  const WrapperTypeInfo* wrapper_type_info =
      script_wrappable->GetWrapperTypeInfo();
  v8::Local<v8::FunctionTemplate> global_interface_template =
      wrapper_type_info->domTemplate(isolate_, *world_);
  if (global_interface_template.IsEmpty())
    return false;
  v8::Local<v8::ObjectTemplate> global_template =
      global_interface_template->InstanceTemplate();
  v8::Local<v8::Context> context;
  {
    // Initialize V8 extensions before creating the context.
    v8::ExtensionConfiguration extension_configuration =
        ScriptController::ExtensionsFor(global_scope_);

    V8PerIsolateData::UseCounterDisabledScope use_counter_disabled(
        V8PerIsolateData::From(isolate_));
    context =
        v8::Context::New(isolate_, &extension_configuration, global_template);
  }
  if (context.IsEmpty())
    return false;

  script_state_ = ScriptState::Create(context, world_);

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

  // All interfaces must be registered to V8PerContextData.
  // So we explicitly call constructorForType for the global object.
  V8PerContextData::From(context)->ConstructorForType(wrapper_type_info);

  if (global_scope_->IsMainThreadWorkletGlobalScope()) {
    // Set the human readable name for the world if the call passes an actual
    // |human_readable name|.
    if (!human_readable_name.IsEmpty()) {
      world_->SetNonMainWorldHumanReadableName(world_->GetWorldId(),
                                               human_readable_name);
    }
  } else {
    // Name new context for debugging. For main thread worklet global scopes
    // this is done once the context is initialized.
    WorkerThreadDebugger* debugger = WorkerThreadDebugger::From(isolate_);
    debugger->ContextCreated(global_scope_->GetThread(), url_for_debugger,
                             context);
  }

  wrapper_type_info->InstallConditionalFeatures(
      context, *world_, global_object, v8::Local<v8::Object>(),
      v8::Local<v8::Function>(), global_interface_template);

  if (!disable_eval_pending_.IsEmpty()) {
    script_state_->GetContext()->AllowCodeGenerationFromStrings(false);
    script_state_->GetContext()->SetErrorMessageForCodeGenerationFromStrings(
        V8String(isolate_, disable_eval_pending_));
    disable_eval_pending_ = String();
  }

  // This can only be called after the global object is fully initialised, as it
  // reads values from it.
  InitializeV8ExtrasBinding(script_state_);

  return true;
}

ScriptValue WorkerOrWorkletScriptController::EvaluateInternal(
    const ScriptSourceCode& source_code,
    AccessControlStatus access_control_status,
    V8CacheOptions v8_cache_options) {
  DCHECK(IsContextInitialized());

  TRACE_EVENT1("devtools.timeline", "EvaluateScript", "data",
               InspectorEvaluateScriptEvent::Data(nullptr, source_code.Url(),
                                                  source_code.StartPosition()));

  ScriptState::Scope scope(script_state_);

  v8::TryCatch block(isolate_);

  v8::Local<v8::Script> compiled_script;
  v8::MaybeLocal<v8::Value> maybe_result;
  // Use default ReferrerScriptInfo here, as
  // - A work{er,let} script doesn't have a nonce, and
  // - a work{er,let} script is always "not parser inserted".
  ReferrerScriptInfo referrer_info;
  v8::ScriptCompiler::CompileOptions compile_options;
  V8CodeCache::ProduceCacheOptions produce_cache_options;
  v8::ScriptCompiler::NoCacheReason no_cache_reason;
  std::tie(compile_options, produce_cache_options, no_cache_reason) =
      V8CodeCache::GetCompileOptions(v8_cache_options, source_code);
  if (V8ScriptRunner::CompileScript(script_state_, source_code,
                                    access_control_status, compile_options,
                                    no_cache_reason, referrer_info)
          .ToLocal(&compiled_script)) {
    maybe_result = V8ScriptRunner::RunCompiledScript(isolate_, compiled_script,
                                                     global_scope_);
    V8CodeCache::ProduceCache(isolate_, compiled_script, source_code,
                              produce_cache_options, compile_options);
  }

  if (!block.CanContinue()) {
    ForbidExecution();
    return ScriptValue();
  }

  if (block.HasCaught()) {
    v8::Local<v8::Message> message = block.Message();
    execution_state_->had_exception = true;
    execution_state_->error_message = ToCoreString(message->Get());
    execution_state_->location_ = SourceLocation::FromMessage(
        isolate_, message, ExecutionContext::From(script_state_));
    execution_state_->exception = ScriptValue(script_state_, block.Exception());
    block.Reset();
  } else {
    execution_state_->had_exception = false;
  }

  v8::Local<v8::Value> result;
  if (!maybe_result.ToLocal(&result) || result->IsUndefined())
    return ScriptValue();

  return ScriptValue(script_state_, result);
}

bool WorkerOrWorkletScriptController::Evaluate(
    const ScriptSourceCode& source_code,
    AccessControlStatus access_control_status,
    ErrorEvent** error_event,
    V8CacheOptions v8_cache_options) {
  if (IsExecutionForbidden())
    return false;

  ExecutionState state(this);
  EvaluateInternal(source_code, access_control_status, v8_cache_options);
  if (IsExecutionForbidden())
    return false;

  ScriptState::Scope scope(script_state_);
  if (state.had_exception) {
    if (error_event) {
      if (state.error_event_from_imported_script_) {
        // Propagate inner error event outwards.
        *error_event = state.error_event_from_imported_script_.Release();
        return false;
      }
      if (global_scope_->ShouldSanitizeScriptError(state.location_->Url(),
                                                   access_control_status)) {
        *error_event = ErrorEvent::CreateSanitizedError(world_.get());
      } else {
        *error_event =
            ErrorEvent::Create(state.error_message, state.location_->Clone(),
                               state.exception, world_.get());
      }
    } else {
      DCHECK(!global_scope_->ShouldSanitizeScriptError(state.location_->Url(),
                                                       access_control_status));
      ErrorEvent* event = nullptr;
      if (state.error_event_from_imported_script_) {
        event = state.error_event_from_imported_script_.Release();
      } else {
        event =
            ErrorEvent::Create(state.error_message, state.location_->Clone(),
                               state.exception, world_.get());
      }
      global_scope_->DispatchErrorEvent(event, access_control_status);
    }
    return false;
  }
  return true;
}

ScriptValue WorkerOrWorkletScriptController::EvaluateAndReturnValueForTest(
    const ScriptSourceCode& source_code) {
  ExecutionState state(this);
  return EvaluateInternal(source_code, kOpaqueResource, kV8CacheOptionsDefault);
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
  disable_eval_pending_ = error_message;
}

void WorkerOrWorkletScriptController::RethrowExceptionFromImportedScript(
    ErrorEvent* error_event,
    ExceptionState& exception_state) {
  const String& error_message = error_event->message();
  if (execution_state_)
    execution_state_->error_event_from_imported_script_ = error_event;
  exception_state.RethrowV8Exception(
      V8ThrowException::CreateError(isolate_, error_message));
}

void WorkerOrWorkletScriptController::Trace(blink::Visitor* visitor) {
  visitor->Trace(global_scope_);
  visitor->Trace(script_state_);
}

}  // namespace blink
