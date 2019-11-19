// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/js_module_script.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/script/module_record_resolver.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "v8/include/v8.h"

namespace blink {

// <specdef
// href="https://html.spec.whatwg.org/C/#creating-a-javascript-module-script">
JSModuleScript* JSModuleScript::Create(
    const ParkableString& original_source_text,
    SingleCachedMetadataHandler* cache_handler,
    ScriptSourceLocationType source_location_type,
    Modulator* modulator,
    const KURL& source_url,
    const KURL& base_url,
    const ScriptFetchOptions& options,
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
                                 "JSModuleScript", "Create");

  ModuleRecordProduceCacheData* produce_cache_data = nullptr;

  v8::Local<v8::Module> result = ModuleRecord::Compile(
      isolate, source_text.ToString(), source_url, base_url, options,
      start_position, exception_state, modulator->GetV8CacheOptions(),
      cache_handler, source_location_type, &produce_cache_data);

  // CreateInternal processes Steps 4 and 8-10.
  //
  // [nospec] We initialize the other JSModuleScript members anyway by running
  // Steps 8-13 before Step 6. In a case that compile failed, we will
  // immediately turn the script into errored state. Thus the members will not
  // be used for the speced algorithms, but may be used from inspector.
  JSModuleScript* script =
      CreateInternal(source_text.length(), modulator, result, source_url,
                     base_url, options, start_position, produce_cache_data);

  // <spec step="8">If result is a list of errors, then:</spec>
  if (exception_state.HadException()) {
    DCHECK(result.IsEmpty());

    // <spec step="8.1">Set script's parse error to result[0].</spec>
    v8::Local<v8::Value> error = exception_state.GetException();
    exception_state.ClearException();
    script->SetParseErrorAndClearRecord(ScriptValue(isolate, error));

    // <spec step="8.2">Return script.</spec>
    return script;
  }

  // <spec step="9">For each string requested of
  // result.[[RequestedModules]]:</spec>
  for (const auto& requested :
       modulator->ModuleRequestsFromModuleRecord(result)) {
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
    script->SetParseErrorAndClearRecord(ScriptValue(isolate, error));

    // <spec step="9.2.3">Return script.</spec>
    return script;
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
                   fetch_options),
      source_text_length_(source_text_length),
      start_position_(start_position),
      produce_cache_data_(produce_cache_data) {}

void JSModuleScript::ProduceCache() {
  if (!produce_cache_data_)
    return;

  ScriptState* script_state = SettingsObject()->GetScriptState();
  v8::Isolate* isolate = script_state->GetIsolate();
  ScriptState::Scope scope(script_state);

  V8CodeCache::ProduceCache(isolate, produce_cache_data_, source_text_length_,
                            SourceURL(), StartPosition());

  produce_cache_data_ = nullptr;
}

void JSModuleScript::Trace(Visitor* visitor) {
  visitor->Trace(produce_cache_data_);
  ModuleScript::Trace(visitor);
}

}  // namespace blink
