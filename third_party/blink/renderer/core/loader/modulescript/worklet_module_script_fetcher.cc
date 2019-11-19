// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/modulescript/worklet_module_script_fetcher.h"

#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"

namespace blink {

WorkletModuleScriptFetcher::WorkletModuleScriptFetcher(
    WorkletModuleResponsesMap* module_responses_map)
    : module_responses_map_(module_responses_map) {}

void WorkletModuleScriptFetcher::Fetch(
    FetchParameters& fetch_params,
    ResourceFetcher* fetch_client_settings_object_fetcher,
    const Modulator* modulator_for_built_in_modules,
    ModuleGraphLevel level,
    ModuleScriptFetcher::Client* client) {
  if (module_responses_map_->GetEntry(
          fetch_params.Url(), client,
          fetch_client_settings_object_fetcher->GetTaskRunner())) {
    return;
  }

  // TODO(japhet): This worklet global scope will drive the fetch of this
  // module. If another global scope requests the same module,
  // module_responses_map_ will ensure that it is notified when this fetch
  // completes. Currently, all worklet global scopes are destroyed when the
  // Document is destroyed, so we won't end up in a situation where this global
  // scope is being destroyed and needs to cancel the fetch, but some other
  // global scope is still alive and still wants to complete the fetch. When we
  // support worklet global scopes being created and destroyed flexibly, we'll
  // need to handle that case, maybe by having a way to restart fetches in a
  // different global scope?
  url_ = fetch_params.Url();
  ScriptResource::Fetch(fetch_params, fetch_client_settings_object_fetcher,
                        this, ScriptResource::kNoStreaming);
}

void WorkletModuleScriptFetcher::NotifyFinished(Resource* resource) {
  ClearResource();

  base::Optional<ModuleScriptCreationParams> params;
  ScriptResource* script_resource = ToScriptResource(resource);
  HeapVector<Member<ConsoleMessage>> error_messages;
  ModuleScriptCreationParams::ModuleType module_type;
  if (WasModuleLoadSuccessful(script_resource, &error_messages, &module_type)) {
    params.emplace(script_resource->GetResponse().CurrentRequestUrl(),
                   module_type, script_resource->SourceText(),
                   script_resource->CacheHandler(),
                   script_resource->GetResourceRequest().GetCredentialsMode());
  }

  // This will eventually notify |client| passed to
  // WorkletModuleScriptFetcher::Fetch().
  module_responses_map_->SetEntryParams(url_, params);
}

}  // namespace blink
