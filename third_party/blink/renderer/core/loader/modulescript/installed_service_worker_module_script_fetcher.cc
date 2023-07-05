// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/modulescript/installed_service_worker_module_script_fetcher.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_location_type.h"
#include "third_party/blink/renderer/core/dom/dom_implementation.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/workers/installed_scripts_manager.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
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
    ModuleType expected_module_type,
    ResourceFetcher*,
    ModuleGraphLevel level,
    ModuleScriptFetcher::Client* client) {
  DCHECK_EQ(fetch_params.GetScriptType(), mojom::blink::ScriptType::kModule);
  DCHECK(global_scope_->IsContextThread());
  auto* installed_scripts_manager = global_scope_->GetInstalledScriptsManager();
  DCHECK(installed_scripts_manager);
  DCHECK(installed_scripts_manager->IsScriptInstalled(fetch_params.Url()));
  expected_module_type_ = expected_module_type;

  std::unique_ptr<InstalledScriptsManager::ScriptData> script_data =
      installed_scripts_manager->GetScriptData(fetch_params.Url());

  if (!script_data) {
    HeapVector<Member<ConsoleMessage>> error_messages;
    error_messages.push_back(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kJavaScript,
        mojom::ConsoleMessageLevel::kError,
        "Failed to load the script unexpectedly",
        fetch_params.Url().GetString(), nullptr, 0));
    client->NotifyFetchFinishedError(error_messages);
    return;
  }

  network::mojom::ReferrerPolicy response_referrer_policy =
      network::mojom::ReferrerPolicy::kDefault;

  if (level == ModuleGraphLevel::kTopLevelModuleFetch) {
    // |fetch_params.Url()| is always equal to the response URL because service
    // worker script fetch disallows redirect.
    // https://w3c.github.io/ServiceWorker/#ref-for-concept-request-redirect-mode
    KURL response_url = fetch_params.Url();

    if (!script_data->GetReferrerPolicy().IsNull()) {
      SecurityPolicy::ReferrerPolicyFromHeaderValue(
          script_data->GetReferrerPolicy(),
          kDoNotSupportReferrerPolicyLegacyKeywords, &response_referrer_policy);
    }

    global_scope_->Initialize(
        response_url, response_referrer_policy,
        ParseContentSecurityPolicyHeaders(
            script_data->GetContentSecurityPolicyResponseHeaders()),
        script_data->CreateOriginTrialTokens().get());
  }

  // TODO(sasebree) De-duplicate similar logic that lives in
  // ModuleScriptFetcher::WasModuleLoadSuccessful
  if (expected_module_type_ != ModuleType::kJavaScript ||
      !MIMETypeRegistry::IsSupportedJavaScriptMIMEType(
          script_data->GetHttpContentType())) {
    // This should never happen.
    // If we reach here, we know we received an incompatible mime type from the
    // network
    HeapVector<Member<ConsoleMessage>> error_messages;
    error_messages.push_back(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kJavaScript,
        mojom::ConsoleMessageLevel::kError,
        "Failed to load the script unexpectedly",
        fetch_params.Url().GetString(), nullptr, 0));
    client->NotifyFetchFinishedError(error_messages);
    return;
  }

  // Create an external module script where base_url == source_url.
  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-script-base-url
  client->NotifyFetchFinishedSuccess(ModuleScriptCreationParams(
      /*source_url=*/fetch_params.Url(), /*base_url=*/fetch_params.Url(),
      ScriptSourceLocationType::kExternalFile, expected_module_type_,
      ParkableString(script_data->TakeSourceText().Impl()),
      /*cache_handler=*/nullptr, response_referrer_policy));
}

void InstalledServiceWorkerModuleScriptFetcher::Trace(Visitor* visitor) const {
  ModuleScriptFetcher::Trace(visitor);
  visitor->Trace(global_scope_);
}

}  // namespace blink
