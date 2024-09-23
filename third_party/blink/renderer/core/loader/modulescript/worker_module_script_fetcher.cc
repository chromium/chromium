// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/modulescript/worker_module_script_fetcher.h"

#include <memory>

#include "services/network/public/mojom/referrer_policy.mojom-blink.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/network_utils.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_location_type.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/loader/fetch/unique_identifier.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_response_headers.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"

namespace blink {

WorkerModuleScriptFetcher::WorkerModuleScriptFetcher(
    WorkerGlobalScope* global_scope,
    base::PassKey<ModuleScriptLoader> pass_key)
    : ModuleScriptFetcher(pass_key), global_scope_(global_scope) {}

// <specdef href="https://html.spec.whatwg.org/C/#run-a-worker">
void WorkerModuleScriptFetcher::Fetch(
    FetchParameters& fetch_params,
    ModuleType expected_module_type,
    ResourceFetcher* fetch_client_settings_object_fetcher,
    ModuleGraphLevel level,
    ModuleScriptFetcher::Client* client) {
  DCHECK_EQ(fetch_params.GetScriptType(), mojom::blink::ScriptType::kModule);
  DCHECK(global_scope_->IsContextThread());
  DCHECK(!fetch_client_settings_object_fetcher_);
  fetch_client_settings_object_fetcher_ = fetch_client_settings_object_fetcher;
  client_ = client;
  level_ = level;
  expected_module_type_ = expected_module_type;

  // Use WorkerMainScriptLoader to load the main script when
  // dedicated workers (PlzDedicatedWorker) and shared workers.
  std::unique_ptr<WorkerMainScriptLoadParameters>
      worker_main_script_load_params =
          global_scope_->TakeWorkerMainScriptLoadingParametersForModules();
  if (worker_main_script_load_params) {
    DCHECK_EQ(level_, ModuleGraphLevel::kTopLevelModuleFetch);

    auto identifier = CreateUniqueIdentifier();
    if (global_scope_->IsServiceWorkerGlobalScope()) {
      global_scope_->SetMainResoureIdentifier(identifier);
    }

    fetch_params.MutableResourceRequest().SetInspectorId(identifier);
    worker_main_script_loader_ = MakeGarbageCollected<WorkerMainScriptLoader>();
    worker_main_script_loader_->Start(
        fetch_params, std::move(worker_main_script_load_params),
        &fetch_client_settings_object_fetcher->Context(),
        fetch_client_settings_object_fetcher->GetResourceLoadObserver(), this);
    return;
  }

  // <spec step="12">In both cases, to perform the fetch given request, perform
  // the following steps if the is top-level flag is set:</spec>
  //
  // <spec step="12.1">Set request's reserved client to inside settings.</spec>
  //
  // This is implemented in the browser process.

  // <spec step="12.2">Fetch request, and asynchronously wait to run the
  // remaining steps as part of fetch's process response for the response
  // response.</spec>

  // If streaming is not allowed, no compile hints are needed either.
  constexpr v8_compile_hints::V8CrowdsourcedCompileHintsProducer*
      kNoCompileHintsProducer = nullptr;
  constexpr v8_compile_hints::V8CrowdsourcedCompileHintsConsumer*
      kNoCompileHintsConsumer = nullptr;
  constexpr bool kNoV8CompileHintsMagicCommentRuntimeEnabledFeature = false;
  ScriptResource::Fetch(fetch_params, fetch_client_settings_object_fetcher,
                        this, global_scope_->GetIsolate(),
                        ScriptResource::kNoStreaming, kNoCompileHintsProducer,
                        kNoCompileHintsConsumer,
                        kNoV8CompileHintsMagicCommentRuntimeEnabledFeature);
}

void WorkerModuleScriptFetcher::Trace(Visitor* visitor) const {
  ModuleScriptFetcher::Trace(visitor);
  visitor->Trace(client_);
  visitor->Trace(global_scope_);
  visitor->Trace(fetch_client_settings_object_fetcher_);
  visitor->Trace(worker_main_script_loader_);
}

// https://html.spec.whatwg.org/C/#worker-processing-model
void WorkerModuleScriptFetcher::NotifyFinished(Resource* resource) {
  DCHECK(global_scope_->IsContextThread());
  ClearResource();

  auto* script_resource = To<ScriptResource>(resource);
  {
    HeapVector<Member<ConsoleMessage>> error_messages;
    if (!WasModuleLoadSuccessful(script_resource, expected_module_type_,
                                 &error_messages)) {
      client_->NotifyFetchFinishedError(error_messages);
      return;
    }
  }

  NotifyClient(resource->Url(), expected_module_type_,
               script_resource->SourceText(), resource->GetResponse(),
               script_resource->CacheHandler());
}

void WorkerModuleScriptFetcher::NotifyClient(
    const KURL& request_url,
    ModuleType module_type,
    const ParkableString& source_text,
    const ResourceResponse& response,
    CachedMetadataHandler* cache_handler) {
  HeapVector<Member<ConsoleMessage>> error_messages;

  const KURL response_url = response.ResponseUrl();

  network::mojom::ReferrerPolicy response_referrer_policy =
      network::mojom::ReferrerPolicy::kDefault;
  const String response_referrer_policy_header =
      response.HttpHeaderField(http_names::kReferrerPolicy);
  if (!response_referrer_policy_header.IsNull()) {
    SecurityPolicy::ReferrerPolicyFromHeaderValue(
        response_referrer_policy_header,
        kDoNotSupportReferrerPolicyLegacyKeywords, &response_referrer_policy);
  }

  if (level_ == ModuleGraphLevel::kTopLevelModuleFetch) {
    // TODO(nhiroki, hiroshige): Access to WorkerGlobalScope in module loaders
    // is a layering violation. Also, updating WorkerGlobalScope ('module map
    // settigns object') in flight can be dangerous because module loaders may
    // refer to it. We should move these steps out of core/loader/modulescript/
    // and run them after module loading. This may require the spec change.
    // (https://crbug.com/845285)

    // Ensure redirects don't affect SecurityOrigin.
    DCHECK(fetch_client_settings_object_fetcher_->GetProperties()
               .GetFetchClientSettingsObject()
               .GetSecurityOrigin()
               ->CanReadContent(request_url))
        << "Top-level worker script request url must be same-origin with "
           "outside settings constructor origin or permitted by the parent "
           "chrome-extension.";

    // |response_url| must be same-origin with request origin or its url's
    // scheme must be "data".
    //
    // https://fetch.spec.whatwg.org/#concept-main-fetch
    // Step 5:
    // - request’s current URL’s origin is same origin with request’s
    // origin (request's current URL indicates |response_url|)
    // - request’s current URL’s scheme is "data"
    // ---> Return the result of performing a scheme fetch using request.
    // - request’s mode is "same-origin"
    // ---> Return a network error. [spec text]
    if (!SecurityOrigin::AreSameOrigin(request_url, response_url) &&
        !response_url.ProtocolIsData()) {
      error_messages.push_back(MakeGarbageCollected<ConsoleMessage>(
          mojom::ConsoleMessageSource::kSecurity,
          mojom::ConsoleMessageLevel::kError,
          "Refused to cross-origin redirects of the top-level worker script."));
      client_->NotifyFetchFinishedError(error_messages);
      return;
    }

    std::unique_ptr<Vector<String>> response_origin_trial_tokens =
        OriginTrialContext::ParseHeaderValue(
            response.HttpHeaderField(http_names::kOriginTrial));

    // Step 12.3-12.6 are implemented in Initialize().
    global_scope_->Initialize(
        response_url, response_referrer_policy,
        ParseContentSecurityPolicyHeaders(
            ContentSecurityPolicyResponseHeaders(response)),
        response_origin_trial_tokens.get());
  }

  // <spec step="12.7">Asynchronously complete the perform the fetch steps with
  // response.</spec>
  // Create an external module script where base_url == source_url.
  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-script-base-url
  client_->NotifyFetchFinishedSuccess(ModuleScriptCreationParams(
      /*source_url=*/response_url, /*base_url=*/response_url,
      ScriptSourceLocationType::kExternalFile, module_type, source_text,
      cache_handler, response_referrer_policy));
}

void WorkerModuleScriptFetcher::DidReceiveDataWorkerMainScript(
    base::span<const char> span) {
  if (!decoder_) {
    decoder_ = std::make_unique<TextResourceDecoder>(TextResourceDecoderOptions(
        TextResourceDecoderOptions::kPlainTextContent,
        worker_main_script_loader_->GetScriptEncoding()));
  }
  if (!span.size()) {
    return;
  }
  source_text_.Append(decoder_->Decode(span));
}

void WorkerModuleScriptFetcher::OnStartLoadingBodyWorkerMainScript(
    const ResourceResponse& resource_response) {
  if (!MIMETypeRegistry::IsSupportedJavaScriptMIMEType(
          resource_response.HttpContentType())) {
    HeapVector<Member<ConsoleMessage>> error_messages;
    String message =
        "Failed to load module script: The server responded with a "
        "non-JavaScript MIME type of \"" +
        resource_response.HttpContentType() +
        "\". Strict MIME type checking is enforced for module scripts per HTML "
        "spec.";
    error_messages.push_back(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kJavaScript,
        mojom::ConsoleMessageLevel::kError, message,
        resource_response.ResponseUrl().GetString(), /*loader=*/nullptr, -1));
    worker_main_script_loader_->Cancel();
    client_->NotifyFetchFinishedError(error_messages);
    return;
  }
}

void WorkerModuleScriptFetcher::OnFinishedLoadingWorkerMainScript() {
  const ResourceResponse& response = worker_main_script_loader_->GetResponse();
  if (decoder_)
    source_text_.Append(decoder_->Flush());
  NotifyClient(worker_main_script_loader_->GetRequestURL(),
               ModuleType::kJavaScript,
               ParkableString(source_text_.ToString().ReleaseImpl()), response,
               worker_main_script_loader_->CreateCachedMetadataHandler());
}

void WorkerModuleScriptFetcher::OnFailedLoadingWorkerMainScript() {
  client_->NotifyFetchFinishedError(HeapVector<Member<ConsoleMessage>>());
}

}  // namespace blink
