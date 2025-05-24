// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/js_module_script.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_creation_params.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/script/module_record_resolver.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "v8/include/v8.h"

namespace blink {

// <specdef
// href="https://html.spec.whatwg.org/C/#creating-a-javascript-module-script">
JSModuleScript* JSModuleScript::Create(
    const ModuleScriptCreationParams& original_params,
    Modulator* modulator,
    const ScriptFetchOptions& options,
    const TextPosition& start_position) {
  // Note: this needs to be set here so modulator->IsScriptingDisabled() below
  //       has access to the correct context information.
  // TODO(crbug.com/371004128): this seems wrong; `IsScriptingDisabled()` should
  //       be modified so that it uses the correct ScriptState internally.
  ScriptState* script_state = modulator->GetScriptState();
  ScriptState::Scope scope(script_state);

  // <spec step="1">If scripting is disabled for settings's responsible browsing
  // context, then set source to the empty string.</spec>
  const ModuleScriptCreationParams& params =
      modulator->IsScriptingDisabled()
          ? original_params.CopyWithClearedSourceText()
          : original_params;

  // <spec step="2">Let script be a new module script that this algorithm will
  // subsequently initialize.</spec>

  // <spec step="3">Set script's settings object to settings.</spec>
  //
  // Note: "script's settings object" will be |modulator|.

  // <spec step="7">Let result be ParseModule(source, settings's Realm,
  // script).</spec>
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::TryCatch try_catch(isolate);

  ModuleRecordProduceCacheData* produce_cache_data = nullptr;

  v8::Local<v8::Module> result = ModuleRecord::Compile(
      script_state, params, options, start_position,
      modulator->GetV8CacheOptions(), &produce_cache_data);

  // CreateInternal processes Steps 4 and 8-10.
  //
  // [nospec] We initialize the other JSModuleScript members anyway by running
  // Steps 8-13 before Step 6. In a case that compile failed, we will
  // immediately turn the script into errored state. Thus the members will not
  // be used for the speced algorithms, but may be used from inspector.
  JSModuleScript* script = CreateInternal(
      params.GetSourceText().length(), modulator, result, params.SourceURL(),
      params.BaseURL(), options, start_position, produce_cache_data);

  // <spec step="8">If result is a list of errors, then:</spec>
  if (try_catch.HasCaught()) {
    DCHECK(result.IsEmpty());

    // <spec step="8.1">Set script's parse error to result[0].</spec>
    v8::Local<v8::Value> error = try_catch.Exception();
    script->SetParseErrorAndClearRecord(ScriptValue(isolate, error));

    // <spec step="8.2">Return script.</spec>
    return script;
  }

  // <spec step="9">For each string requested of
  // result.[[RequestedModules]]:</spec>
  for (const auto& requested :
       ModuleRecord::ModuleRequests(script_state, result)) {
    v8::MaybeLocal<v8::Value> error;

    String failure_reason;
    // <spec step="9.1">If requested.[[Attributes]] contains a Record entry
    // such that entry.[[Key]] is not "type", then:</spec>
    if (requested.HasInvalidImportAttributeKey(&failure_reason)) {
      // <spec step="9.1.1">Let error be a new SyntaxError exception.</spec>
      error = V8ThrowException::CreateSyntaxError(
          isolate, "Invalid attribute key \"" + failure_reason + "\".");

      // <spec step="9.2">Resolve a module specifier given script and
      // requested.[[Specifier]], catching any exceptions.</spec>
    } else if (!script
                    ->ResolveModuleSpecifier(requested.specifier,
                                             &failure_reason)
                    .IsValid()) {
      error = V8ThrowException::CreateTypeError(
          isolate, "Failed to resolve module specifier \"" +
                       requested.specifier + "\". " + failure_reason);
      // <spec step="9.4">Let moduleType be the result of running the module
      // type from module request steps given requested.</spec>
      //
      // <spec step="9.5">If the result of running the module type allowed steps
      // given moduleType and settings is false, then:</spec>
    } else if (modulator->ModuleTypeFromRequest(requested) ==
               ModuleType::kInvalid) {
      // <spec step="9.5.1">Let error be a new TypeError exception.</spec>
      error = V8ThrowException::CreateTypeError(
          isolate, "\"" + requested.GetModuleTypeString() +
                       "\" is not a valid module type.");
    }

    if (!error.IsEmpty()) {
      // <spec step="9.1.2">Set script's parse error to error.</spec>
      script->SetParseErrorAndClearRecord(
          ScriptValue(isolate, error.ToLocalChecked()));

      // <spec step="9.1.3">Return script.</spec>
      return script;
    }
  }

  // <spec step="11">Return script.</spec>
  return script;
}

JSModuleScript* JSModuleScript::CreateForTest(
    Modulator* modulator,
    v8::Local<v8::Module> record,
    const KURL& base_url,
    const ScriptFetchOptions& options) {
  KURL dummy_source_url;
  return CreateInternal(0, modulator, record, dummy_source_url, base_url,
                        options, TextPosition::MinimumPosition(), nullptr);
}

// <specdef
// href="https://html.spec.whatwg.org/C/#creating-a-javascript-module-script">
JSModuleScript* JSModuleScript::CreateInternal(
    size_t source_text_length,
    Modulator* modulator,
    v8::Local<v8::Module> result,
    const KURL& source_url,
    const KURL& base_url,
    const ScriptFetchOptions& options,
    const TextPosition& start_position,
    ModuleRecordProduceCacheData* produce_cache_data) {
  // <spec step="6">Set script's parse error and error to rethrow to
  // null.</spec>
  //
  // <spec step="10">Set script's record to result.</spec>
  //
  // <spec step="4">Set script's base URL to baseURL.</spec>
  //
  // <spec step="5">Set script's fetch options to options.</spec>
  JSModuleScript* module_script = MakeGarbageCollected<JSModuleScript>(
      modulator, result, source_url, base_url, options, source_text_length,
      start_position, produce_cache_data);

  // Step 7, a part of ParseModule(): Passing script as the last parameter
  // here ensures result.[[HostDefined]] will be script.
  modulator->GetModuleRecordResolver()->RegisterModuleScript(module_script);

  return module_script;
}

JSModuleScript::JSModuleScript(Modulator* settings_object,
                               v8::Local<v8::Module> record,
                               const KURL& source_url,
                               const KURL& base_url,
                               const ScriptFetchOptions& fetch_options,
                               size_t source_text_length,
                               const TextPosition& start_position,
                               ModuleRecordProduceCacheData* produce_cache_data)
    : ModuleScript(settings_object,
                   record,
                   source_url,
                   base_url,
                   fetch_options,
                   start_position),
      source_text_length_(source_text_length),
      produce_cache_data_(produce_cache_data) {}

void JSModuleScript::ProduceCache() {
  if (!produce_cache_data_)
    return;

  ScriptState* script_state = SettingsObject()->GetScriptState();
  v8::Isolate* isolate = script_state->GetIsolate();
  ScriptState::Scope scope(script_state);

  ExecutionContext* execution_context =
      ExecutionContext::From(isolate->GetCurrentContext());
  V8CodeCache::ProduceCache(
      isolate, ExecutionContext::GetCodeCacheHostFromContext(execution_context),
      produce_cache_data_, source_text_length_, SourceUrl(), StartPosition());

  produce_cache_data_ = nullptr;
}

void JSModuleScript::Trace(Visitor* visitor) const {
  visitor->Trace(produce_cache_data_);
  ModuleScript::Trace(visitor);
}

}  // namespace blink
