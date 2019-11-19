// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/dynamic_module_resolver.h"

#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/bindings/core/v8/referrer_script_info.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
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
                          ScriptPromiseResolver* promise_resolver)
      : url_(url), modulator_(modulator), promise_resolver_(promise_resolver) {}

  void Trace(Visitor*) override;

 private:
  // Implements ModuleTreeClient:
  void NotifyModuleTreeLoadFinished(ModuleScript*) final;

  const KURL url_;
  const Member<Modulator> modulator_;
  const Member<ScriptPromiseResolver> promise_resolver_;
};

// Implements steps 2.[5-8] of
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

  // <spec step="6">If result is null, then:</spec>
  if (!module_script) {
    // <spec step="6.1">Let completion be Completion { [[Type]]: throw,
    // [[Value]]: a new TypeError, [[Target]]: empty }.</spec>
    v8::Local<v8::Value> error = V8ThrowException::CreateTypeError(
        isolate,
        "Failed to fetch dynamically imported module: " + url_.GetString());

    // <spec step="6.2">Perform FinishDynamicImport(referencingScriptOrModule,
    // specifier, promiseCapability, completion).</spec>
    promise_resolver_->Reject(error);

    // <spec step="6.3">Return.</spec>
    return;
  }

  // <spec step="7">Run the module script result, with the rethrow errors
  // boolean set to true.</spec>
  ScriptValue error = modulator_->ExecuteModule(
      module_script, Modulator::CaptureEvalErrorFlag::kCapture);

  // <spec step="8">If running the module script throws an exception, ...</spec>
  if (!error.IsEmpty()) {
    // <spec step="8">... then perform
    // FinishDynamicImport(referencingScriptOrModule, specifier,
    // promiseCapability, the thrown exception completion).</spec>
    //
    // Note: "the thrown exception completion" is |error|.
    //
    // <spec
    // href="https://tc39.github.io/proposal-dynamic-import/#sec-finishdynamicimport"
    // step="1">If completion is an abrupt completion, then perform !
    // Call(promiseCapability.[[Reject]], undefined, « completion.[[Value]]
    // »).</spec>
    promise_resolver_->Reject(error);
    return;
  }

  // <spec step="9">Otherwise, perform
  // FinishDynamicImport(referencingScriptOrModule, specifier,
  // promiseCapability, NormalCompletion(undefined)).</spec>
  //
  // <spec
  // href="https://tc39.github.io/proposal-dynamic-import/#sec-finishdynamicimport"
  // step="2.1">Assert: completion is a normal completion and
  // completion.[[Value]] is undefined.</spec>
  DCHECK(error.IsEmpty());

  // <spec
  // href="https://tc39.github.io/proposal-dynamic-import/#sec-finishdynamicimport"
  // step="2.2">Let moduleRecord be !
  // HostResolveImportedModule(referencingScriptOrModule, specifier).</spec>
  //
  // Note: We skip invocation of ModuleRecordResolver here. The
  // result of HostResolveImportedModule is guaranteed to be |module_script|.
  v8::Local<v8::Module> record = module_script->V8Module();
  DCHECK(!record.IsEmpty());

  // <spec
  // href="https://tc39.github.io/proposal-dynamic-import/#sec-finishdynamicimport"
  // step="2.3">Assert: Evaluate has already been invoked on moduleRecord and
  // successfully completed.</spec>
  //
  // Because |error| is empty, we are sure that ExecuteModule() above was
  // successfully completed.

  // <spec
  // href="https://tc39.github.io/proposal-dynamic-import/#sec-finishdynamicimport"
  // step="2.4">Let namespace be GetModuleNamespace(moduleRecord).</spec>
  v8::Local<v8::Value> module_namespace = ModuleRecord::V8Namespace(record);

  // <spec
  // href="https://tc39.github.io/proposal-dynamic-import/#sec-finishdynamicimport"
  // step="2.5">If namespace is an abrupt completion, perform !
  // Call(promiseCapability.[[Reject]], undefined, « namespace.[[Value]]
  // »).</spec>
  //
  // Note: Blink's implementation never allows |module_namespace| to be
  // an abrupt completion.

  // <spec
  // href="https://tc39.github.io/proposal-dynamic-import/#sec-finishdynamicimport"
  // step="2.6">Otherwise, perform ! Call(promiseCapability.[[Resolve]],
  // undefined, « namespace.[[Value]] »).</spec>
  promise_resolver_->Resolve(module_namespace);
}

void DynamicImportTreeClient::Trace(Visitor* visitor) {
  visitor->Trace(modulator_);
  visitor->Trace(promise_resolver_);
  ModuleTreeClient::Trace(visitor);
}

}  // namespace

void DynamicModuleResolver::Trace(Visitor* visitor) {
  visitor->Trace(modulator_);
}

// <specdef
// href="https://html.spec.whatwg.org/C/#hostimportmoduledynamically(referencingscriptormodule,-specifier,-promisecapability)">
void DynamicModuleResolver::ResolveDynamically(
    const String& specifier,
    const KURL& referrer_resource_url,
    const ReferrerScriptInfo& referrer_info,
    ScriptPromiseResolver* promise_resolver) {
  DCHECK(modulator_->GetScriptState()->GetIsolate()->InContext())
      << "ResolveDynamically should be called from V8 callback, within a valid "
         "context.";

  // https://github.com/WICG/import-maps/blob/master/spec.md#when-import-maps-can-be-encountered
  // Strictly, the flag should be cleared at
  // #internal-module-script-graph-fetching-procedure, i.e. in ModuleTreeLinker,
  // but due to https://crbug.com/928435 https://crbug.com/928564 we also clears
  // the flag here, as import maps can be accessed earlier than specced below
  // (in ResolveModuleSpecifier()) and we need to clear the flag before that.
  modulator_->ClearIsAcquiringImportMaps();

  // <spec step="4.1">Let referencing script be
  // referencingScriptOrModule.[[HostDefined]].</spec>

  // <spec step="4.3">Set base URL to referencing script's base URL.</spec>
  KURL base_url = referrer_info.BaseURL();
  if (base_url.IsNull()) {
    // ReferrerScriptInfo::BaseURL returns null if it should defer to referrer
    // resource url.
    base_url = referrer_resource_url;
  }
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

  // <spec label="fetch-an-import()-module-script-graph" step="1">Let url be the
  // result of resolving a module specifier given base URL and specifier.</spec>
  KURL url = modulator_->ResolveModuleSpecifier(specifier, base_url);

  // <spec label="fetch-an-import()-module-script-graph" step="2">If url is
  // failure, then asynchronously complete this algorithm with null, and abort
  // these steps.</spec>
  if (!url.IsValid()) {
    // <spec step="6">If result is null, then:</spec>
    //
    // <spec step="6.1">Let completion be Completion { [[Type]]: throw,
    // [[Value]]: a new TypeError, [[Target]]: empty }.</spec>
    v8::Isolate* isolate = modulator_->GetScriptState()->GetIsolate();
    v8::Local<v8::Value> error = V8ThrowException::CreateTypeError(
        isolate, "Failed to resolve module specifier '" + specifier + "'");

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
  // TODO(domfarolino): It has not yet been decided how a script's "importance"
  // should affect its dynamic imports. There is discussion at
  // https://github.com/whatwg/html/issues/3670, but for now there is no effect,
  // and dynamic imports get kImportanceAuto. If this changes,
  // ReferrerScriptInfo will need a mojom::FetchImportanceMode member, that must
  // be properly set.
  ScriptFetchOptions options(referrer_info.Nonce(), IntegrityMetadataSet(),
                             String(), referrer_info.ParserState(),
                             referrer_info.CredentialsMode(),
                             referrer_info.GetReferrerPolicy(),
                             mojom::FetchImportanceMode::kImportanceAuto);

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
  if (auto* scope = DynamicTo<WorkerGlobalScope>(*execution_context))
    scope->EnsureFetcher();
  modulator_->FetchTree(url, execution_context->Fetcher(),
                        mojom::RequestContextType::SCRIPT, options,
                        ModuleScriptCustomFetchType::kNone, tree_client);

  // Steps 6-9 are implemented at
  // DynamicImportTreeClient::NotifyModuleLoadFinished.

  // <spec step="10">Return undefined.</spec>
}

}  // namespace blink
