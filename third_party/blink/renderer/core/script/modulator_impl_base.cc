// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/modulator_impl_base.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_fetch_request.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_tree_linker.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_tree_linker_registry.h"
#include "third_party/blink/renderer/core/script/dynamic_module_resolver.h"
#include "third_party/blink/renderer/core/script/layered_api.h"
#include "third_party/blink/renderer/core/script/module_map.h"
#include "third_party/blink/renderer/core/script/module_script.h"
#include "third_party/blink/renderer/core/script/script_module_resolver_impl.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"

namespace blink {

ExecutionContext* ModulatorImplBase::GetExecutionContext() const {
  return ExecutionContext::From(script_state_);
}

ModulatorImplBase::ModulatorImplBase(ScriptState* script_state)
    : script_state_(script_state),
      task_runner_(ExecutionContext::From(script_state_)
                       ->GetTaskRunner(TaskType::kNetworking)),
      map_(ModuleMap::Create(this)),
      tree_linker_registry_(ModuleTreeLinkerRegistry::Create()),
      script_module_resolver_(ScriptModuleResolverImpl::Create(
          this,
          ExecutionContext::From(script_state_))),
      dynamic_module_resolver_(DynamicModuleResolver::Create(this)) {
  DCHECK(script_state_);
  DCHECK(task_runner_);
}

ModulatorImplBase::~ModulatorImplBase() {}

bool ModulatorImplBase::IsScriptingDisabled() const {
  return !GetExecutionContext()->CanExecuteScripts(kAboutToExecuteScript);
}

// <specdef label="fetch-a-module-script-tree"
// href="https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-module-script-tree">
// <specdef label="fetch-a-module-worker-script-tree"
// href="https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-module-worker-script-tree">
void ModulatorImplBase::FetchTree(
    const KURL& url,
    FetchClientSettingsObjectSnapshot* fetch_client_settings_object,
    mojom::RequestContextType destination,
    const ScriptFetchOptions& options,
    ModuleScriptCustomFetchType custom_fetch_type,
    ModuleTreeClient* client) {
  // <spec label="fetch-a-module-script-tree" step="2">Perform the internal
  // module script graph fetching procedure given url, settings object,
  // destination, options, settings object, visited set, "client", and with the
  // top-level module fetch flag set. If the caller of this algorithm specified
  // custom perform the fetch steps, pass those along as well.</spec>

  // <spec label="fetch-a-module-worker-script-tree" step="3">Perform the
  // internal module script graph fetching procedure given url, fetch client
  // settings object, destination, options, module map settings object, visited
  // set, "client", and with the top-level module fetch flag set. If the caller
  // of this algorithm specified custom perform the fetch steps, pass those
  // along as well.</spec>

  ModuleTreeLinker::Fetch(url, fetch_client_settings_object, destination,
                          options, this, custom_fetch_type,
                          tree_linker_registry_, client);

  // <spec label="fetch-a-module-script-tree" step="3">When the internal module
  // script graph fetching procedure asynchronously completes with result,
  // asynchronously complete this algorithm with result.</spec>

  // <spec label="fetch-a-module-worker-script-tree" step="4">When the internal
  // module script graph fetching procedure asynchronously completes with
  // result, asynchronously complete this algorithm with result.</spec>

  // Note: We delegate to ModuleTreeLinker to notify ModuleTreeClient.
}

void ModulatorImplBase::FetchDescendantsForInlineScript(
    ModuleScript* module_script,
    FetchClientSettingsObjectSnapshot* fetch_client_settings_object,
    mojom::RequestContextType destination,
    ModuleTreeClient* client) {
  ModuleTreeLinker::FetchDescendantsForInlineScript(
      module_script, fetch_client_settings_object, destination, this,
      ModuleScriptCustomFetchType::kNone, tree_linker_registry_, client);
}

void ModulatorImplBase::FetchSingle(
    const ModuleScriptFetchRequest& request,
    FetchClientSettingsObjectSnapshot* fetch_client_settings_object,
    ModuleGraphLevel level,
    ModuleScriptCustomFetchType custom_fetch_type,
    SingleModuleClient* client) {
  map_->FetchSingleModuleScript(request, fetch_client_settings_object, level,
                                custom_fetch_type, client);
}

ModuleScript* ModulatorImplBase::GetFetchedModuleScript(const KURL& url) {
  return map_->GetFetchedModuleScript(url);
}

// <specdef href="https://html.spec.whatwg.org/#resolve-a-module-specifier">
KURL ModulatorImplBase::ResolveModuleSpecifier(const String& module_request,
                                               const KURL& base_url,
                                               String* failure_reason) {
  // <spec step="1">Apply the URL parser to specifier. If the result is not
  // failure, return the result.</spec>
  KURL url(NullURL(), module_request);
  if (url.IsValid()) {
    // <spec
    // href="https://github.com/drufball/layered-apis/blob/master/spec.md#resolve-a-module-specifier"
    // step="1">Let parsed be the result of applying the URL parser to
    // specifier. If parsed is not failure, then return the layered API fetching
    // URL given parsed and script's base URL.</spec>
    if (RuntimeEnabledFeatures::LayeredAPIEnabled())
      return blink::layered_api::ResolveFetchingURL(url);

    return url;
  }

  // <spec step="2">If specifier does not start with the character U+002F
  // SOLIDUS (/), the two-character sequence U+002E FULL STOP, U+002F SOLIDUS
  // (./), or the three-character sequence U+002E FULL STOP, U+002E FULL STOP,
  // U+002F SOLIDUS (../), return failure.</spec>
  if (!module_request.StartsWith("/") && !module_request.StartsWith("./") &&
      !module_request.StartsWith("../")) {
    if (failure_reason) {
      *failure_reason =
          "Relative references must start with either \"/\", \"./\", or "
          "\"../\".";
    }
    return KURL();
  }

  // <spec step="3">Return the result of applying the URL parser to specifier
  // with base URL as the base URL.</spec>
  DCHECK(base_url.IsValid());
  KURL absolute_url(base_url, module_request);
  if (absolute_url.IsValid())
    return absolute_url;

  if (failure_reason) {
    *failure_reason = "Invalid relative url or base scheme isn't hierarchical.";
  }
  return KURL();
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
  dynamic_module_resolver_->ResolveDynamically(specifier, referrer_url,
                                               referrer_info, resolver);
}

// <specdef href="https://html.spec.whatwg.org/#hostgetimportmetaproperties">
ModuleImportMeta ModulatorImplBase::HostGetImportMetaProperties(
    ScriptModule record) const {
  // <spec step="1">Let module script be moduleRecord.[[HostDefined]].</spec>
  ModuleScript* module_script = script_module_resolver_->GetHostDefined(record);
  DCHECK(module_script);

  // <spec step="2">Let urlString be module script's base URL,
  // serialized.</spec>
  String url_string = module_script->BaseURL().GetString();

  // <spec step="3">Return « Record { [[Key]]: "url", [[Value]]: urlString }
  // ».</spec>
  return ModuleImportMeta(url_string);
}

ScriptValue ModulatorImplBase::InstantiateModule(ScriptModule script_module) {
  ScriptState::Scope scope(script_state_);
  return script_module.Instantiate(script_state_);
}

Vector<Modulator::ModuleRequest>
ModulatorImplBase::ModuleRequestsFromScriptModule(ScriptModule script_module) {
  ScriptState::Scope scope(script_state_);
  Vector<String> specifiers = script_module.ModuleRequests(script_state_);
  Vector<TextPosition> positions =
      script_module.ModuleRequestPositions(script_state_);
  DCHECK_EQ(specifiers.size(), positions.size());
  Vector<ModuleRequest> requests;
  requests.ReserveInitialCapacity(specifiers.size());
  for (wtf_size_t i = 0; i < specifiers.size(); ++i) {
    requests.emplace_back(specifiers[i], positions[i]);
  }
  return requests;
}

// <specdef href="https://html.spec.whatwg.org/#run-a-module-script">
ScriptValue ModulatorImplBase::ExecuteModule(
    const ModuleScript* module_script,
    CaptureEvalErrorFlag capture_error) {
  // <spec step="1">If rethrow errors is not given, let it be false.</spec>

  // <spec step="2">Let settings be the settings object of script.</spec>
  //
  // The settings object is |this|.

  // <spec step="3">Check if we can run script with settings. If this returns
  // "do not run" then return NormalCompletion(empty).</spec>
  if (IsScriptingDisabled())
    return ScriptValue();

  // <spec step="4">Prepare to run script given settings.</spec>
  //
  // This is placed here to also cover ScriptModule::ReportException().
  ScriptState::Scope scope(script_state_);

  // <spec step="5">Let evaluationStatus be null.</spec>
  //
  // |error| corresponds to "evaluationStatus of [[Type]]: throw".
  ScriptValue error;

  // <spec step="6">If script's error to rethrow is not null, ...</spec>
  if (module_script->HasErrorToRethrow()) {
    // <spec step="6">... then set evaluationStatus to Completion { [[Type]]:
    // throw, [[Value]]: script's error to rethrow, [[Target]]: empty }.</spec>
    error = module_script->CreateErrorToRethrow();
  } else {
    // <spec step="7">Otherwise:</spec>

    // <spec step="7.1">Let record be script's record.</spec>
    const ScriptModule& record = module_script->Record();
    CHECK(!record.IsNull());

    // <spec step="7.2">Set evaluationStatus to record.Evaluate(). ...</spec>
    error = record.Evaluate(script_state_);

    // <spec step="7.2">... If Evaluate fails to complete as a result of the
    // user agent aborting the running script, then set evaluationStatus to
    // Completion { [[Type]]: throw, [[Value]]: a new "QuotaExceededError"
    // DOMException, [[Target]]: empty }.</spec>
  }

  // <spec step="8">If evaluationStatus is an abrupt completion, then:</spec>
  if (!error.IsEmpty()) {
    // <spec step="8.1">If rethrow errors is true, rethrow the exception given
    // by evaluationStatus.[[Value]].</spec>
    if (capture_error == CaptureEvalErrorFlag::kCapture)
      return error;

    // <spec step="8.2">Otherwise, report the exception given by
    // evaluationStatus.[[Value]] for script.</spec>
    ScriptModule::ReportException(script_state_, error.V8Value());
  }

  // <spec step="9">Clean up after running script with settings.</spec>
  //
  // Implemented as the ScriptState::Scope destructor.
  return ScriptValue();
}

void ModulatorImplBase::Trace(blink::Visitor* visitor) {
  visitor->Trace(script_state_);
  visitor->Trace(map_);
  visitor->Trace(tree_linker_registry_);
  visitor->Trace(script_module_resolver_);
  visitor->Trace(dynamic_module_resolver_);

  Modulator::Trace(visitor);
}

}  // namespace blink
