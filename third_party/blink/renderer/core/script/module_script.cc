// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/module_script.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/script/script_module_resolver.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "v8/include/v8.h"

namespace blink {

// <specdef href="https://html.spec.whatwg.org/#creating-a-module-script">
ModuleScript* ModuleScript::Create(const ParkableString& original_source_text,
                                   Modulator* modulator,
                                   const KURL& source_url,
                                   const KURL& base_url,
                                   const ScriptFetchOptions& options,
                                   AccessControlStatus access_control_status,
                                   const TextPosition& start_position) {
  // <spec step="1">If scripting is disabled for settings's responsible browsing
  // context, then set source to the empty string.</spec>
  ParkableString source_text;
  if (!modulator->IsScriptingDisabled())
    source_text = original_source_text;

  // <spec step="2">Let script be a new module script that this algorithm will
  // subsequently initialize.</spec>

  // <spec step="3">Set script's settings object to settings.</spec>
  //
  // Note: "script's settings object" will be |modulator|.

  // <spec step="7">Let result be ParseModule(source, settings's Realm,
  // script).</spec>
  ScriptState* script_state = modulator->GetScriptState();
  ScriptState::Scope scope(script_state);
  v8::Isolate* isolate = script_state->GetIsolate();
  ExceptionState exception_state(isolate, ExceptionState::kExecutionContext,
                                 "ModuleScript", "Create");

  ScriptModule result = ScriptModule::Compile(
      isolate, source_text.ToString(), source_url, base_url, options,
      access_control_status, start_position, exception_state);

  // CreateInternal processes Steps 4 and 8-10.
  //
  // [nospec] We initialize the other ModuleScript members anyway by running
  // Steps 8-13 before Step 6. In a case that compile failed, we will
  // immediately turn the script into errored state. Thus the members will not
  // be used for the speced algorithms, but may be used from inspector.
  ModuleScript* script =
      CreateInternal(source_text, modulator, result, source_url, base_url,
                     options, start_position);

  // <spec step="8">If result is a list of errors, then:</spec>
  if (exception_state.HadException()) {
    DCHECK(result.IsNull());

    // <spec step="8.1">Set script's parse error to result[0].</spec>
    v8::Local<v8::Value> error = exception_state.GetException();
    exception_state.ClearException();
    script->SetParseErrorAndClearRecord(ScriptValue(script_state, error));

    // <spec step="8.2">Return script.</spec>
    return script;
  }

  // <spec step="9">For each string requested of
  // result.[[RequestedModules]]:</spec>
  for (const auto& requested :
       modulator->ModuleRequestsFromScriptModule(result)) {
    // <spec step="9.1">Let url be the result of resolving a module specifier
    // given script's base URL and requested.</spec>
    //
    // <spec step="9.2">If url is failure, then:</spec>
    String failure_reason;
    if (script->ResolveModuleSpecifier(requested.specifier, &failure_reason)
            .IsValid())
      continue;

    // <spec step="9.2.1">Let error be a new TypeError exception.</spec>
    String error_message = "Failed to resolve module specifier \"" +
                           requested.specifier + "\". " + failure_reason;
    v8::Local<v8::Value> error =
        V8ThrowException::CreateTypeError(isolate, error_message);

    // <spec step="9.2.2">Set script's parse error to error.</spec>
    script->SetParseErrorAndClearRecord(ScriptValue(script_state, error));

    // <spec step="9.2.3">Return script.</spec>
    return script;
  }

  // <spec step="11">Return script.</spec>
  return script;
}

ModuleScript* ModuleScript::CreateForTest(Modulator* modulator,
                                          ScriptModule record,
                                          const KURL& base_url,
                                          const ScriptFetchOptions& options) {
  ParkableString dummy_source_text(String("").ReleaseImpl());
  KURL dummy_source_url;
  return CreateInternal(dummy_source_text, modulator, record, dummy_source_url,
                        base_url, options, TextPosition::MinimumPosition());
}

// <specdef href="https://html.spec.whatwg.org/#creating-a-module-script">
ModuleScript* ModuleScript::CreateInternal(const ParkableString& source_text,
                                           Modulator* modulator,
                                           ScriptModule result,
                                           const KURL& source_url,
                                           const KURL& base_url,
                                           const ScriptFetchOptions& options,
                                           const TextPosition& start_position) {
  // <spec step="6">Set script's parse error and error to rethrow to
  // null.</spec>
  //
  // <spec step="10">Set script's record to result.</spec>
  //
  // <spec step="4">Set script's base URL to baseURL.</spec>
  //
  // <spec step="5">Set script's fetch options to options.</spec>
  //
  // [nospec] |source_text| is saved for CSP checks.
  ModuleScript* module_script =
      new ModuleScript(modulator, result, source_url, base_url, options,
                       source_text, start_position);

  // Step 7, a part of ParseModule(): Passing script as the last parameter
  // here ensures result.[[HostDefined]] will be script.
  modulator->GetScriptModuleResolver()->RegisterModuleScript(module_script);

  return module_script;
}

ModuleScript::ModuleScript(Modulator* settings_object,
                           ScriptModule record,
                           const KURL& source_url,
                           const KURL& base_url,
                           const ScriptFetchOptions& fetch_options,
                           const ParkableString& source_text,
                           const TextPosition& start_position)
    : Script(fetch_options, base_url),
      settings_object_(settings_object),
      source_text_(source_text),
      start_position_(start_position),
      source_url_(source_url) {
  if (record.IsNull()) {
    // We allow empty records for module infra tests which never touch records.
    // This should never happen outside unit tests.
    return;
  }

  DCHECK(settings_object);
  v8::Isolate* isolate = settings_object_->GetScriptState()->GetIsolate();
  v8::HandleScope scope(isolate);
  record_.Set(isolate, record.NewLocal(isolate));
}

ScriptModule ModuleScript::Record() const {
  if (record_.IsEmpty())
    return ScriptModule();

  v8::Isolate* isolate = settings_object_->GetScriptState()->GetIsolate();
  v8::HandleScope scope(isolate);
  return ScriptModule(isolate, record_.NewLocal(isolate), source_url_);
}

bool ModuleScript::HasEmptyRecord() const {
  return record_.IsEmpty();
}

void ModuleScript::SetParseErrorAndClearRecord(ScriptValue error) {
  DCHECK(!error.IsEmpty());

  record_.Clear();
  ScriptState::Scope scope(error.GetScriptState());
  parse_error_.Set(error.GetIsolate(), error.V8Value());
}

ScriptValue ModuleScript::CreateParseError() const {
  ScriptState* script_state = settings_object_->GetScriptState();
  v8::Isolate* isolate = script_state->GetIsolate();
  ScriptState::Scope scope(script_state);
  ScriptValue error(script_state, parse_error_.NewLocal(isolate));
  DCHECK(!error.IsEmpty());
  return error;
}

void ModuleScript::SetErrorToRethrow(ScriptValue error) {
  ScriptState::Scope scope(error.GetScriptState());
  error_to_rethrow_.Set(error.GetIsolate(), error.V8Value());
}

ScriptValue ModuleScript::CreateErrorToRethrow() const {
  ScriptState* script_state = settings_object_->GetScriptState();
  v8::Isolate* isolate = script_state->GetIsolate();
  ScriptState::Scope scope(script_state);
  ScriptValue error(script_state, error_to_rethrow_.NewLocal(isolate));
  DCHECK(!error.IsEmpty());
  return error;
}

KURL ModuleScript::ResolveModuleSpecifier(const String& module_request,
                                          String* failure_reason) {
  auto found = specifier_to_url_cache_.find(module_request);
  if (found != specifier_to_url_cache_.end())
    return found->value;

  KURL url = settings_object_->ResolveModuleSpecifier(module_request, BaseURL(),
                                                      failure_reason);
  // Cache the result only on success, so that failure_reason is set for
  // subsequent calls too.
  if (url.IsValid())
    specifier_to_url_cache_.insert(module_request, url);
  return url;
}

void ModuleScript::Trace(blink::Visitor* visitor) {
  visitor->Trace(settings_object_);
  visitor->Trace(record_.UnsafeCast<v8::Value>());
  visitor->Trace(parse_error_);
  visitor->Trace(error_to_rethrow_);
  Script::Trace(visitor);
}

void ModuleScript::RunScript(LocalFrame* frame, const SecurityOrigin*) const {
  DVLOG(1) << *this << "::RunScript()";
  settings_object_->ExecuteModule(this,
                                  Modulator::CaptureEvalErrorFlag::kReport);
}

String ModuleScript::InlineSourceTextForCSP() const {
  return source_text_.ToString();
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
