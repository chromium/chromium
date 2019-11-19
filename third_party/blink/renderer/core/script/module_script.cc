// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/module_script.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/script/module_record_resolver.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "v8/include/v8.h"

namespace blink {

ModuleScript::ModuleScript(Modulator* settings_object,
                           v8::Local<v8::Module> record,
                           const KURL& source_url,
                           const KURL& base_url,
                           const ScriptFetchOptions& fetch_options)
    : Script(fetch_options, base_url),
      settings_object_(settings_object),
      source_url_(source_url) {
  if (record.IsEmpty()) {
    // We allow empty records for module infra tests which never touch records.
    // This should never happen outside unit tests.
    return;
  }

  DCHECK(settings_object);
  v8::Isolate* isolate = settings_object_->GetScriptState()->GetIsolate();
  v8::HandleScope scope(isolate);
  record_.Set(isolate, record);
}

v8::Local<v8::Module> ModuleScript::V8Module() const {
  if (record_.IsEmpty()) {
    return v8::Local<v8::Module>();
  }
  v8::Isolate* isolate = settings_object_->GetScriptState()->GetIsolate();

  return record_.NewLocal(isolate);
}

bool ModuleScript::HasEmptyRecord() const {
  return record_.IsEmpty();
}

void ModuleScript::SetParseErrorAndClearRecord(ScriptValue error) {
  DCHECK(!error.IsEmpty());

  record_.Clear();
  parse_error_.Set(settings_object_->GetScriptState()->GetIsolate(),
                   error.V8Value());
}

ScriptValue ModuleScript::CreateParseError() const {
  ScriptState* script_state = settings_object_->GetScriptState();
  ScriptState::Scope scope(script_state);
  ScriptValue error(script_state->GetIsolate(), parse_error_.Get(script_state));
  DCHECK(!error.IsEmpty());
  return error;
}

void ModuleScript::SetErrorToRethrow(ScriptValue error) {
  ScriptState* script_state = settings_object_->GetScriptState();
  ScriptState::Scope scope(script_state);
  error_to_rethrow_.Set(script_state->GetIsolate(), error.V8Value());
}

ScriptValue ModuleScript::CreateErrorToRethrow() const {
  ScriptState* script_state = settings_object_->GetScriptState();
  ScriptState::Scope scope(script_state);
  ScriptValue error(script_state->GetIsolate(),
                    error_to_rethrow_.Get(script_state));
  DCHECK(!error.IsEmpty());
  return error;
}

KURL ModuleScript::ResolveModuleSpecifier(const String& module_request,
                                          String* failure_reason) const {
  auto found = specifier_to_url_cache_.find(module_request);
  if (found != specifier_to_url_cache_.end())
    return found->value;

  KURL url = SettingsObject()->ResolveModuleSpecifier(module_request, BaseURL(),
                                                      failure_reason);
  // Cache the result only on success, so that failure_reason is set for
  // subsequent calls too.
  if (url.IsValid())
    specifier_to_url_cache_.insert(module_request, url);
  return url;
}

void ModuleScript::Trace(Visitor* visitor) {
  visitor->Trace(settings_object_);
  visitor->Trace(record_.UnsafeCast<v8::Value>());
  visitor->Trace(parse_error_);
  visitor->Trace(error_to_rethrow_);
  Script::Trace(visitor);
}

void ModuleScript::RunScript(LocalFrame* frame, const SecurityOrigin*) {
  DVLOG(1) << *this << "::RunScript()";
  SettingsObject()->ExecuteModule(this,
                                  Modulator::CaptureEvalErrorFlag::kReport);
}

void ModuleScript::RunScriptOnWorker(WorkerGlobalScope& worker_global_scope) {
  DCHECK(worker_global_scope.IsContextThread());

  WorkerReportingProxy& worker_reporting_proxy =
      worker_global_scope.ReportingProxy();

  worker_reporting_proxy.WillEvaluateModuleScript();
  // This |error| is always null because the second argument is |kReport|.
  // TODO(nhiroki): Catch an error when an evaluation error happens.
  // (https://crbug.com/680046)
  ScriptValue error = SettingsObject()->ExecuteModule(
      this, Modulator::CaptureEvalErrorFlag::kReport);
  worker_reporting_proxy.DidEvaluateModuleScript(error.IsEmpty());
}

std::ostream& operator<<(std::ostream& stream,
                         const ModuleScript& module_script) {
  stream << "ModuleScript[" << &module_script;
  if (module_script.HasEmptyRecord())
    stream << ", empty-record";

  if (module_script.HasErrorToRethrow())
    stream << ", error-to-rethrow";

  if (module_script.HasParseError())
    stream << ", parse-error";

  return stream << "]";
}

}  // namespace blink
