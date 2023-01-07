// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/modulescript/worklet_module_script_fetcher.h"

#include "third_party/blink/renderer/bindings/core/v8/script_source_location_type.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"

namespace blink {

WorkletModuleScriptFetcher::WorkletModuleScriptFetcher(
    WorkletModuleResponsesMap* module_responses_map,
    base::PassKey<ModuleScriptLoader> pass_key)
    : ModuleScriptFetcher(pass_key),
      module_responses_map_(module_responses_map) {}

void WorkletModuleScriptFetcher::Fetch(
    FetchParameters& fetch_params,
    ModuleType expected_module_type,
    ResourceFetcher* fetch_client_settings_object_fetcher,
    ModuleGraphLevel level,
    ModuleScriptFetcher::Client* client) {
  DCHECK_EQ(fetch_params.GetScriptType(), mojom::blink::ScriptType::kModule);
  if (module_responses_map_->GetEntry(
          fetch_params.Url(), expected_module_type, client,
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
  expected_module_type_ = expected_module_type;
  ScriptResource::Fetch(fetch_params, fetch_client_settings_object_fetcher,
                        this, ScriptResource::kNoStreaming);
}

void WorkletModuleScriptFetcher::NotifyFinished(Resource* resource) {
  ClearResource();

  absl::optional<ModuleScriptCreationParams> params;
  auto* script_resource = To<ScriptResource>(resource);
  HeapVector<Member<ConsoleMessage>> error_messages;
  if (WasModuleLoadSuccessful(script_resource, expected_module_type_,
                              &error_messages)) {
    const KURL& url = script_resource->GetResponse().ResponseUrl();
    // Create an external module script where base_url == source_url.
    // https://html.spec.whatwg.org/multipage/webappapis.html#concept-script-base-url
    params.emplace(/*source_url=*/url, /*base_url=*/url,
                   ScriptSourceLocationType::kExternalFile,
                   expected_module_type_, script_resource->SourceText(),
                   script_resource->CacheHandler());
  }

  // This will eventually notify |client| passed to
  // WorkletModuleScriptFetcher::Fetch().
  module_responses_map_->SetEntryParams(url_, expected_module_type_, params);
}

}  // namespace blink
