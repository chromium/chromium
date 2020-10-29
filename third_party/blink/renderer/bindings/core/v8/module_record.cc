// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/module_record.h"
#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/v8_cache_options.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/boxed_v8_module.h"
#include "third_party/blink/renderer/bindings/core/v8/referrer_script_info.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/script/module_record_resolver.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_fetch_options.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

ModuleRecordProduceCacheData::ModuleRecordProduceCacheData(
    v8::Isolate* isolate,
    SingleCachedMetadataHandler* cache_handler,
    V8CodeCache::ProduceCacheOptions produce_cache_options,
    v8::Local<v8::Module> module)
    : cache_handler_(cache_handler),
      produce_cache_options_(produce_cache_options) {
  v8::HandleScope scope(isolate);

  if (produce_cache_options ==
          V8CodeCache::ProduceCacheOptions::kProduceCodeCache &&
      module->GetStatus() == v8::Module::kUninstantiated) {
    v8::Local<v8::UnboundModuleScript> unbound_script =
        module->GetUnboundModuleScript();
    if (!unbound_script.IsEmpty())
      unbound_script_.Set(isolate, unbound_script);
  }
}

void ModuleRecordProduceCacheData::Trace(Visitor* visitor) const {
  visitor->Trace(cache_handler_);
  visitor->Trace(unbound_script_.UnsafeCast<v8::Value>());
}

v8::Local<v8::Module> ModuleRecord::Compile(
    v8::Isolate* isolate,
    const String& source,
    const KURL& source_url,
    const KURL& base_url,
    const ScriptFetchOptions& options,
    const TextPosition& text_position,
    ExceptionState& exception_state,
    mojom::blink::V8CacheOptions v8_cache_options,
    SingleCachedMetadataHandler* cache_handler,
    ScriptSourceLocationType source_location_type,
    ModuleRecordProduceCacheData** out_produce_cache_data) {
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Module> module;

  // Module scripts currently don't support |kEagerCompile| which can be
  // used for |mojom::blink::V8CacheOptions::kFullCodeWithoutHeatCheck|, so use
  // |mojom::blink::V8CacheOptions::kCodeWithoutHeatCheck| instead.
  if (v8_cache_options ==
      mojom::blink::V8CacheOptions::kFullCodeWithoutHeatCheck) {
    v8_cache_options = mojom::blink::V8CacheOptions::kCodeWithoutHeatCheck;
  }

  v8::ScriptCompiler::CompileOptions compile_options;
  V8CodeCache::ProduceCacheOptions produce_cache_options;
  v8::ScriptCompiler::NoCacheReason no_cache_reason;
  std::tie(compile_options, produce_cache_options, no_cache_reason) =
      V8CodeCache::GetCompileOptions(v8_cache_options, cache_handler,
                                     source.length(), source_location_type);

  if (!V8ScriptRunner::CompileModule(
           isolate, source, cache_handler, source_url, text_position,
           compile_options, no_cache_reason,
           ReferrerScriptInfo(base_url, options,
                              ReferrerScriptInfo::BaseUrlSource::kOther))
           .ToLocal(&module)) {
    DCHECK(try_catch.HasCaught());
    exception_state.RethrowV8Exception(try_catch.Exception());
    return v8::Local<v8::Module>();
  }
  DCHECK(!try_catch.HasCaught());

  if (out_produce_cache_data) {
    *out_produce_cache_data =
        MakeGarbageCollected<ModuleRecordProduceCacheData>(
            isolate, cache_handler, produce_cache_options, module);
  }

  return module;
}

ScriptValue ModuleRecord::Instantiate(ScriptState* script_state,
                                      v8::Local<v8::Module> record,
                                      const KURL& source_url) {
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);

  DCHECK(!record.IsEmpty());
  v8::Local<v8::Context> context = script_state->GetContext();

  // Script IDs are not available on errored modules or on non-source text
  // modules, so we give them a default value.
  probe::ExecuteScript probe(ExecutionContext::From(script_state), source_url,
                             record->GetStatus() != v8::Module::kErrored &&
                                     record->IsSourceTextModule()
                                 ? record->ScriptId()
                                 : v8::UnboundScript::kNoScriptId);
  bool success;
  if (!record->InstantiateModule(context, &ResolveModuleCallback)
           .To(&success) ||
      !success) {
    DCHECK(try_catch.HasCaught());
    return ScriptValue(isolate, try_catch.Exception());
  }
  DCHECK(!try_catch.HasCaught());
  return ScriptValue();
}

void ModuleRecord::ReportException(ScriptState* script_state,
                                   v8::Local<v8::Value> exception) {
  V8ScriptRunner::ReportException(script_state->GetIsolate(), exception);
}

Vector<ModuleRequest> ModuleRecord::ModuleRequests(
    ScriptState* script_state,
    v8::Local<v8::Module> record) {
  if (record.IsEmpty())
    return Vector<ModuleRequest>();

  Vector<ModuleRequest> requests;
  int length = record->GetModuleRequestsLength();
  requests.ReserveInitialCapacity(length);

  for (int i = 0; i < length; ++i) {
    v8::Local<v8::String> v8_name = record->GetModuleRequest(i);
    v8::Location v8_loc = record->GetModuleRequestLocation(i);
    TextPosition position(
        OrdinalNumber::FromZeroBasedInt(v8_loc.GetLineNumber()),
        OrdinalNumber::FromZeroBasedInt(v8_loc.GetColumnNumber()));
    requests.emplace_back(ToCoreString(v8_name), position);
  }
  return requests;
}

v8::Local<v8::Value> ModuleRecord::V8Namespace(v8::Local<v8::Module> record) {
  DCHECK(!record.IsEmpty());
  return record->GetModuleNamespace();
}

v8::MaybeLocal<v8::Module> ModuleRecord::ResolveModuleCallback(
    v8::Local<v8::Context> context,
    v8::Local<v8::String> specifier,
    v8::Local<v8::Module> referrer) {
  v8::Isolate* isolate = context->GetIsolate();
  Modulator* modulator = Modulator::From(ScriptState::From(context));
  DCHECK(modulator);

  ExceptionState exception_state(isolate, ExceptionState::kExecutionContext,
                                 "ModuleRecord", "resolveModuleCallback");
  v8::Local<v8::Module> resolved =
      modulator->GetModuleRecordResolver()->Resolve(
          ToCoreStringWithNullCheck(specifier), referrer, exception_state);
  DCHECK(!resolved.IsEmpty());
  DCHECK(!exception_state.HadException());
  return resolved;
}

}  // namespace blink
