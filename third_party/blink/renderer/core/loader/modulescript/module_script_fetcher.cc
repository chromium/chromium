// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/modulescript/module_script_fetcher.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/dom_implementation.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/subresource_integrity_helper.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

ModuleScriptFetcher::ModuleScriptFetcher(
    base::PassKey<ModuleScriptLoader> pass_key) {}

void ModuleScriptFetcher::Client::OnFetched(
    const base::Optional<ModuleScriptCreationParams>& params) {
  NotifyFetchFinished(params, HeapVector<Member<ConsoleMessage>>());
}

void ModuleScriptFetcher::Client::OnFailed() {
  NotifyFetchFinished(base::nullopt, HeapVector<Member<ConsoleMessage>>());
}

void ModuleScriptFetcher::Trace(Visitor* visitor) const {
  ResourceClient::Trace(visitor);
}

// <specdef href="https://html.spec.whatwg.org/C/#fetch-a-single-module-script">
bool ModuleScriptFetcher::WasModuleLoadSuccessful(
    Resource* resource,
    HeapVector<Member<ConsoleMessage>>* error_messages,
    ModuleScriptCreationParams::ModuleType* module_type) {
  DCHECK(error_messages);

  if (resource) {
    SubresourceIntegrityHelper::GetConsoleMessages(
        resource->IntegrityReportInfo(), error_messages);
  }

  // <spec step="9">... response's type is "error" ...</spec>
  if (!resource || resource->ErrorOccurred() ||
      resource->IntegrityDisposition() !=
          ResourceIntegrityDisposition::kPassed) {
    return false;
  }

  const auto& response = resource->GetResponse();
  // <spec step="9">... response's status is not an ok status</spec>
  if (response.IsHTTP() && !cors::IsOkStatus(response.HttpStatusCode())) {
    return false;
  }

  // <spec step="10">Let type be the result of extracting a MIME type from
  // response's header list.</spec>
  //
  // Note: For historical reasons, fetching a classic script does not include
  // MIME type checking. In contrast, module scripts will fail to load if they
  // are not of a correct MIME type.
  // We use ResourceResponse::HttpContentType() instead of MimeType(), as
  // MimeType() may be rewritten by mime sniffer.
  //
  // <spec step="12">If type is a JavaScript MIME type, then:</spec>
  if (MIMETypeRegistry::IsSupportedJavaScriptMIMEType(
          response.HttpContentType())) {
    *module_type = ModuleScriptCreationParams::ModuleType::kJavaScriptModule;
    return true;
  }
  // <spec step="13">If type is a JSON MIME type, then:</spec>
  if (base::FeatureList::IsEnabled(blink::features::kJSONModules) &&
      MIMETypeRegistry::IsJSONMimeType(response.HttpContentType())) {
    *module_type = ModuleScriptCreationParams::ModuleType::kJSONModule;
    return true;
  }

  if (RuntimeEnabledFeatures::CSSModulesEnabled() &&
      MIMETypeRegistry::IsSupportedStyleSheetMIMEType(
          response.HttpContentType())) {
    *module_type = ModuleScriptCreationParams::ModuleType::kCSSModule;
    return true;
  }
  String required_response_type = "JavaScript";
  if (base::FeatureList::IsEnabled(blink::features::kJSONModules)) {
    required_response_type = required_response_type + ", JSON";
  }
  if (RuntimeEnabledFeatures::CSSModulesEnabled()) {
    required_response_type = required_response_type + ", CSS";
  }

  String message =
      "Failed to load module script: The server responded with a non-" +
      required_response_type + " MIME type of \"" +
      resource->GetResponse().HttpContentType() +
      "\". Strict MIME type checking is enforced for module scripts per HTML "
      "spec.";
  error_messages->push_back(MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kJavaScript,
      mojom::ConsoleMessageLevel::kError, message,
      response.CurrentRequestUrl().GetString(), /*loader=*/nullptr,
      resource->InspectorId()));
  return false;
}

}  // namespace blink
