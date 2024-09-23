// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/dynamic_module_resolver.h"

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/bindings/core/v8/referrer_script_info.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_creation_params.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_fetch_request.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/script/module_script.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

class DynamicImportTreeClient final : public ModuleTreeClient {
 public:
  DynamicImportTreeClient(const KURL& url,
                          Modulator* modulator,
                          ScriptPromiseResolver<IDLAny>* promise_resolver)
      : url_(url), modulator_(modulator), promise_resolver_(promise_resolver) {}

  void Trace(Visitor*) const override;

 private:
  // Implements ModuleTreeClient:
  void NotifyModuleTreeLoadFinished(ModuleScript*) final;

  const KURL url_;
  const Member<Modulator> modulator_;
  const Member<ScriptPromiseResolver<IDLAny>> promise_resolver_;
};

// Abstract callback for modules resolution.
class ModuleResolutionCallback : public ScriptFunction::Callable {
 public:
  explicit ModuleResolutionCallback(
      ScriptPromiseResolver<IDLAny>* promise_resolver)
      : promise_resolver_(promise_resolver) {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(promise_resolver_);
    ScriptFunction::Callable::Trace(visitor);
  }

 protected:
  Member<ScriptPromiseResolver<IDLAny>> promise_resolver_;
};

// Callback for modules with top-level await.
// Called on successful resolution.
class ModuleResolutionSuccessCallback final : public ModuleResolutionCallback {
 public:
  ModuleResolutionSuccessCallback(
      ScriptPromiseResolver<IDLAny>* promise_resolver,
      ModuleScript* module_script)
      : ModuleResolutionCallback(promise_resolver),
        module_script_(module_script) {}

  void Trace(Visitor* visitor) const final {
    visitor->Trace(module_script_);
    ModuleResolutionCallback::Trace(visitor);
  }

 private:
  ScriptValue Call(ScriptState* script_state, ScriptValue value) override {
    ScriptState::Scope scope(script_state);
    v8::Local<v8::Module> record = module_script_->V8Module();
    v8::Local<v8::Value> module_namespace = ModuleRecord::V8Namespace(record);
    promise_resolver_->Resolve(module_namespace);
    return ScriptValue();
  }

  Member<ModuleScript> module_script_;
};

// Callback for modules with top-level await.
// Called on unsuccessful resolution.
class ModuleResolutionFailureCallback final : public ModuleResolutionCallback {
 public:
  explicit ModuleResolutionFailureCallback(
      ScriptPromiseResolver<IDLAny>* promise_resolver)
      : ModuleResolutionCallback(promise_resolver) {}

 private:
  ScriptValue Call(ScriptState* script_state, ScriptValue exception) override {
    ScriptState::Scope scope(script_state);
    promise_resolver_->Reject(exception);
    return ScriptValue();
  }
};

// Implements steps 2 and 9-10 of
// <specdef
// href="https://html.spec.whatwg.org/C/#hostimportmoduledynamically(referencingscriptormodule,-specifier,-promisecapability)">
void DynamicImportTreeClient::NotifyModuleTreeLoadFinished(
    ModuleScript* module_script) {
  // [nospec] Abort the steps if the browsing context is discarded.
  if (!modulator_->HasValidContext()) {
    // The promise_resolver_ should have ::Detach()-ed at this point,
    // so ::Reject() is not necessary.
    return;
  }

  ScriptState* script_state = modulator_->GetScriptState();
  ScriptState::Scope scope(script_state);
  v8::Isolate* isolate = script_state->GetIsolate();

  // <spec step="2">If settings object's ...</spec>
  if (!module_script) {
    // <spec step="2.1">Let completion be Completion { [[Type]]: throw,
    // [[Value]]: a new TypeError, [[Target]]: empty }.</spec>
    v8::Local<v8::Value> error = V8ThrowException::CreateTypeError(
        isolate,
        "Failed to fetch dynamically imported module: " + url_.GetString());

    // <spec step="2.2">Perform FinishDynamicImport(referencingScriptOrModule,
    // specifier, promiseCapability, completion).</spec>
    promise_resolver_->Reject(error);

    // <spec step="2.3">Return.</spec>
    return;
  }

  // <spec step="9">Otherwise, set promise to the result of running a module
  // script given result and true.</spec>
  ScriptEvaluationResult result =
      module_script->RunScriptOnScriptStateAndReturnValue(
          script_state,
          ExecuteScriptPolicy::kDoNotExecuteScriptWhenScriptsDisabled,
          V8ScriptRunner::RethrowErrorsOption::Rethrow(String()));

  switch (result.GetResultType()) {
    case ScriptEvaluationResult::ResultType::kException:
      // With top-level await, even though according to spec a promise is always
      // returned, the kException case is still reachable when there is a parse
      // or instantiation error.
      promise_resolver_->Reject(result.GetExceptionForModule());
      break;

    case ScriptEvaluationResult::ResultType::kNotRun:
    case ScriptEvaluationResult::ResultType::kAborted:
      // Do nothing when script is disabled or after a script is aborted.
      break;

    case ScriptEvaluationResult::ResultType::kSuccess: {
      // <spec step="10">Perform
      // FinishDynamicImport(referencingScriptOrModule, specifier,
      // promiseCapability, promise).</spec>
      auto* callback_success = MakeGarbageCollected<ScriptFunction>(
          script_state, MakeGarbageCollected<ModuleResolutionSuccessCallback>(
                            promise_resolver_, module_script));
      auto* callback_failure = MakeGarbageCollected<ScriptFunction>(
          script_state, MakeGarbageCollected<ModuleResolutionFailureCallback>(
                            promise_resolver_));
      result.GetPromise(script_state).Then(callback_success, callback_failure);
      break;
    }
  }
}

void DynamicImportTreeClient::Trace(Visitor* visitor) const {
  visitor->Trace(modulator_);
  visitor->Trace(promise_resolver_);
  ModuleTreeClient::Trace(visitor);
}

}  // namespace

void DynamicModuleResolver::Trace(Visitor* visitor) const {
  visitor->Trace(modulator_);
}

// <specdef
// href="https://html.spec.whatwg.org/C/#hostimportmoduledynamically(referencingscriptormodule,-specifier,-promisecapability)">
void DynamicModuleResolver::ResolveDynamically(
    const ModuleRequest& module_request,
    const ReferrerScriptInfo& referrer_info,
    ScriptPromiseResolver<IDLAny>* promise_resolver) {
  DCHECK(modulator_->GetScriptState()->GetIsolate()->InContext())
      << "ResolveDynamically should be called from V8 callback, within a valid "
         "context.";

  // <spec step="4.1">Let referencing script be
  // referencingScriptOrModule.[[HostDefined]].</spec>

  // <spec step="4.3">Set base URL to referencing script's base URL.</spec>
  KURL base_url = referrer_info.BaseURL();
  if (base_url.IsNull()) {
    // The case where "referencing script" doesn't exist.
    //
    // <spec step="1">Let settings object be the current settings object.</spec>
    //
    // <spec step="2">Let base URL be settings object's API base URL.</spec>
    base_url = ExecutionContext::From(modulator_->GetScriptState())->BaseURL();
  }
  DCHECK(!base_url.IsNull());

  // <spec step="5">Fetch an import() module script graph given specifier, base
  // URL, settings object, and fetch options. Wait until the algorithm
  // asynchronously completes with result.</spec>
  //
  // <specdef label="fetch-an-import()-module-script-graph"
  // href="https://html.spec.whatwg.org/C/#fetch-an-import()-module-script-graph">

  // https://wicg.github.io/import-maps/#wait-for-import-maps
  // 1.2. Set document’s acquiring import maps to false. [spec text]
  modulator_->SetAcquiringImportMapsState(
      Modulator::AcquiringImportMapsState::kAfterModuleScriptLoad);

  // <spec label="fetch-an-import()-module-script-graph" step="1">Let url be the
  // result of resolving a module specifier given base URL and specifier.</spec>
  KURL url = modulator_->ResolveModuleSpecifier(
      module_request.specifier, base_url, /*failure_reason=*/nullptr);

  ModuleType module_type = modulator_->ModuleTypeFromRequest(module_request);

  // <spec label="fetch-an-import()-module-script-graph" step="2">If url is
  // failure, then asynchronously complete this algorithm with null, and abort
  // these steps.</spec>
  if (!url.IsValid() || module_type == ModuleType::kInvalid) {
    // <spec step="6">If result is null, then:</spec>
    String error_message;
    if (!url.IsValid()) {
      error_message = "Failed to resolve module specifier '" +
                      module_request.specifier + "'";
      if (referrer_info.BaseURL().IsAboutBlankURL() &&
          base_url.IsAboutBlankURL()) {
        error_message =
            error_message +
            ". The base URL is about:blank because import() is called from a "
            "CORS-cross-origin script.";
      }

    } else {
      error_message = "\"" + module_request.GetModuleTypeString() +
                      "\" is not a valid module type.";
    }

    // <spec step="6.1">Let completion be Completion { [[Type]]: throw,
    // [[Value]]: a new TypeError, [[Target]]: empty }.</spec>
    v8::Isolate* isolate = modulator_->GetScriptState()->GetIsolate();
    v8::Local<v8::Value> error =
        V8ThrowException::CreateTypeError(isolate, error_message);

    // <spec step="6.2">Perform FinishDynamicImport(referencingScriptOrModule,
    // specifier, promiseCapability, completion).</spec>
    //
    // <spec
    // href="https://tc39.github.io/proposal-dynamic-import/#sec-finishdynamicimport"
    // step="1">If completion is an abrupt completion, then perform !
    // Call(promiseCapability.[[Reject]], undefined, « completion.[[Value]]
    // »).</spec>
    promise_resolver->Reject(error);

    // <spec step="6.3">Return.</spec>
    return;
  }

  // <spec step="4.4">Set fetch options to the descendant script fetch options
  // for referencing script's fetch options.</spec>
  //
  // <spec
  // href="https://html.spec.whatwg.org/C/#descendant-script-fetch-options"> For
  // any given script fetch options options, the descendant script fetch options
  // are a new script fetch options whose items all have the same values, except
  // for the integrity metadata, which is instead the empty string.</spec>
  //
  // <spec href="https://wicg.github.io/priority-hints/#script">
  // dynamic imports get kAuto. Only the main script resource is impacted by
  // Priority Hints.
  //
  ScriptFetchOptions options(
      referrer_info.Nonce(), modulator_->GetIntegrityMetadata(url),
      modulator_->GetIntegrityMetadataString(url), referrer_info.ParserState(),
      referrer_info.CredentialsMode(), referrer_info.GetReferrerPolicy(),
      mojom::blink::FetchPriorityHint::kAuto,
      RenderBlockingBehavior::kNonBlocking);

  // <spec label="fetch-an-import()-module-script-graph" step="3">Fetch a single
  // module script given url, settings object, "script", options, settings
  // object, "client", and with the top-level module fetch flag set. If the
  // caller of this algorithm specified custom perform the fetch steps, pass
  // those along as well. Wait until the algorithm asynchronously completes with
  // result.</spec>
  auto* tree_client = MakeGarbageCollected<DynamicImportTreeClient>(
      url, modulator_.Get(), promise_resolver);
  // TODO(kouhei): ExecutionContext::From(modulator_->GetScriptState()) is
  // highly discouraged since it breaks layering. Rewrite this.
  auto* execution_context =
      ExecutionContext::From(modulator_->GetScriptState());
  modulator_->FetchTree(url, module_type, execution_context->Fetcher(),
                        mojom::blink::RequestContextType::SCRIPT,
                        network::mojom::RequestDestination::kScript, options,
                        ModuleScriptCustomFetchType::kNone, tree_client,
                        referrer_info.BaseURL().GetString());

  // Steps 6-9 are implemented at
  // DynamicImportTreeClient::NotifyModuleLoadFinished.

  // <spec step="10">Return undefined.</spec>
}

}  // namespace blink
