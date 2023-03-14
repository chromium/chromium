// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/callback_function_base.h"

#include "third_party/blink/renderer/platform/bindings/binding_security_for_platform.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"

namespace blink {

CallbackFunctionBase::CallbackFunctionBase(
    v8::Local<v8::Object> callback_function) {
  DCHECK(!callback_function.IsEmpty());

  v8::Isolate* isolate = callback_function->GetIsolate();
  callback_function_.Reset(isolate, callback_function);

  ScriptState* incumbent_script_state =
      ScriptState::From(isolate->GetIncumbentContext());

  // Set |callback_relevant_script_state_| iff the creation context and the
  // incumbent context are the same origin-domain. Otherwise, leave it as
  // nullptr.
  if (callback_function->IsFunction()) {
    // If the callback object is a function, it's guaranteed to be the same
    // origin at least, and very likely to be the same origin-domain. Even if
    // it's not the same origin-domain, it's already been possible for the
    // callsite to run arbitrary script in the context. No need to protect it.
    // This is an optimization faster than ShouldAllowAccessToV8Context below.
    ScriptState* callback_relevant_script_state =
        ScriptState::ForRelevantRealm(callback_function);
    if (incumbent_script_state != callback_relevant_script_state) {
      // If the contexts are the same delay caching until the first callback
      // execution.
      MakeCachedData(callback_relevant_script_state, incumbent_script_state);
    }
  } else {
    v8::MaybeLocal<v8::Context> creation_context =
        callback_function->GetCreationContext();
    if (BindingSecurityForPlatform::ShouldAllowAccessToV8Context(
            incumbent_script_state->GetContext(), creation_context,
            BindingSecurityForPlatform::ErrorReportOption::kDoNotReport)) {
      ScriptState* callback_relevant_script_state =
          ScriptState::From(creation_context.ToLocalChecked());
      MakeCachedData(callback_relevant_script_state, incumbent_script_state);
    } else {
      // Record the fact that callback relevant script is not available.
      MakeCachedData(nullptr, incumbent_script_state);
    }
  }
}

void CallbackFunctionBase::Trace(Visitor* visitor) const {
  visitor->Trace(callback_function_);
  visitor->Trace(cached_data_);
}

void CallbackFunctionBase::CachedData::Trace(Visitor* visitor) const {
  visitor->Trace(callback_relevant_script_state_);
  visitor->Trace(incumbent_script_state_);
}

void CallbackFunctionBase::MakeCachedData(
    ScriptState* callback_relevant_script_state,
    ScriptState* incumbent_script_state) const {
  // This method is either called during object construction or lazily via
  // one of the ScriptState* getters. In the former case there are no data
  // races while in the latter the data race is theoretically possible but the
  // contents of the resulting object will contain exactly the same data, so
  // we are generally fine with whatever instance survives. The other one
  // will be collected by GC.
  cached_data_ = MakeGarbageCollected<CachedData>(
      callback_relevant_script_state, incumbent_script_state);
}

void CallbackFunctionBase::MakeCachedData() const {
  DCHECK(!cached_data_);
  // cached_data_ wasn't initialized by constructor which means that the
  // incumbent script state is the same as the callback relevant script state
  // and the latter is computable. So, compute the latter again and set both
  // fields.
  v8::Isolate* isolate = GetIsolate();
  v8::HandleScope scope(isolate);

  v8::Local<v8::Object> callback_function = callback_function_.Get(isolate);
  ScriptState* callback_relevant_script_state =
      ScriptState::ForRelevantRealm(callback_function);
  ScriptState* incumbent_script_state = callback_relevant_script_state;
  MakeCachedData(callback_relevant_script_state, incumbent_script_state);
}

ScriptState* CallbackFunctionBase::CallbackRelevantScriptStateOrReportError(
    const char* interface_name,
    const char* operation_name) {
  ScriptState* callback_relevant_script_state =
      CallbackRelevantScriptStateImpl();
  if (callback_relevant_script_state) {
    return callback_relevant_script_state;
  }

  // Report a SecurityError due to a cross origin callback object.
  ScriptState::Scope incumbent_scope(cached_data_->incumbent_script_state_);
  v8::TryCatch try_catch(GetIsolate());
  try_catch.SetVerbose(true);
  ExceptionState exception_state(GetIsolate(),
                                 ExceptionState::kExecutionContext,
                                 interface_name, operation_name);
  exception_state.ThrowSecurityError(
      "An invocation of the provided callback failed due to cross origin "
      "access.");
  return nullptr;
}

ScriptState* CallbackFunctionBase::CallbackRelevantScriptStateOrThrowException(
    const char* interface_name,
    const char* operation_name) {
  ScriptState* callback_relevant_script_state =
      CallbackRelevantScriptStateImpl();
  if (callback_relevant_script_state) {
    return callback_relevant_script_state;
  }

  // Throw a SecurityError due to a cross origin callback object.
  ScriptState::Scope incumbent_scope(cached_data_->incumbent_script_state_);
  ExceptionState exception_state(GetIsolate(),
                                 ExceptionState::kExecutionContext,
                                 interface_name, operation_name);
  exception_state.ThrowSecurityError(
      "An invocation of the provided callback failed due to cross origin "
      "access.");
  return nullptr;
}

void CallbackFunctionBase::EvaluateAsPartOfCallback(
    base::OnceCallback<void()> closure) {
  ScriptState* callback_relevant_script_state =
      CallbackRelevantScriptStateImpl();
  if (!callback_relevant_script_state) {
    return;
  }

  // https://webidl.spec.whatwg.org/#es-invoking-callback-functions
  // step 8: Prepare to run script with relevant settings.
  ScriptState::Scope callback_relevant_context_scope(
      callback_relevant_script_state);
  // step 9: Prepare to run a callback with stored settings.
  v8::Context::BackupIncumbentScope backup_incumbent_scope(
      IncumbentScriptState()->GetContext());

  std::move(closure).Run();
}

}  // namespace blink
