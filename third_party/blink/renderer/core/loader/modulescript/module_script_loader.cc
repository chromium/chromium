// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/modulescript/module_script_loader.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/dom_implementation.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_fetcher.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_loader_client.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_loader_registry.h"
#include "third_party/blink/renderer/core/script/js_module_script.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/script/value_wrapper_synthetic_module_script.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loading_log.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

ModuleScriptLoader::ModuleScriptLoader(Modulator* modulator,
                                       const ScriptFetchOptions& options,
                                       ModuleScriptLoaderRegistry* registry,
                                       ModuleScriptLoaderClient* client)
    : modulator_(modulator),
      options_(options),
      registry_(registry),
      client_(client) {
  DCHECK(modulator);
  DCHECK(registry);
  DCHECK(client);
}

ModuleScriptLoader::~ModuleScriptLoader() = default;

#if DCHECK_IS_ON()
const char* ModuleScriptLoader::StateToString(ModuleScriptLoader::State state) {
  switch (state) {
    case State::kInitial:
      return "Initial";
    case State::kFetching:
      return "Fetching";
    case State::kFinished:
      return "Finished";
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}
#endif

void ModuleScriptLoader::AdvanceState(ModuleScriptLoader::State new_state) {
  switch (state_) {
    case State::kInitial:
      DCHECK_EQ(new_state, State::kFetching);
      break;
    case State::kFetching:
      DCHECK_EQ(new_state, State::kFinished);
      break;
    case State::kFinished:
      NOTREACHED_IN_MIGRATION();
      break;
  }

#if DCHECK_IS_ON()
  RESOURCE_LOADING_DVLOG(1)
      << "ModuleLoader[" << url_.GetString() << "]::advanceState("
      << StateToString(state_) << " -> " << StateToString(new_state) << ")";
#endif
  state_ = new_state;

  if (state_ == State::kFinished) {
    registry_->ReleaseFinishedLoader(this);
    client_->NotifyNewSingleModuleFinished(module_script_);
  }
}

void ModuleScriptLoader::Fetch(
    const ModuleScriptFetchRequest& module_request,
    ResourceFetcher* fetch_client_settings_object_fetcher,
    ModuleGraphLevel level,
    Modulator* module_map_settings_object,
    ModuleScriptCustomFetchType custom_fetch_type,
    ModuleScriptLoaderRegistry* registry,
    ModuleScriptLoaderClient* client) {
  ModuleScriptLoader* loader = MakeGarbageCollected<ModuleScriptLoader>(
      module_map_settings_object, module_request.Options(), registry, client);
  registry->AddLoader(loader);
  loader->FetchInternal(module_request, fetch_client_settings_object_fetcher,
                        level, custom_fetch_type);
}

// <specdef
// href="https://html.spec.whatwg.org/C/#fetch-destination-from-module-type">
void SetFetchDestinationFromModuleType(
    ResourceRequest& resource_request,
    const ModuleScriptFetchRequest& module_request) {
  if (!base::FeatureList::IsEnabled(
          features::kFetchDestinationJsonCssModules)) {
    resource_request.SetRequestContext(module_request.ContextType());
    resource_request.SetRequestDestination(module_request.Destination());
    return;
  }

  switch (module_request.GetExpectedModuleType()) {
    case ModuleType::kCSS:
      resource_request.SetRequestContext(
          mojom::blink::RequestContextType::STYLE);
      resource_request.SetRequestDestination(
          network::mojom::RequestDestination::kStyle);
      break;
    case ModuleType::kJSON:
      resource_request.SetRequestContext(
          mojom::blink::RequestContextType::JSON);
      resource_request.SetRequestDestination(
          network::mojom::RequestDestination::kJson);
      break;
    case ModuleType::kJavaScript:
      resource_request.SetRequestContext(module_request.ContextType());
      resource_request.SetRequestDestination(module_request.Destination());
      break;
    case ModuleType::kInvalid:
      // ModuleTreeLinker checks that the module type is valid
      // before creating ModuleScriptFetchRequest objects.
      NOTREACHED();
  }
}

// <specdef href="https://html.spec.whatwg.org/C/#fetch-a-single-module-script">
void ModuleScriptLoader::FetchInternal(
    const ModuleScriptFetchRequest& module_request,
    ResourceFetcher* fetch_client_settings_object_fetcher,
    ModuleGraphLevel level,
    ModuleScriptCustomFetchType custom_fetch_type) {
  const FetchClientSettingsObject& fetch_client_settings_object =
      fetch_client_settings_object_fetcher->GetProperties()
          .GetFetchClientSettingsObject();

  // <spec step="7">Set moduleMap[(url, moduleType)] to "fetching".</spec>
  AdvanceState(State::kFetching);

  // <spec step="8">Let request be a new request whose url is url, ...</spec>
  ResourceRequest resource_request(module_request.Url());
#if DCHECK_IS_ON()
  url_ = module_request.Url();
#endif

  DOMWrapperWorld& request_world = modulator_->GetScriptState()->World();

  // Prevents web service workers from intercepting isolated world dynamic
  // script imports requests and responding with different contents.
  // TODO(crbug.com/1296102): Link to documentation that describes the criteria
  // where module imports are handled by service worker fetch handler.
  resource_request.SetSkipServiceWorker(request_world.IsIsolatedWorld());

  // <spec step="9">Set request 's destination to the result of running the
  // fetch destination from module type steps given destination and
  // moduleType.</spec>
  SetFetchDestinationFromModuleType(resource_request, module_request);

  ResourceLoaderOptions options(&request_world);

  // <spec step="11">Set request's initiator type to "script".</spec>
  options.initiator_info.name = fetch_initiator_type_names::kScript;

  // <spec step="12">Set up the module script request given request and
  // options.</spec>
  //
  // <specdef label="SMSR"
  // href="https://html.spec.whatwg.org/C/#set-up-the-module-script-request">

  // <spec label="SMSR">... its parser metadata to options's parser metadata,
  // ...</spec>
  options.parser_disposition = options_.ParserState();

  // TODO(crbug.com/1064920): Remove this once PlzDedicatedWorker ships.
  options.reject_coep_unsafe_none = options_.GetRejectCoepUnsafeNone();

  if (level == ModuleGraphLevel::kDependentModuleFetch) {
    options.initiator_info.is_imported_module = true;
    options.initiator_info.referrer = module_request.ReferrerString();
    options.initiator_info.position = module_request.GetReferrerPosition();
  }

  // Note: |options| should not be modified after here.
  FetchParameters fetch_params(std::move(resource_request), options);
  fetch_params.SetModuleScript();

  // <spec label="SMSR">... its integrity metadata to options's integrity
  // metadata, ...</spec>
  fetch_params.SetIntegrityMetadata(options_.GetIntegrityMetadata());
  fetch_params.MutableResourceRequest().SetFetchIntegrity(
      options_.GetIntegrityAttributeValue());

  // <spec label="SMSR">Set request's cryptographic nonce metadata to options's
  // cryptographic nonce, ...</spec>
  fetch_params.SetContentSecurityPolicyNonce(options_.Nonce());

  // <spec label="SMSR">... its referrer policy to options's referrer
  // policy.</spec>
  fetch_params.MutableResourceRequest().SetReferrerPolicy(
      module_request.Options().GetReferrerPolicy());

  // <spec step="5">... mode is "cors", ...</spec>
  //
  // <spec label="SMSR">... its credentials mode to options's credentials mode,
  // ...</spec>
  fetch_params.SetCrossOriginAccessControl(
      fetch_client_settings_object.GetSecurityOrigin(),
      options_.CredentialsMode());

  // <spec step="6">If destination is "worker" or "sharedworker" and the
  // top-level module fetch flag is set, then set request's mode to
  // "same-origin".</spec>
  //
  // `kServiceWorker` is included here for consistency, while it isn't mentioned
  // in the spec. This doesn't affect the behavior, because we already forbid
  // redirects and cross-origin response URLs in other places.
  if ((module_request.Destination() ==
           network::mojom::RequestDestination::kWorker ||
       module_request.Destination() ==
           network::mojom::RequestDestination::kSharedWorker ||
       module_request.Destination() ==
           network::mojom::RequestDestination::kServiceWorker) &&
      level == ModuleGraphLevel::kTopLevelModuleFetch) {
    // This should be done after SetCrossOriginAccessControl() that sets the
    // mode to kCors.
    fetch_params.MutableResourceRequest().SetMode(
        network::mojom::RequestMode::kSameOrigin);
  }

  // <spec step="5">... referrer is referrer, ...</spec>
  fetch_params.MutableResourceRequest().SetReferrerString(
      module_request.ReferrerString());

  // https://wicg.github.io/priority-hints/#script :
  // Step 10. "... request’s priority to option’s fetchpriority ..."
  fetch_params.MutableResourceRequest().SetFetchPriorityHint(
      options_.FetchPriorityHint());

  // <spec step="5">... and client is fetch client settings object.</spec>
  //
  // -> set by ResourceFetcher

  // Note: The fetch request's "origin" isn't specified in
  // https://html.spec.whatwg.org/C/#fetch-a-single-module-script
  // Thus, the "origin" is "client" per
  // https://fetch.spec.whatwg.org/#concept-request-origin

  // Module scripts are always defer.
  fetch_params.SetDefer(FetchParameters::kLazyLoad);
  fetch_params.SetRenderBlockingBehavior(
      module_request.Options().GetRenderBlockingBehavior());

  // <spec step="12.1">Let source text be the result of UTF-8 decoding
  // response's body.</spec>
  fetch_params.SetDecoderOptions(
      TextResourceDecoderOptions::CreateUTF8Decode());

  // <spec step="8">If the caller specified custom steps to perform the fetch,
  // perform them on request, setting the is top-level flag if the top-level
  // module fetch flag is set. Return from this algorithm, and when the custom
  // perform the fetch steps complete with response response, run the remaining
  // steps. Otherwise, fetch request. Return from this algorithm, and run the
  // remaining steps as part of the fetch's process response for the response
  // response.</spec>
  module_fetcher_ =
      modulator_->CreateModuleScriptFetcher(custom_fetch_type, PassKey());
  module_fetcher_->Fetch(fetch_params, module_request.GetExpectedModuleType(),
                         fetch_client_settings_object_fetcher, level, this);
}

// <specdef href="https://html.spec.whatwg.org/C/#fetch-a-single-module-script">
void ModuleScriptLoader::NotifyFetchFinishedError(
    const HeapVector<Member<ConsoleMessage>>& error_messages) {
  // [nospec] Abort the steps if the browsing context is discarded.
  if (!modulator_->HasValidContext()) {
    AdvanceState(State::kFinished);
    return;
  }

  // Note: "conditions" referred in Step 9 is implemented in
  // WasModuleLoadSuccessful() in module_script_fetcher.cc.
  // <spec step="9">If any of the following conditions are met, set
  // moduleMap[url] to null, asynchronously complete this algorithm with null,
  // and abort these steps: ...</spec>
  for (ConsoleMessage* error_message : error_messages) {
    ExecutionContext::From(modulator_->GetScriptState())
        ->AddConsoleMessage(error_message);
  }
  AdvanceState(State::kFinished);
}

// This implements the `processResponseConsumeBody` part of
// https://html.spec.whatwg.org/C#fetch-a-single-module-script
void ModuleScriptLoader::NotifyFetchFinishedSuccess(
    const ModuleScriptCreationParams& params) {
  // [nospec] Abort the steps if the browsing context is discarded.
  if (!modulator_->HasValidContext()) {
    AdvanceState(State::kFinished);
    return;
  }

  // <spec step="13.2">Let source text be the result of UTF-8 decoding
  // bodyBytes.</spec>
  //
  // <spec step="13.6">If referrerPolicy is not the empty string, set
  // options's referrer policy to referrerPolicy.</spec>
  //
  // Note that the "empty string" referrer policy corresponds to `kDefault`,
  // so we only use the response referrer policy if it is *not* `kDefault`.
  if (params.ResponseReferrerPolicy() !=
      network::mojom::ReferrerPolicy::kDefault) {
    options_.UpdateReferrerPolicyAfterResponseReceived(
        params.ResponseReferrerPolicy());
  }

  switch (params.GetModuleType()) {
    case ModuleType::kJSON:
      module_script_ = ValueWrapperSyntheticModuleScript::
          CreateJSONWrapperSyntheticModuleScript(params, modulator_);
      break;
    case ModuleType::kCSS:
      module_script_ = ValueWrapperSyntheticModuleScript::
          CreateCSSWrapperSyntheticModuleScript(params, modulator_);
      break;
    case ModuleType::kJavaScript:
      // <spec step="13.7">If mimeType is a JavaScript MIME type and
      // moduleType is "javascript", then set moduleScript to the result of
      // creating a JavaScript module script given sourceText, settingsObject,
      // response's URL, options, and importMap.</spec>
      //
      // The MIME type verification happens at
      // ModuleScriptFetcher::WasModuleLoadSuccessful.
      module_script_ = JSModuleScript::Create(params, modulator_, options_);
      break;
    case ModuleType::kInvalid:
      NOTREACHED_IN_MIGRATION();
  }

  AdvanceState(State::kFinished);
}

void ModuleScriptLoader::Trace(Visitor* visitor) const {
  visitor->Trace(modulator_);
  visitor->Trace(module_script_);
  visitor->Trace(registry_);
  visitor->Trace(client_);
  visitor->Trace(module_fetcher_);
}

}  // namespace blink
