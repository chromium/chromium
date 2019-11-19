// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/modulescript/document_module_script_fetcher.h"

#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/script/layered_api.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

void DocumentModuleScriptFetcher::Fetch(
    FetchParameters& fetch_params,
    ResourceFetcher* fetch_client_settings_object_fetcher,
    const Modulator* modulator_for_built_in_modules,
    ModuleGraphLevel level,
    ModuleScriptFetcher::Client* client) {
  DCHECK(fetch_client_settings_object_fetcher);
  DCHECK(!client_);
  client_ = client;

  if (modulator_for_built_in_modules &&
      FetchIfLayeredAPI(*modulator_for_built_in_modules, fetch_params))
    return;

  ScriptResource::Fetch(fetch_params, fetch_client_settings_object_fetcher,
                        this, ScriptResource::kNoStreaming);
}

void DocumentModuleScriptFetcher::NotifyFinished(Resource* resource) {
  ClearResource();

  ScriptResource* script_resource = ToScriptResource(resource);

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

void DocumentModuleScriptFetcher::Trace(blink::Visitor* visitor) {
  visitor->Trace(client_);
  ResourceClient::Trace(visitor);
}

bool DocumentModuleScriptFetcher::FetchIfLayeredAPI(
    const Modulator& modulator_for_built_in_modules,
    FetchParameters& fetch_params) {
  if (!modulator_for_built_in_modules.BuiltInModuleInfraEnabled())
    return false;

  KURL layered_api_url = blink::layered_api::GetInternalURL(fetch_params.Url());

  if (layered_api_url.IsNull())
    return false;

  String source_text = blink::layered_api::GetSourceText(
      modulator_for_built_in_modules, layered_api_url);

  if (source_text.IsNull()) {
    HeapVector<Member<ConsoleMessage>> error_messages;
    error_messages.push_back(ConsoleMessage::CreateForRequest(
        mojom::ConsoleMessageSource::kJavaScript,
        mojom::ConsoleMessageLevel::kError, "Unexpected data error",
        fetch_params.Url().GetString(), nullptr, 0));
    client_->NotifyFetchFinished(base::nullopt, error_messages);
    return true;
  }

  // TODO(hiroshige): Support V8 Code Cache for Layered API.
  // TODO(sasebree). Support Non-JS Modules for Layered API.
  ModuleScriptCreationParams params(
      layered_api_url,
      ModuleScriptCreationParams::ModuleType::kJavaScriptModule,
      ParkableString(source_text.ReleaseImpl()), nullptr /* cache_handler */,
      fetch_params.GetResourceRequest().GetCredentialsMode());
  client_->NotifyFetchFinished(params, HeapVector<Member<ConsoleMessage>>());
  return true;
}

}  // namespace blink
