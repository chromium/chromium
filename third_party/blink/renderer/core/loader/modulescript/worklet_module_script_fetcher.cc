// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/modulescript/worklet_module_script_fetcher.h"

#include "services/network/public/cpp/header_util.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_location_type.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/workers/worklet_global_scope.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"

namespace blink {

WorkletModuleScriptFetcher::WorkletModuleScriptFetcher(
    WorkletGlobalScope* global_scope,
    base::PassKey<ModuleScriptLoader> pass_key)
    : ModuleScriptFetcher(pass_key), global_scope_(global_scope) {}

void WorkletModuleScriptFetcher::Fetch(
    FetchParameters& fetch_params,
    ModuleType expected_module_type,
    ResourceFetcher* fetch_client_settings_object_fetcher,
    ModuleGraphLevel level,
    ModuleScriptFetcher::Client* client,
    ModuleImportPhase import_phase) {
  DCHECK_EQ(fetch_params.GetScriptType(), mojom::blink::ScriptType::kModule);
  if (global_scope_->GetModuleResponsesMap()->GetEntry(
          fetch_params.Url(), expected_module_type, client,
          fetch_client_settings_object_fetcher->GetTaskRunner())) {
    return;
  }

  // TODO(japhet): This worklet global scope will drive the fetch of this
  // module. If another global scope requests the same module,
  // global_scope_->GetModuleResponsesMap() will ensure that it is notified when
  // this fetch completes. Currently, all worklet global scopes are destroyed
  // when the Document is destroyed, so we won't end up in a situation where
  // this global scope is being destroyed and needs to cancel the fetch, but
  // some other global scope is still alive and still wants to complete the
  // fetch. When we support worklet global scopes being created and destroyed
  // flexibly, we'll need to handle that case, maybe by having a way to restart
  // fetches in a different global scope?
  url_ = fetch_params.Url();
  expected_module_type_ = expected_module_type;

  // If streaming is not allowed, no compile hints are needed either.
  constexpr v8_compile_hints::V8CrowdsourcedCompileHintsProducer*
      kNoCompileHintsProducer = nullptr;
  constexpr v8_compile_hints::V8CrowdsourcedCompileHintsConsumer*
      kNoCompileHintsConsumer = nullptr;
  ScriptResource::Fetch(fetch_params, fetch_client_settings_object_fetcher,
                        this, global_scope_->GetIsolate(),
                        ScriptResource::kNoStreaming, kNoCompileHintsProducer,
                        kNoCompileHintsConsumer,
                        v8_compile_hints::MagicCommentMode::kNone);
}

void WorkletModuleScriptFetcher::NotifyFinished(Resource* resource) {
  ClearResource();

  auto* script_resource = To<ScriptResource>(resource);
  HeapVector<Member<ConsoleMessage>> error_messages;
  std::optional<ResolvedModuleType> resolved_module_type =
      WasModuleLoadSuccessful(script_resource, expected_module_type_,
                              &error_messages);
  if (resolved_module_type) {
    const KURL& url = script_resource->GetResponse().ResponseUrl();

    network::mojom::ReferrerPolicy response_referrer_policy =
        network::mojom::ReferrerPolicy::kDefault;

    const String& response_referrer_policy_header =
        script_resource->GetResponse().HttpHeaderField(
            http_names::kReferrerPolicy);
    if (!response_referrer_policy_header.IsNull()) {
      SecurityPolicy::ReferrerPolicyFromHeaderValue(
          response_referrer_policy_header,
          kDoNotSupportReferrerPolicyLegacyKeywords, &response_referrer_policy);
    }

    // Create an external module script where base_url == source_url.
    // https://html.spec.whatwg.org/multipage/webappapis.html#concept-script-base-url
    ModuleScriptCreationParams params(
        /*source_url=*/url, /*base_url=*/url,
        ScriptSourceLocationType::kExternalFile, resolved_module_type.value(),
        script_resource->GetSourceTextOrWasmSource(
            resolved_module_type.value()),
        script_resource->CacheHandler(), response_referrer_policy,
        script_resource->GetResponse().HttpHeaderField(http_names::kSourceMap));

    global_scope_->GetModuleResponsesMap()->SetEntryParams(
        url_, expected_module_type_, std::move(params));
    return;
  }

  const bool is_cors_or_access_check =
      script_resource->ErrorOccurred() &&
      (script_resource->GetResourceError().IsAccessCheck() ||
       script_resource->GetResourceError().CorsErrorStatus().has_value());

  const bool is_url_cross_origin =
      global_scope_->DocumentSecurityOrigin() &&
      !global_scope_->DocumentSecurityOrigin()->IsSameOriginWith(
          SecurityOrigin::Create(url_).get());
  const bool is_cors_passing = script_resource->GetResponse().GetType() ==
                               network::mojom::FetchResponseType::kCors;

  const bool is_opaque_cross_origin =
      (is_url_cross_origin && !is_cors_passing) || is_cors_or_access_check;

  WorkletModuleError::Type error_type = WorkletModuleError::Type::kUnknown;
  int http_status_code = 0;

  const bool is_http_error =
      (script_resource->ErrorOccurred() &&
       script_resource->GetResourceError().IsCancelledFromHttpError()) ||
      (script_resource->GetResponse().IsHTTP() &&
       !network::IsSuccessfulStatus(
           script_resource->GetResponse().HttpStatusCode()));

  if (is_cors_or_access_check) {
    error_type = WorkletModuleError::Type::kCors;
  } else if (is_http_error) {
    if (!is_opaque_cross_origin) {
      error_type = WorkletModuleError::Type::kHttp;
      http_status_code = script_resource->GetResponse().HttpStatusCode();
    } else {
      error_type = WorkletModuleError::Type::kNetwork;
    }
  } else if (script_resource->ErrorOccurred()) {
    error_type = WorkletModuleError::Type::kNetwork;
  } else if (!error_messages.empty()) {
    if (!script_resource->PassedIntegrityChecks()) {
      error_type = WorkletModuleError::Type::kIntegrity;
    } else {
      error_type = WorkletModuleError::Type::kMime;
    }
  }

  if (error_messages.empty()) {
    String message =
        "Failed to load worklet module script: " + url_.GetString();
    switch (error_type) {
      case WorkletModuleError::Type::kHttp:
        message = message +
                  " (HTTP status: " + String::Number(http_status_code) + ")";
        break;
      case WorkletModuleError::Type::kCors:
        message = message + " (CORS or access check error)";
        break;
      case WorkletModuleError::Type::kNetwork:
        message = message + " (Network error)";
        break;
      case WorkletModuleError::Type::kIntegrity:
        message = message + " (SRI integrity check failed)";
        break;
      case WorkletModuleError::Type::kMime:
      case WorkletModuleError::Type::kDisposed:
      case WorkletModuleError::Type::kUnknown:
        break;
    }
    global_scope_->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        ConsoleMessage::Source::kJavaScript, ConsoleMessage::Level::kError,
        message, url_.GetString(),
        /*loader=*/nullptr, script_resource->InspectorId()));
  } else {
    for (ConsoleMessage* message : error_messages) {
      global_scope_->AddConsoleMessage(message);
    }
  }

  global_scope_->GetModuleResponsesMap()->SetEntryError(
      url_, expected_module_type_,
      WorkletModuleError{.type = error_type,
                         .http_status_code = http_status_code,
                         .is_cross_origin = is_opaque_cross_origin});
}

void WorkletModuleScriptFetcher::Trace(Visitor* visitor) const {
  ModuleScriptFetcher::Trace(visitor);
  visitor->Trace(global_scope_);
}

}  // namespace blink
