// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/modulescript/module_script_fetcher.h"

#include "services/network/public/cpp/header_util.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/dom_implementation.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/resource/script_resource.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

ModuleScriptFetcher::ModuleScriptFetcher(
    base::PassKey<ModuleScriptLoader> pass_key) {}

void ModuleScriptFetcher::Client::OnFetched(
    const ModuleScriptCreationParams& params) {
  NotifyFetchFinishedSuccess(params);
}

void ModuleScriptFetcher::Client::OnFailed() {
  NotifyFetchFinishedError(HeapVector<Member<ConsoleMessage>>());
}

void ModuleScriptFetcher::Trace(Visitor* visitor) const {
  ResourceClient::Trace(visitor);
}

// <specdef href="https://html.spec.whatwg.org/C/#fetch-a-single-module-script">
std::optional<ResolvedModuleType> ModuleScriptFetcher::WasModuleLoadSuccessful(
    ScriptResource* resource,
    ModuleType expected_module_type,
    HeapVector<Member<ConsoleMessage>>* error_messages) {
  DCHECK(error_messages);
  if (resource) {
    for (const auto& message : resource->IntegrityReport().Messages()) {
      error_messages->push_back(MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kSecurity,
          mojom::blink::ConsoleMessageLevel::kError, message));
    }
  }

  // <spec step="13.1">... bodyBytes is null or failure ...</spec>
  if (!resource || resource->ErrorOccurred() ||
      !resource->PassedIntegrityChecks()) {
    return std::nullopt;
  }

  const auto& response = resource->GetResponse();
  // <spec step="13.1">... response's status is not an ok status</spec>
  if (response.IsHTTP() &&
      !network::IsSuccessfulStatus(response.HttpStatusCode())) {
    return std::nullopt;
  }

  // <spec step="13.2">Let mimeType be the result of extracting a MIME type from
  // response's header list.</spec>
  //
  // Note: For historical reasons, fetching a classic script does not include
  // MIME type checking. In contrast, module scripts will fail to load if they
  // are not of a correct MIME type.
  // We use ResourceResponse::HttpContentType() instead of MimeType(), as
  // MimeType() may be rewritten by mime sniffer.

  if (base::FeatureList::IsEnabled(
          blink::features::kJavaScriptSourcePhaseImports) &&
      expected_module_type == ModuleType::kJavaScriptOrWasm &&
      MIMETypeRegistry::IsWasmMIMEType(response.HttpContentType())) {
    return ResolvedModuleType::kWasm;
  }

  if (expected_module_type == ModuleType::kJavaScriptOrWasm &&
      MIMETypeRegistry::IsSupportedJavaScriptMIMEType(
          response.HttpContentType())) {
    return ResolvedModuleType::kJavaScript;
  }

  if (expected_module_type == ModuleType::kJSON &&
      MIMETypeRegistry::IsJSONMimeType(response.HttpContentType())) {
    return ResolvedModuleType::kJSON;
  }

  if (expected_module_type == ModuleType::kCSS &&
      MIMETypeRegistry::IsSupportedStyleSheetMIMEType(
          response.HttpContentType())) {
    return ResolvedModuleType::kCSS;
  }

  String message = StrCat(
      {"Failed to load module script: Expected a ",
       ModuleScriptCreationParams::ModuleTypeToString(expected_module_type),
       " module script but the server responded with a MIME type of \"",
       resource->GetResponse().HttpContentType(),
       "\". Strict MIME type checking is enforced for module scripts per HTML "
       "spec."});

  error_messages->push_back(MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kJavaScript,
      mojom::ConsoleMessageLevel::kError, message,
      response.ResponseUrl().GetString(), /*loader=*/nullptr,
      resource->InspectorId()));
  return std::nullopt;
}

}  // namespace blink
