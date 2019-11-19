// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/modulescript/installed_service_worker_module_script_fetcher.h"

#include "third_party/blink/public/mojom/appcache/appcache.mojom-blink.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/workers/installed_scripts_manager.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"

namespace blink {

InstalledServiceWorkerModuleScriptFetcher::
    InstalledServiceWorkerModuleScriptFetcher(WorkerGlobalScope* global_scope)
    : global_scope_(global_scope) {
  DCHECK(global_scope_->IsServiceWorkerGlobalScope());
}

void InstalledServiceWorkerModuleScriptFetcher::Fetch(
    FetchParameters& fetch_params,
    ResourceFetcher*,
    const Modulator* modulator_for_built_in_modules,
    ModuleGraphLevel level,
    ModuleScriptFetcher::Client* client) {
  DCHECK(global_scope_->IsContextThread());
  auto* installed_scripts_manager = global_scope_->GetInstalledScriptsManager();
  DCHECK(installed_scripts_manager);
  DCHECK(installed_scripts_manager->IsScriptInstalled(fetch_params.Url()));

  std::unique_ptr<InstalledScriptsManager::ScriptData> script_data =
      installed_scripts_manager->GetScriptData(fetch_params.Url());

  if (!script_data) {
    HeapVector<Member<ConsoleMessage>> error_messages;
    error_messages.push_back(ConsoleMessage::CreateForRequest(
        mojom::ConsoleMessageSource::kJavaScript,
        mojom::ConsoleMessageLevel::kError,
        "Failed to load the script unexpectedly",
        fetch_params.Url().GetString(), nullptr, 0));
    client->NotifyFetchFinished(base::nullopt, error_messages);
    return;
  }

  if (level == ModuleGraphLevel::kTopLevelModuleFetch) {
    // |fetch_params.Url()| is always equal to the response URL because service
    // worker script fetch disallows redirect.
    // https://w3c.github.io/ServiceWorker/#ref-for-concept-request-redirect-mode
    KURL response_url = fetch_params.Url();

    auto response_referrer_policy = network::mojom::ReferrerPolicy::kDefault;
    if (!script_data->GetReferrerPolicy().IsNull()) {
      SecurityPolicy::ReferrerPolicyFromHeaderValue(
          script_data->GetReferrerPolicy(),
          kDoNotSupportReferrerPolicyLegacyKeywords, &response_referrer_policy);
    }

    // Construct a ContentSecurityPolicy object to convert
    // ContentSecurityPolicyResponseHeaders to CSPHeaderAndType.
    // TODO(nhiroki): Find an efficient way to do this.
    auto* response_content_security_policy =
        MakeGarbageCollected<ContentSecurityPolicy>();
    response_content_security_policy->DidReceiveHeaders(
        script_data->GetContentSecurityPolicyResponseHeaders());

    global_scope_->Initialize(response_url, response_referrer_policy,
                              script_data->GetResponseAddressSpace(),
                              response_content_security_policy->Headers(),
                              script_data->CreateOriginTrialTokens().get(),
                              mojom::blink::kAppCacheNoCacheId);
  }

  // TODO(sasebree). Figure out how to get the correct mime type for the
  // ModuleScriptCreationParams here. Hard-coding application/javascript will
  // cause JSON modules to break for service workers. We need to store the mime
  // type of the service worker scripts in ServiceWorkerStorage, and pass it up
  // to here via InstalledScriptManager.
  ModuleScriptCreationParams params(
      fetch_params.Url(),
      ModuleScriptCreationParams::ModuleType::kJavaScriptModule,
      ParkableString(script_data->TakeSourceText().Impl()),
      nullptr /* cache_handler */,
      fetch_params.GetResourceRequest().GetCredentialsMode());
  client->NotifyFetchFinished(params, HeapVector<Member<ConsoleMessage>>());
}

void InstalledServiceWorkerModuleScriptFetcher::Trace(blink::Visitor* visitor) {
  ModuleScriptFetcher::Trace(visitor);
  visitor->Trace(global_scope_);
}

}  // namespace blink
