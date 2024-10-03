// Copyright 2017 The Chromium Authors
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
#include "third_party/blink/renderer/core/loader/modulescript/module_script_creation_params.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/script/module_record_resolver.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_fetch_options.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

ModuleRecordProduceCacheData::ModuleRecordProduceCacheData(
    v8::Isolate* isolate,
    CachedMetadataHandler* cache_handler,
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
      unbound_script_.Reset(isolate, unbound_script);
  }
}

void ModuleRecordProduceCacheData::Trace(Visitor* visitor) const {
  visitor->Trace(cache_handler_);
  visitor->Trace(unbound_script_);
}

v8::Local<v8::Module> ModuleRecord::Compile(
    ScriptState* script_state,
    const ModuleScriptCreationParams& params,
    const ScriptFetchOptions& options,
    const TextPosition& text_position,
    mojom::blink::V8CacheOptions v8_cache_options,
    ModuleRecordProduceCacheData** out_produce_cache_data) {
  v8::Isolate* isolate = script_state->GetIsolate();
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
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  if (params.CacheHandler()) {
    params.CacheHandler()->Check(
        ExecutionContext::GetCodeCacheHostFromContext(execution_context),
        params.GetSourceText());
  }
  // TODO(chromium:1406506): Add a compile hints solution for module records.
  constexpr bool kMightGenerateCompileHints = false;
  const bool v8_compile_hints_magic_comment_runtime_enabled =
      RuntimeEnabledFeatures::JavaScriptCompileHintsMagicRuntimeEnabled(
          execution_context);
  std::tie(compile_options, produce_cache_options, no_cache_reason) =
      V8CodeCache::GetCompileOptions(
          v8_cache_options, params.CacheHandler(),
          params.GetSourceText().length(), params.SourceLocationType(),
          params.BaseURL(), kMightGenerateCompileHints,
          v8_compile_hints_magic_comment_runtime_enabled);

  if (!V8ScriptRunner::CompileModule(
           isolate, params, text_position, compile_options, no_cache_reason,
           ReferrerScriptInfo(params.BaseURL(), options))
           .ToLocal(&module)) {
    return v8::Local<v8::Module>();
  }

  if (out_produce_cache_data) {
    *out_produce_cache_data =
        MakeGarbageCollected<ModuleRecordProduceCacheData>(
            isolate, params.CacheHandler(), produce_cache_options, module);
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
  v8::MicrotasksScope microtasks_scope(
      isolate, ToMicrotaskQueue(script_state),
      v8::MicrotasksScope::kDoNotRunMicrotasks);

  // Script IDs are not available on errored modules or on non-source text
  // modules, so we give them a default value.
  probe::ExecuteScript probe(ExecutionContext::From(script_state), context,
                             source_url,
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

  v8::Local<v8::FixedArray> v8_module_requests = record->GetModuleRequests();
  int length = v8_module_requests->Length();
  Vector<ModuleRequest> requests;
  requests.ReserveInitialCapacity(length);
  bool needs_text_position =
      !WTF::IsMainThread() ||
      probe::ToCoreProbeSink(ExecutionContext::From(script_state))
          ->HasDevToolsSessions();

  for (int i = 0; i < length; ++i) {
    v8::Local<v8::ModuleRequest> v8_module_request =
        v8_module_requests->Get(script_state->GetContext(), i)
            .As<v8::ModuleRequest>();
    v8::Local<v8::String> v8_specifier = v8_module_request->GetSpecifier();
    TextPosition position = TextPosition::MinimumPosition();
    if (needs_text_position) {
      // The source position is only used by DevTools for module requests and
      // only visible if devtools is open when the request is initiated.
      // Calculating the source position is not free and V8 has to initialize
      // the line end information for the complete module, thus we try to
      // avoid this additional work here if DevTools is closed.
      int source_offset = v8_module_request->GetSourceOffset();
      v8::Location v8_loc = record->SourceOffsetToLocation(source_offset);
      position = TextPosition(
          OrdinalNumber::FromZeroBasedInt(v8_loc.GetLineNumber()),
          OrdinalNumber::FromZeroBasedInt(v8_loc.GetColumnNumber()));
    }
    Vector<ImportAttribute> import_attributes =
        ModuleRecord::ToBlinkImportAttributes(
            script_state->GetContext(), record,
            v8_module_request->GetImportAttributes(),
            /*v8_import_attributes_has_positions=*/true);

    requests.emplace_back(
        ToCoreString(script_state->GetIsolate(), v8_specifier), position,
        import_attributes);
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
    v8::Local<v8::FixedArray> import_attributes,
    v8::Local<v8::Module> referrer) {
  v8::Isolate* isolate = context->GetIsolate();
  Modulator* modulator = Modulator::From(ScriptState::From(isolate, context));
  DCHECK(modulator);

  ModuleRequest module_request(
      ToCoreStringWithNullCheck(isolate, specifier),
      TextPosition::MinimumPosition(),
      ModuleRecord::ToBlinkImportAttributes(
          context, referrer, import_attributes,
          /*v8_import_attributes_has_positions=*/true));

  ExceptionState exception_state(isolate, v8::ExceptionContext::kOperation,
                                 "ModuleRecord", "resolveModuleCallback");
  v8::Local<v8::Module> resolved =
      modulator->GetModuleRecordResolver()->Resolve(module_request, referrer,
                                                    exception_state);
  DCHECK(!resolved.IsEmpty());
  DCHECK(!exception_state.HadException());

  return resolved;
}

Vector<ImportAttribute> ModuleRecord::ToBlinkImportAttributes(
    v8::Local<v8::Context> context,
    v8::Local<v8::Module> record,
    v8::Local<v8::FixedArray> v8_import_attributes,
    bool v8_import_attributes_has_positions) {
  // If v8_import_attributes_has_positions == true then v8_import_attributes has
  // source position information and is given in the form [key1, value1,
  // source_offset1, key2, value2, source_offset2, ...]. Otherwise if
  // v8_import_attributes_has_positions == false, then v8_import_attributes is
  // in the form [key1, value1, key2, value2, ...].
  const int kV8AttributeEntrySize = v8_import_attributes_has_positions ? 3 : 2;

  v8::Isolate* isolate = context->GetIsolate();
  Vector<ImportAttribute> import_attributes;
  int number_of_import_attributes =
      v8_import_attributes->Length() / kV8AttributeEntrySize;
  import_attributes.ReserveInitialCapacity(number_of_import_attributes);
  for (int i = 0; i < number_of_import_attributes; ++i) {
    v8::Local<v8::String> v8_attribute_key =
        v8_import_attributes->Get(context, i * kV8AttributeEntrySize)
            .As<v8::String>();
    v8::Local<v8::String> v8_attribute_value =
        v8_import_attributes->Get(context, (i * kV8AttributeEntrySize) + 1)
            .As<v8::String>();
    TextPosition attribute_position = TextPosition::MinimumPosition();
    if (v8_import_attributes_has_positions) {
      int32_t v8_attribute_source_offset =
          v8_import_attributes->Get(context, (i * kV8AttributeEntrySize) + 2)
              .As<v8::Int32>()
              ->Value();
      v8::Location v8_attribute_loc =
          record->SourceOffsetToLocation(v8_attribute_source_offset);
      attribute_position = TextPosition(
          OrdinalNumber::FromZeroBasedInt(v8_attribute_loc.GetLineNumber()),
          OrdinalNumber::FromZeroBasedInt(v8_attribute_loc.GetColumnNumber()));
    }

    import_attributes.emplace_back(ToCoreString(isolate, v8_attribute_key),
                                   ToCoreString(isolate, v8_attribute_value),
                                   attribute_position);
  }

  return import_attributes;
}

}  // namespace blink
