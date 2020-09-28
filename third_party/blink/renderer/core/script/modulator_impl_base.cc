// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/modulator_impl_base.h"
#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/module_record.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_fetch_request.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_tree_linker.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_tree_linker_registry.h"
#include "third_party/blink/renderer/core/script/dynamic_module_resolver.h"
#include "third_party/blink/renderer/core/script/import_map.h"
#include "third_party/blink/renderer/core/script/js_module_script.h"
#include "third_party/blink/renderer/core/script/module_map.h"
#include "third_party/blink/renderer/core/script/module_record_resolver_impl.h"
#include "third_party/blink/renderer/core/script/parsed_specifier.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

ExecutionContext* ModulatorImplBase::GetExecutionContext() const {
  return ExecutionContext::From(script_state_);
}

ModulatorImplBase::ModulatorImplBase(ScriptState* script_state)
    : script_state_(script_state),
      task_runner_(ExecutionContext::From(script_state_)
                       ->GetTaskRunner(TaskType::kNetworking)),
      map_(MakeGarbageCollected<ModuleMap>(this)),
      tree_linker_registry_(MakeGarbageCollected<ModuleTreeLinkerRegistry>()),
      module_record_resolver_(MakeGarbageCollected<ModuleRecordResolverImpl>(
          this,
          ExecutionContext::From(script_state_))),
      dynamic_module_resolver_(
          MakeGarbageCollected<DynamicModuleResolver>(this)) {
  DCHECK(script_state_);
  DCHECK(task_runner_);
}

ModulatorImplBase::~ModulatorImplBase() {}

class ModuleEvaluationRejectionCallback final : public ScriptFunction {
 public:
  explicit ModuleEvaluationRejectionCallback(ScriptState* script_state)
      : ScriptFunction(script_state) {}

  static v8::Local<v8::Function> CreateFunction(ScriptState* script_state) {
    ModuleEvaluationRejectionCallback* self =
        MakeGarbageCollected<ModuleEvaluationRejectionCallback>(script_state);
    return self->BindToV8Function();
  }

 private:
  ScriptValue Call(ScriptValue value) override {
    ModuleRecord::ReportException(GetScriptState(), value.V8Value());
    return ScriptValue();
  }
};

bool ModulatorImplBase::IsScriptingDisabled() const {
  return !GetExecutionContext()->CanExecuteScripts(kAboutToExecuteScript);
}

bool ModulatorImplBase::ImportMapsEnabled() const {
  return RuntimeEnabledFeatures::ImportMapsEnabled(GetExecutionContext());
}

// <specdef label="fetch-a-module-script-tree"
// href="https://html.spec.whatwg.org/C/#fetch-a-module-script-tree">
// <specdef label="fetch-a-module-worker-script-tree"
// href="https://html.spec.whatwg.org/C/#fetch-a-module-worker-script-tree">
void ModulatorImplBase::FetchTree(
    const KURL& url,
    ResourceFetcher* fetch_client_settings_object_fetcher,
    mojom::RequestContextType context_type,
    network::mojom::RequestDestination destination,
    const ScriptFetchOptions& options,
    ModuleScriptCustomFetchType custom_fetch_type,
    ModuleTreeClient* client) {
  ModuleTreeLinker::Fetch(url, fetch_client_settings_object_fetcher,
                          context_type, destination, options, this,
                          custom_fetch_type, tree_linker_registry_, client);
}

void ModulatorImplBase::FetchDescendantsForInlineScript(
    ModuleScript* module_script,
    ResourceFetcher* fetch_client_settings_object_fetcher,
    mojom::RequestContextType context_type,
    network::mojom::RequestDestination destination,
    ModuleTreeClient* client) {
  ModuleTreeLinker::FetchDescendantsForInlineScript(
      module_script, fetch_client_settings_object_fetcher, context_type,
      destination, this, ModuleScriptCustomFetchType::kNone,
      tree_linker_registry_, client);
}

void ModulatorImplBase::FetchSingle(
    const ModuleScriptFetchRequest& request,
    ResourceFetcher* fetch_client_settings_object_fetcher,
    ModuleGraphLevel level,
    ModuleScriptCustomFetchType custom_fetch_type,
    SingleModuleClient* client) {
  map_->FetchSingleModuleScript(request, fetch_client_settings_object_fetcher,
                                level, custom_fetch_type, client);
}

ModuleScript* ModulatorImplBase::GetFetchedModuleScript(const KURL& url) {
  return map_->GetFetchedModuleScript(url);
}

// <specdef href="https://html.spec.whatwg.org/C/#resolve-a-module-specifier">
KURL ModulatorImplBase::ResolveModuleSpecifier(const String& specifier,
                                               const KURL& base_url,
                                               String* failure_reason) {
  ParsedSpecifier parsed_specifier =
      ParsedSpecifier::Create(specifier, base_url);

  if (!parsed_specifier.IsValid()) {
    if (failure_reason) {
      *failure_reason =
          "Invalid relative url or base scheme isn't hierarchical.";
    }
    return KURL();
  }

  // If |logger| is non-null, outputs detailed logs.
  // The detailed log should be useful for debugging particular import maps
  // errors, but should be supressed (i.e. |logger| should be null) in normal
  // cases.

  base::Optional<KURL> mapped_url;
  if (import_map_) {
    String import_map_debug_message;
    mapped_url = import_map_->Resolve(parsed_specifier, base_url,
                                      &import_map_debug_message);

    // Output the resolution log. This is too verbose to be always shown, but
    // will be helpful for Web developers (and also Chromium developers) for
    // debugging import maps.
    LOG(INFO) << import_map_debug_message;

    if (mapped_url) {
      KURL url = *mapped_url;
      if (!url.IsValid()) {
        if (failure_reason)
          *failure_reason = import_map_debug_message;
        return KURL();
      }
      return url;
    }
  }

  // The specifier is not mapped by import maps, either because
  // - There are no import maps, or
  // - The import map doesn't have an entry for |parsed_specifier|.

  switch (parsed_specifier.GetType()) {
    case ParsedSpecifier::Type::kInvalid:
      NOTREACHED();
      return KURL();

    case ParsedSpecifier::Type::kBare:
      // Reject bare specifiers as specced by the pre-ImportMap spec.
      if (failure_reason) {
        *failure_reason =
            "Relative references must start with either \"/\", \"./\", or "
            "\"../\".";
      }
      return KURL();

    case ParsedSpecifier::Type::kURL:
      return parsed_specifier.GetUrl();
  }
}

ScriptValue ModulatorImplBase::CreateTypeError(const String& message) const {
  ScriptState::Scope scope(script_state_);
  ScriptValue error(
      script_state_->GetIsolate(),
      V8ThrowException::CreateTypeError(script_state_->GetIsolate(), message));
  return error;
}

ScriptValue ModulatorImplBase::CreateSyntaxError(const String& message) const {
  ScriptState::Scope scope(script_state_);
  ScriptValue error(script_state_->GetIsolate(),
                    V8ThrowException::CreateSyntaxError(
                        script_state_->GetIsolate(), message));
  return error;
}

// <specdef href="https://wicg.github.io/import-maps/#register-an-import-map">
void ModulatorImplBase::RegisterImportMap(const ImportMap* import_map,
                                          ScriptValue error_to_rethrow) {
  DCHECK(import_map);
  DCHECK(ImportMapsEnabled());

  // <spec step="7">If import map parse result’s error to rethrow is not null,
  // then:</spec>
  if (!error_to_rethrow.IsEmpty()) {
    // <spec step="7.1">Report the exception given import map parse result’s
    // error to rethrow. ...</spec>
    if (!IsScriptingDisabled()) {
      ScriptState::Scope scope(script_state_);
      ModuleRecord::ReportException(script_state_, error_to_rethrow.V8Value());
    }

    // <spec step="7.2">Return.</spec>
    return;
  }

  // <spec step="8">Update element’s node document's import map with import map
  // parse result’s import map.</spec>
  //
  // TODO(crbug.com/927119): Implement merging. Currently only one import map is
  // allowed.
  if (import_map_) {
    GetExecutionContext()->AddConsoleMessage(
        mojom::ConsoleMessageSource::kOther, mojom::ConsoleMessageLevel::kError,
        "Multiple import maps are not yet supported. https://crbug.com/927119");
    return;
  }

  import_map_ = import_map;
}

bool ModulatorImplBase::HasValidContext() {
  return script_state_->ContextIsValid();
}

void ModulatorImplBase::ResolveDynamically(
    const String& specifier,
    const KURL& referrer_url,
    const ReferrerScriptInfo& referrer_info,
    ScriptPromiseResolver* resolver) {
  String reason;
  if (IsDynamicImportForbidden(&reason)) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        GetScriptState()->GetIsolate(), reason));
    return;
  }
  UseCounter::Count(GetExecutionContext(),
                    WebFeature::kDynamicImportModuleScript);
  dynamic_module_resolver_->ResolveDynamically(specifier, referrer_url,
                                               referrer_info, resolver);
}

// <specdef href="https://html.spec.whatwg.org/C/#hostgetimportmetaproperties">
ModuleImportMeta ModulatorImplBase::HostGetImportMetaProperties(
    v8::Local<v8::Module> record) const {
  // <spec step="1">Let module script be moduleRecord.[[HostDefined]].</spec>
  const ModuleScript* module_script =
      module_record_resolver_->GetModuleScriptFromModuleRecord(record);
  DCHECK(module_script);

  // <spec step="3">Let urlString be module script's base URL,
  // serialized.</spec>
  String url_string = module_script->BaseURL().GetString();

  // <spec step="4">Return « Record { [[Key]]: "url", [[Value]]: urlString }
  // ».</spec>
  return ModuleImportMeta(url_string);
}

ScriptValue ModulatorImplBase::InstantiateModule(
    v8::Local<v8::Module> module_record,
    const KURL& source_url) {
  UseCounter::Count(GetExecutionContext(),
                    WebFeature::kInstantiateModuleScript);

  ScriptState::Scope scope(script_state_);
  return ModuleRecord::Instantiate(script_state_, module_record, source_url);
}

Vector<ModuleRequest> ModulatorImplBase::ModuleRequestsFromModuleRecord(
    v8::Local<v8::Module> module_record) {
  ScriptState::Scope scope(script_state_);
  return ModuleRecord::ModuleRequests(script_state_, module_record);
}

void ModulatorImplBase::ProduceCacheModuleTreeTopLevel(
    ModuleScript* module_script) {
  DCHECK(module_script);
  // Since we run this asynchronously, context might be gone already,
  // for example because the frame was detached.
  if (!script_state_->ContextIsValid())
    return;
  HeapHashSet<Member<const ModuleScript>> discovered_set;
  ProduceCacheModuleTree(module_script, &discovered_set);
}

void ModulatorImplBase::ProduceCacheModuleTree(
    ModuleScript* module_script,
    HeapHashSet<Member<const ModuleScript>>* discovered_set) {
  DCHECK(module_script);

  v8::Isolate* isolate = GetScriptState()->GetIsolate();
  v8::HandleScope scope(isolate);

  discovered_set->insert(module_script);

  v8::Local<v8::Module> record = module_script->V8Module();
  DCHECK(!record.IsEmpty());

  module_script->ProduceCache();

  Vector<ModuleRequest> child_specifiers =
      ModuleRequestsFromModuleRecord(record);

  for (const auto& module_request : child_specifiers) {
    KURL child_url =
        module_script->ResolveModuleSpecifier(module_request.specifier);

    CHECK(child_url.IsValid())
        << "ModuleScript::ResolveModuleSpecifier() impl must "
           "return a valid url.";

    ModuleScript* child_module = GetFetchedModuleScript(child_url);
    CHECK(child_module);

    if (discovered_set->Contains(child_module))
      continue;

    ProduceCacheModuleTree(child_module, discovered_set);
  }
}

// <specdef href="https://html.spec.whatwg.org/C/#run-a-module-script">
// Spec with TLA: https://github.com/whatwg/html/pull/4352
ModuleEvaluationResult ModulatorImplBase::ExecuteModule(
    ModuleScript* module_script,
    CaptureEvalErrorFlag capture_error) {
  // <spec step="1">If rethrow errors is not given, let it be false.</spec>

  // <spec step="2">Let settings be the settings object of script.</spec>
  //
  // The settings object is |this|.

  // <spec step="3">Check if we can run script with settings. If this returns
  // "do not run" then return NormalCompletion(empty).</spec>
  if (IsScriptingDisabled())
    return ModuleEvaluationResult::Empty();

  // <spec step="4">Prepare to run script given settings.</spec>
  //
  // These are placed here to also cover ModuleRecord::ReportException().
  v8::Isolate* isolate = script_state_->GetIsolate();
  v8::MicrotasksScope microtasks_scope(isolate,
                                       ToMicrotaskQueue(GetExecutionContext()),
                                       v8::MicrotasksScope::kRunMicrotasks);
  ScriptState::EscapableScope scope(script_state_);

  // Without TLA: <spec step="5">Let evaluationStatus be null.</spec>
  ModuleEvaluationResult result = ModuleEvaluationResult::Empty();

  // <spec step="6">If script's error to rethrow is not null, ...</spec>
  if (module_script->HasErrorToRethrow()) {
    // Without TLA: <spec step="6">... then set evaluationStatus to Completion
    //     { [[Type]]: throw, [[Value]]: script's error to rethrow,
    //       [[Target]]: empty }.</spec>
    // With TLA:    <spec step="5">If script's error to rethrow is not null,
    //     then let valuationPromise be a promise rejected with script's error
    //     to rethrow.</spec>
    result = ModuleEvaluationResult::FromException(
        module_script->CreateErrorToRethrow().V8Value());
  } else {
    // <spec step="7">Otherwise:</spec>

    // <spec step="7.1">Let record be script's record.</spec>
    v8::Local<v8::Module> record = module_script->V8Module();
    CHECK(!record.IsEmpty());

    // <spec step="7.2">Set evaluationStatus to record.Evaluate(). ...</spec>
    result = ModuleRecord::Evaluate(script_state_, record,
                                    module_script->SourceURL());

    // <spec step="7.2">... If Evaluate fails to complete as a result of the
    // user agent aborting the running script, then set evaluationStatus to
    // Completion { [[Type]]: throw, [[Value]]: a new "QuotaExceededError"
    // DOMException, [[Target]]: empty }.</spec>

    // [not specced] Store V8 code cache on successful evaluation.
    if (result.IsSuccess()) {
      TaskRunner()->PostTask(
          FROM_HERE,
          WTF::Bind(&ModulatorImplBase::ProduceCacheModuleTreeTopLevel,
                    WrapWeakPersistent(this), WrapPersistent(module_script)));
    }
  }

  if (base::FeatureList::IsEnabled(features::kTopLevelAwait)) {
    if (capture_error == CaptureEvalErrorFlag::kReport) {
      // <spec step="7"> If report errors is true, then upon rejection of
      // evaluationPromise with reason, report the exception given by reason
      // for script.</spec>
      v8::Local<v8::Function> callback_failure =
          ModuleEvaluationRejectionCallback::CreateFunction(script_state_);
      // Add a rejection handler to report back errors once the result promise
      // is rejected.
      result.GetPromise(script_state_)
          .Then(v8::Local<v8::Function>(), callback_failure);
    }
  } else {
    // <spec step="8">If evaluationStatus is an abrupt completion, then:</spec>
    if (result.IsException()) {
      // <spec step="8.1">If rethrow errors is true, rethrow the exception given
      // by evaluationStatus.[[Value]].</spec>
      if (capture_error == CaptureEvalErrorFlag::kReport) {
        // <spec step="8.2">Otherwise, report the exception given by
        // evaluationStatus.[[Value]] for script.</spec>
        ModuleRecord::ReportException(script_state_, result.GetException());
      }
    }
  }

  // <spec step="8">Clean up after running script with settings.</spec>
  // - Partially implement in MicrotaskScope destructor and the
  // - ScriptState::EscapableScope destructor.
  return result.Escape(&scope);
}

void ModulatorImplBase::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  visitor->Trace(map_);
  visitor->Trace(tree_linker_registry_);
  visitor->Trace(module_record_resolver_);
  visitor->Trace(dynamic_module_resolver_);
  visitor->Trace(import_map_);

  Modulator::Trace(visitor);
}

}  // namespace blink
