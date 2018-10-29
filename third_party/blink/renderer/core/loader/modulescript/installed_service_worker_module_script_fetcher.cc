// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/modulescript/installed_service_worker_module_script_fetcher.h"

#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/workers/installed_scripts_manager.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"

namespace blink {

InstalledServiceWorkerModuleScriptFetcher::
    InstalledServiceWorkerModuleScriptFetcher(WorkerGlobalScope* global_scope)
    : global_scope_(global_scope) {}

void InstalledServiceWorkerModuleScriptFetcher::Fetch(
    FetchParameters& fetch_params,
    ModuleGraphLevel level,
    ModuleScriptFetcher::Client* client) {
  DCHECK(global_scope_->IsContextThread());
  InstalledScriptsManager* installed_scripts_manager =
      global_scope_->GetThread()->GetInstalledScriptsManager();
  DCHECK(installed_scripts_manager);
  DCHECK(installed_scripts_manager->IsScriptInstalled(fetch_params.Url()));

  std::unique_ptr<InstalledScriptsManager::ScriptData> script =
      installed_scripts_manager->GetScriptData(fetch_params.Url());

  if (!script) {
    HeapVector<Member<ConsoleMessage>> error_messages;
    error_messages.push_back(ConsoleMessage::CreateForRequest(
        kJSMessageSource, kErrorMessageLevel,
        "Failed to load the script unexpectedly",
        fetch_params.Url().GetString(), nullptr, 0));
    client->NotifyFetchFinished(base::nullopt, error_messages);
    return;
  }

  const AccessControlStatus access_control_status =
      global_scope_->GetSecurityOrigin()->CanReadContent(fetch_params.Url())
          ? kSharableCrossOrigin
          : kOpaqueResource;
  ModuleScriptCreationParams params(
      fetch_params.Url(), ParkableString(script->TakeSourceText().Impl()),
      fetch_params.GetResourceRequest().GetFetchCredentialsMode(),
      access_control_status);
  client->NotifyFetchFinished(params, HeapVector<Member<ConsoleMessage>>());
}

void InstalledServiceWorkerModuleScriptFetcher::Trace(blink::Visitor* visitor) {
  ModuleScriptFetcher::Trace(visitor);
  visitor->Trace(global_scope_);
}

}  // namespace blink
