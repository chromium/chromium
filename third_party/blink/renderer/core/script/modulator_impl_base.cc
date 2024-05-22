// Copyright 2017 The Chromium Authors
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
#include "third_party/blink/renderer/core/loader/modulescript/module_script_creation_params.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_fetch_request.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_tree_linker.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_tree_linker_registry.h"
#include "third_party/blink/renderer/core/loader/subresource_integrity_helper.h"
#include "third_party/blink/renderer/core/script/dynamic_module_resolver.h"
#include "third_party/blink/renderer/core/script/import_map.h"
#include "third_party/blink/renderer/core/script/js_module_script.h"
#include "third_party/blink/renderer/core/script/module_map.h"
#include "third_party/blink/renderer/core/script/module_record_resolver_impl.h"
#include "third_party/blink/renderer/core/script/parsed_specifier.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/subresource_integrity.h"

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

bool ModulatorImplBase::IsScriptingDisabled() const {
  return !GetExecutionContext()->CanExecuteScripts(kAboutToExecuteScript);
}

mojom::blink::V8CacheOptions ModulatorImplBase::GetV8CacheOptions() const {
  return GetExecutionContext()->GetV8CacheOptions();
}

// <specdef label="fetch-a-module-script-tree"
// href="https://html.spec.whatwg.org/C/#fetch-a-module-script-tree">
// <specdef label="fetch-a-module-worker-script-tree"
// href="https://html.spec.whatwg.org/C/#fetch-a-module-worker-script-tree">
void ModulatorImplBase::FetchTree(
    const KURL& url,
    ModuleType module_type,
    ResourceFetcher* fetch_client_settings_object_fetcher,
    mojom::blink::RequestContextType context_type,
    network::mojom::RequestDestination destination,
    const ScriptFetchOptions& options,
    ModuleScriptCustomFetchType custom_fetch_type,
    ModuleTreeClient* client,
    String referrer) {
  tree_linker_registry_->Fetch(
      url, module_type, fetch_client_settings_object_fetcher, context_type,
      destination, options, this, custom_fetch_type, client, referrer);
}

void ModulatorImplBase::FetchDescendantsForInlineScript(
    ModuleScript* module_script,
    ResourceFetcher* fetch_client_settings_object_fetcher,
    mojom::blink::RequestContextType context_type,
    network::mojom::RequestDestination destination,
    ModuleTreeClient* client) {
  tree_linker_registry_->FetchDescendantsForInlineScript(
      module_script, fetch_client_settings_object_fetcher, context_type,
      destination, this, ModuleScriptCustomFetchType::kNone, client);
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

ModuleScript* ModulatorImplBase::GetFetchedModuleScript(
    const KURL& url,
    ModuleType module_type) {
  return map_->GetFetchedModuleScript(url, module_type);
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

  std::optional<KURL> mapped_url;
  if (GetImportMap()) {
    String import_map_debug_message;
    mapped_url = GetImportMap()->Resolve(parsed_specifier, base_url,
                                         &import_map_debug_message);

    // Output the resolution log. This is too verbose to be always shown, but
    // will be helpful for Web developers (and also Chromium developers) for
    // debugging import maps.
    VLOG(1) << import_map_debug_message;

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
      NOTREACHED_IN_MIGRATION();
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

bool ModulatorImplBase::HasValidContext() {
  return script_state_->ContextIsValid();
}

void ModulatorImplBase::ResolveDynamically(
    const ModuleRequest& module_request,
    const ReferrerScriptInfo& referrer_info,
    ScriptPromiseResolver<IDLAny>* resolver) {
  String reason;
  if (IsDynamicImportForbidden(&reason)) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        GetScriptState()->GetIsolate(), reason));
    return;
  }
  UseCounter::Count(GetExecutionContext(),
                    WebFeature::kDynamicImportModuleScript);
  dynamic_module_resolver_->ResolveDynamically(module_request, referrer_info,
                                               resolver);
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
  String url_string = module_script->BaseUrl().GetString();

  // <spec step="4">Return « Record { [[Key]]: "url", [[Value]]: urlString }
  // ».</spec>
  return ModuleImportMeta(url_string);
}

String ModulatorImplBase::GetIntegrityMetadataString(const KURL& url) const {
  const ImportMap* import_map = GetImportMap();
  if (!import_map) {
    return String();
  }
  return import_map->GetIntegrity(url);
}

IntegrityMetadataSet ModulatorImplBase::GetIntegrityMetadata(
    const KURL& url) const {
  String value = GetIntegrityMetadataString(url);
  IntegrityMetadataSet integrity_metadata;
  if (!value.IsNull()) {
    SubresourceIntegrity::ReportInfo report_info;
    SubresourceIntegrity::ParseIntegrityAttribute(
        value, SubresourceIntegrity::IntegrityFeatures::kDefault,
        integrity_metadata, &report_info);
    SubresourceIntegrityHelper::DoReport(*GetExecutionContext(), report_info);
  }
  return integrity_metadata;
}

ModuleType ModulatorImplBase::ModuleTypeFromRequest(
    const ModuleRequest& module_request) const {
  String module_type_string = module_request.GetModuleTypeString();
  if (module_type_string.IsNull()) {
    // <spec href="https://html.spec.whatwg.org/#fetch-a-single-module-script"
    // step="1">Let module type be "javascript".</spec> If no type assertion is
    // provided, the import is treated as a JavaScript module.
    return ModuleType::kJavaScript;
  } else if (module_type_string == "json") {
    // <spec href="https://html.spec.whatwg.org/#fetch-a-single-module-script"
    // step="17"> If...module type is "json", then set module script to the
    // result of creating a JSON module script...</spec>
    return ModuleType::kJSON;
  } else if (module_type_string == "css" && GetExecutionContext()->IsWindow()) {
    // <spec href="https://html.spec.whatwg.org/#fetch-a-single-module-script"
    // step="16"> If...module type is "css", then set module script to the
    // result of creating a CSS module script...</spec>
    return ModuleType::kCSS;
  } else {
    // Per https://github.com/whatwg/html/pull/7066, unrecognized type
    // assertions or "css" type assertions in a non-document context should be
    // treated as an error similar to an invalid module specifier.
    return ModuleType::kInvalid;
  }
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
      ModuleRecord::ModuleRequests(GetScriptState(), record);

  for (const auto& module_request : child_specifiers) {
    KURL child_url =
        module_script->ResolveModuleSpecifier(module_request.specifier);

    ModuleType child_module_type = ModuleTypeFromRequest(module_request);
    CHECK_NE(child_module_type, ModuleType::kInvalid);

    CHECK(child_url.IsValid())
        << "ModuleScript::ResolveModuleSpecifier() impl must "
           "return a valid url.";

    ModuleScript* child_module =
        GetFetchedModuleScript(child_url, child_module_type);
    CHECK(child_module);

    if (discovered_set->Contains(child_module))
      continue;

    ProduceCacheModuleTree(child_module, discovered_set);
  }
}

void ModulatorImplBase::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  visitor->Trace(map_);
  visitor->Trace(tree_linker_registry_);
  visitor->Trace(module_record_resolver_);
  visitor->Trace(dynamic_module_resolver_);

  Modulator::Trace(visitor);
}

}  // namespace blink
