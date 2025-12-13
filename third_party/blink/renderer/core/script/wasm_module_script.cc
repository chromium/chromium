// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/wasm_module_script.h"

#include "third_party/blink/renderer/bindings/core/v8/boxed_v8_module.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_creation_params.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/script/module_record_resolver.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "v8/include/v8.h"

namespace blink {

// <specdef
// href="https://html.spec.whatwg.org/C/#creating-a-webassembly-module-script">
WasmModuleScript* WasmModuleScript::Create(
    const ModuleScriptCreationParams& params,
    Modulator* modulator,
    const ScriptFetchOptions& options,
    const TextPosition& start_position) {
  ScriptState* script_state = modulator->GetScriptState();
  ScriptState::Scope scope(script_state);

  // <spec step="1"> If scripting is disabled for settings's then set
  // bodyBytes to the byte sequence 0x00 0x61 0x73 0x6d 0x01 0x00 0x00 0x00.
  // </spec>
  base::span<const uint8_t> source = modulator->IsScriptingDisabled()
                                         ? kEmptyWasmByteSequence
                                         : params.GetWasmSource().as_span();

  // <spec step="2">Let script be a new module script that this algorithm will
  // subsequently initialize.</spec>

  // <spec step="3">Set script's settings object to settings.</spec>
  //
  // Note: "script's settings object" will be |modulator|.

  // <spec step="4">Set script's base URL to baseURL.</spec>
  //
  // <spec step="5">Set script's fetch options to options.</spec>

  // <spec step="7">Let result be ParseModule(source, settings's Realm,
  // script).</spec>
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::TryCatch try_catch(isolate);

  v8::Local<v8::WasmModuleObject> result;
  bool success =
      v8::WasmModuleObject::Compile(
          isolate, v8::MemorySpan<const uint8_t>(source.data(), source.size()))
          .ToLocal(&result);
  // <spec step="8">If the previous step threw an error, then:</spec>
  WasmModuleScript* script = MakeGarbageCollected<WasmModuleScript>(
      modulator, result, params.SourceURL(), params.BaseURL(), options,
      start_position);
  if (try_catch.HasCaught()) {
    DCHECK(!success);
    // <spec step="8.1">Set script's parse error to error.</spec>
    v8::Local<v8::Value> error = try_catch.Exception();
    script->SetParseErrorAndClearRecord(ScriptValue(isolate, error));
    // <spec step="8.2">Return script.</spec>
    return script;
  }

  modulator->GetModuleRecordResolver()->RegisterModuleScript(script);

  return script;
}

WasmModuleScript::WasmModuleScript(Modulator* settings_object,
                                   v8::Local<v8::WasmModuleObject> wasm_module,
                                   const KURL& source_url,
                                   const KURL& base_url,
                                   const ScriptFetchOptions& fetch_options,
                                   const TextPosition& start_position)
    : ModuleScript(settings_object,
                   wasm_module,
                   source_url,
                   base_url,
                   fetch_options,
                   start_position) {}

v8::Local<v8::WasmModuleObject> WasmModuleScript::EmptyModuleForTesting(
    v8::Isolate* isolate) {
  v8::Local<v8::WasmModuleObject> result;
  bool success =
      v8::WasmModuleObject::Compile(
          isolate, v8::MemorySpan<const uint8_t>(kEmptyWasmByteSequence.data(),
                                                 kEmptyWasmByteSequence.size()))
          .ToLocal(&result);
  CHECK(success);
  return result;
}

BoxedV8Module* WasmModuleScript::BoxModuleRecord() const {
  CHECK(!record_.IsEmpty());
  v8::Isolate* isolate = settings_object_->GetScriptState()->GetIsolate();
  v8::Local<v8::WasmModuleObject> record =
      record_.Get(isolate).As<v8::Value>().As<v8::WasmModuleObject>();
  return MakeGarbageCollected<BoxedV8Module>(isolate, record);
}

v8::Local<v8::WasmModuleObject> WasmModuleScript::WasmModule() const {
  if (record_.IsEmpty()) {
    return v8::Local<v8::WasmModuleObject>();
  }
  v8::Isolate* isolate = settings_object_->GetScriptState()->GetIsolate();
  return record_.Get(isolate).As<v8::Value>().As<v8::WasmModuleObject>();
}

ScriptValue WasmModuleScript::Instantiate() const {
  v8::Isolate* isolate = settings_object_->GetScriptState()->GetIsolate();
  v8::Local<v8::Value> error = V8ThrowException::CreateSyntaxError(
      isolate,
      StrCat({SourceUrl().GetString(), kWasmImportInEvaluationPhaseError}));
  return ScriptValue(isolate, error);
}

}  // namespace blink
