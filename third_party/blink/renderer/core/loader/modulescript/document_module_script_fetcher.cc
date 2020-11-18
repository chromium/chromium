// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/modulescript/document_module_script_fetcher.h"

#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

DocumentModuleScriptFetcher::DocumentModuleScriptFetcher(
    util::PassKey<ModuleScriptLoader> pass_key)
    : ModuleScriptFetcher(pass_key) {}

void DocumentModuleScriptFetcher::Fetch(
    FetchParameters& fetch_params,
    ResourceFetcher* fetch_client_settings_object_fetcher,
    ModuleGraphLevel level,
    ModuleScriptFetcher::Client* client) {
  DCHECK(fetch_client_settings_object_fetcher);
  DCHECK(!client_);
  client_ = client;

  ScriptResource::Fetch(fetch_params, fetch_client_settings_object_fetcher,
                        this, ScriptResource::kNoStreaming);
}

void DocumentModuleScriptFetcher::NotifyFinished(Resource* resource) {
  ClearResource();

  auto* script_resource = To<ScriptResource>(resource);

  HeapVector<Member<ConsoleMessage>> error_messages;
  ModuleScriptCreationParams::ModuleType module_type;
  if (!WasModuleLoadSuccessful(script_resource, &error_messages,
                               &module_type)) {
    client_->NotifyFetchFinished(base::nullopt, error_messages);
    return;
  }

  ModuleScriptCreationParams params(
      script_resource->GetResponse().CurrentRequestUrl(), module_type,
      script_resource->SourceText(), script_resource->CacheHandler(),
      script_resource->GetResourceRequest().GetCredentialsMode());
  client_->NotifyFetchFinished(params, error_messages);
}

void DocumentModuleScriptFetcher::Trace(Visitor* visitor) const {
  ModuleScriptFetcher::Trace(visitor);
  visitor->Trace(client_);
  ResourceClient::Trace(visitor);
}

}  // namespace blink
