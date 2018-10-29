// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/modulescript/module_script_loader.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_fetcher.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_loader_client.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_loader_registry.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/script/module_script.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loading_log.h"
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
  NOTREACHED();
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
      NOTREACHED();
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
    FetchClientSettingsObjectSnapshot* fetch_client_settings_object,
    ModuleGraphLevel level,
    Modulator* module_map_settings_object,
    ModuleScriptCustomFetchType custom_fetch_type,
    ModuleScriptLoaderRegistry* registry,
    ModuleScriptLoaderClient* client) {
  ModuleScriptLoader* loader = new ModuleScriptLoader(
      module_map_settings_object, module_request.Options(), registry, client);
  registry->AddLoader(loader);
  loader->FetchInternal(module_request, fetch_client_settings_object, level,
                        custom_fetch_type);
}

// https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-single-module-script
void ModuleScriptLoader::FetchInternal(
    const ModuleScriptFetchRequest& module_request,
    FetchClientSettingsObjectSnapshot* fetch_client_settings_object,
    ModuleGraphLevel level,
    ModuleScriptCustomFetchType custom_fetch_type) {
  // Step 4. "Set moduleMap[url] to "fetching"." [spec text]
  AdvanceState(State::kFetching);

  // Step 5. "Let request be a new request whose url is url, ..." [spec text]
  ResourceRequest resource_request(module_request.Url());
#if DCHECK_IS_ON()
  url_ = module_request.Url();
#endif

  // "destination is destination," [spec text]
  resource_request.SetRequestContext(module_request.Destination());

  ResourceLoaderOptions options;

  // TODO(domfarolino): Probably insert step 6 here, which sets the credentials
  // mode of "worker"- and "sharedworker"-destined requests to "same-origin",
  // ensuring cross-origin module workers result in a network error, once
  // https://github.com/whatwg/html/pull/3656 is merged. Cross-origin
  // workers are not supported anyways due to URL checks in
  // AbstractWorker::ResolveURL, but it might be good to try and follow the spec
  // here, and let this resolve in a network error as Fetch dictates?

  // Step 6. "Set up the module script request given request and options."
  // [spec text]
  // [SMSR]
  // https://html.spec.whatwg.org/multipage/webappapis.html#set-up-the-module-script-request

  // [SMSR] "... its parser metadata to options's parser metadata, ..."
  // [spec text]
  options.parser_disposition = options_.ParserState();

  // As initiator for module script fetch is not specified in HTML spec,
  // we specify "" as initiator per:
  // https://fetch.spec.whatwg.org/#concept-request-initiator
  options.initiator_info.name = g_empty_atom;

  if (level == ModuleGraphLevel::kDependentModuleFetch) {
    options.initiator_info.imported_module_referrer =
        module_request.ReferrerString();
    options.initiator_info.position = module_request.GetReferrerPosition();
  }

  // Note: |options| should not be modified after here.
  FetchParameters fetch_params(resource_request, options);

  // [SMSR] "... its integrity metadata to options's integrity metadata, ..."
  // [spec text]
  fetch_params.SetIntegrityMetadata(options_.GetIntegrityMetadata());
  fetch_params.MutableResourceRequest().SetFetchIntegrity(
      options_.GetIntegrityAttributeValue());

  // [SMSR] "Set request's cryptographic nonce metadata to options's
  // cryptographic nonce, ..." [spec text]
  fetch_params.SetContentSecurityPolicyNonce(options_.Nonce());

  // [SMSR] "... its referrer policy to options's referrer policy." [spec text]
  // Note: For now this is done below with SetHTTPReferrer()
  ReferrerPolicy referrer_policy = module_request.Options().GetReferrerPolicy();
  if (referrer_policy == kReferrerPolicyDefault)
    referrer_policy = fetch_client_settings_object->GetReferrerPolicy();

  // Step 5. "... mode is "cors", ..."
  // [SMSR] "... and its credentials mode to options's credentials mode."
  // [spec text]
  fetch_params.SetCrossOriginAccessControl(
      fetch_client_settings_object->GetSecurityOrigin(),
      options_.CredentialsMode());

  // Step 5. "... referrer is referrer, ..." [spec text]
  // Note: For now this is done below with SetHTTPReferrer()
  String referrer_string = module_request.ReferrerString();
  if (referrer_string == Referrer::ClientReferrerString())
    referrer_string = fetch_client_settings_object->GetOutgoingReferrer();

  // TODO(domfarolino): Stop storing ResourceRequest's referrer as a
  // blink::Referrer (https://crbug.com/850813).
  fetch_params.MutableResourceRequest().SetHTTPReferrer(
      SecurityPolicy::GenerateReferrer(referrer_policy,
                                       fetch_params.GetResourceRequest().Url(),
                                       referrer_string));

  // Step 5. "... and client is fetch client settings object." [spec text]
  // -> set by ResourceFetcher

  // Note: The fetch request's "origin" isn't specified in
  // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-single-module-script
  // Thus, the "origin" is "client" per
  // https://fetch.spec.whatwg.org/#concept-request-origin

  // Module scripts are always defer.
  fetch_params.SetDefer(FetchParameters::kLazyLoad);
  // [nospec] Unlike defer/async classic scripts, module scripts are fetched at
  // High priority.
  fetch_params.MutableResourceRequest().SetPriority(
      ResourceLoadPriority::kHigh);

  // Use UTF-8, according to Step 9:
  // "Let source text be the result of UTF-8 decoding response's body."
  // [spec text]
  fetch_params.SetDecoderOptions(
      TextResourceDecoderOptions::CreateAlwaysUseUTF8ForText());

  // Step 7. "If the caller specified custom steps to perform the fetch,
  // perform them on request, setting the is top-level flag if the top-level
  // module fetch flag is set. Return from this algorithm, and when the custom
  // perform the fetch steps complete with response response, run the remaining
  // steps.
  // Otherwise, fetch request. Return from this algorithm, and run the remaining
  // steps as part of the fetch's process response for the response response."
  // [spec text]
  module_fetcher_ = modulator_->CreateModuleScriptFetcher(custom_fetch_type);
  module_fetcher_->Fetch(fetch_params, level, this);
}

void ModuleScriptLoader::NotifyFetchFinished(
    const base::Optional<ModuleScriptCreationParams>& params,
    const HeapVector<Member<ConsoleMessage>>& error_messages) {
  // [nospec] Abort the steps if the browsing context is discarded.
  if (!modulator_->HasValidContext()) {
    AdvanceState(State::kFinished);
    return;
  }

  // Note: "conditions" referred in Step 8 is implemented in
  // WasModuleLoadSuccessful() in module_script_fetcher.cc.
  // Step 8. "If any of the following conditions are met, set moduleMap[url] to
  // null, asynchronously complete this algorithm with null, and abort these
  // steps." [spec text]
  if (!params.has_value()) {
    for (ConsoleMessage* error_message : error_messages) {
      ExecutionContext::From(modulator_->GetScriptState())
          ->AddConsoleMessage(error_message);
    }
    AdvanceState(State::kFinished);
    return;
  }

  // Step 9. "Let source text be the result of UTF-8 decoding response's body."
  // [spec text]
  // Step 10. "Let module script be the result of creating a module script given
  // source text, module map settings object, response's url, and options."
  // [spec text]
  module_script_ = ModuleScript::Create(
      params->GetSourceText(), modulator_, params->GetResponseUrl(),
      params->GetResponseUrl(), options_, params->GetAccessControlStatus());

  AdvanceState(State::kFinished);
}

void ModuleScriptLoader::Trace(blink::Visitor* visitor) {
  visitor->Trace(modulator_);
  visitor->Trace(module_script_);
  visitor->Trace(registry_);
  visitor->Trace(client_);
  visitor->Trace(module_fetcher_);
}

}  // namespace blink
