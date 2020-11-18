// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/modulescript/installed_service_worker_module_script_fetcher.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/dom_implementation.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/workers/installed_scripts_manager.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"

namespace blink {

InstalledServiceWorkerModuleScriptFetcher::
    InstalledServiceWorkerModuleScriptFetcher(
        WorkerGlobalScope* global_scope,
        base::PassKey<ModuleScriptLoader> pass_key)
    : ModuleScriptFetcher(pass_key), global_scope_(global_scope) {
  DCHECK(global_scope_->IsServiceWorkerGlobalScope());
}

void InstalledServiceWorkerModuleScriptFetcher::Fetch(
    FetchParameters& fetch_params,
    ResourceFetcher*,
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
    error_messages.push_back(MakeGarbageCollected<ConsoleMessage>(
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

  ModuleScriptCreationParams::ModuleType module_type;

  // TODO(sasebree) De-duplicate similar logic that lives in
  // ModuleScriptFetcher::WasModuleLoadSuccessful
  if (MIMETypeRegistry::IsSupportedJavaScriptMIMEType(
          script_data->GetHttpContentType())) {
    module_type = ModuleScriptCreationParams::ModuleType::kJavaScriptModule;
  } else if (base::FeatureList::IsEnabled(blink::features::kJSONModules) &&
             MIMETypeRegistry::IsJSONMimeType(
                 script_data->GetHttpContentType())) {
    module_type = ModuleScriptCreationParams::ModuleType::kJSONModule;
  } else {
    // This should never happen.
    // If we reach here, we know we received an incompatible mime type from the
    // network
    HeapVector<Member<ConsoleMessage>> error_messages;
    error_messages.push_back(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kJavaScript,
        mojom::ConsoleMessageLevel::kError,
        "Failed to load the script unexpectedly",
        fetch_params.Url().GetString(), nullptr, 0));
    client->NotifyFetchFinished(base::nullopt, error_messages);
    return;
  }

  ModuleScriptCreationParams params(
      fetch_params.Url(), module_type,
      ParkableString(script_data->TakeSourceText().Impl()),
      nullptr /* cache_handler */,
      fetch_params.GetResourceRequest().GetCredentialsMode());
  client->NotifyFetchFinished(params, HeapVector<Member<ConsoleMessage>>());
}

void InstalledServiceWorkerModuleScriptFetcher::Trace(Visitor* visitor) const {
  ModuleScriptFetcher::Trace(visitor);
  visitor->Trace(global_scope_);
}

}  // namespace blink
