// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/speculation_rules/speculation_rules_header.h"

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "net/http/structured_headers.h"
#include "services/network/public/mojom/fetch_api.mojom-blink-forward.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-shared.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-shared.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/loader/resource/speculation_rules_resource.h"
#include "third_party/blink/renderer/core/loader/speculation_rule_loader.h"
#include "third_party/blink/renderer/core/speculation_rules/speculation_rules_metrics.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

SpeculationRulesHeader::SpeculationRulesHeader() = default;
SpeculationRulesHeader::~SpeculationRulesHeader() = default;

// static
void SpeculationRulesHeader::ProcessHeadersForDocumentResponse(
    const ResourceResponse& response,
    LocalDOMWindow& window) {
  // If the Speculation-Rules header isn't present at all, then there's nothing
  // to do.
  const AtomicString& header_value =
      response.HttpHeaderField(http_names::kSpeculationRules);
  if (!header_value)
    return;

  window.CountUse(WebFeature::kSpeculationRulesHeader);

  SpeculationRulesHeader self;
  self.ParseSpeculationRulesHeader(header_value, window.BaseURL());
  self.ReportErrors(window);
  self.StartFetches(*window.document());
}

void SpeculationRulesHeader::ParseSpeculationRulesHeader(
    const String& header_value,
    const KURL& base_url) {
  auto parsed_header = net::structured_headers::ParseList(header_value.Utf8());
  if (!parsed_header.has_value()) {
    String message = "Cannot parse Speculation-Rules header value.";
    if (KURL(base_url, header_value.StripWhiteSpace()).IsValid()) {
      message = message + " However, " +
                header_value.StripWhiteSpace().EncodeForDebugging() +
                " appears to be a valid URL. "
                "You may need to enclose it in quotation marks.";
    }
    errors_.push_back(std::pair(
        SpeculationRulesLoadOutcome::kUnparseableSpeculationRulesHeader,
        message));
    return;
  }

  if (parsed_header->empty()) {
    // This is valid, but unlikely to be intentional. Let's make a note of it.
    CountSpeculationRulesLoadOutcome(
        SpeculationRulesLoadOutcome::kEmptySpeculationRulesHeader);
    return;
  }

  for (auto const& parsed_item : parsed_header.value()) {
    // Only strings are valid list members.
    if (parsed_item.member.size() != 1u ||
        !parsed_item.member[0].item.is_string()) {
      String message =
          "Only strings are valid in Speculation-Rules header value "
          "and inner lists are ignored.";
      if (parsed_item.member.size() == 1u &&
          parsed_item.member[0].item.is_token()) {
        String token = String::FromUTF8(parsed_item.member[0].item.GetString());
        if (KURL(base_url, token).IsValid()) {
          message = message + " However, " + token.EncodeForDebugging() +
                    " appears to be a valid URL. "
                    "You may need to enclose it in quotation marks.";
        }
      }
      errors_.push_back(std::pair(
          SpeculationRulesLoadOutcome::kInvalidSpeculationRulesHeaderItem,
          message));
      continue;
    }
    const auto& url_str = String(parsed_item.member[0].item.GetString());
    KURL speculation_rule_url(base_url, url_str);
    if (url_str.empty() || !speculation_rule_url.IsValid()) {
      errors_.push_back(std::pair(
          SpeculationRulesLoadOutcome::kInvalidSpeculationRulesHeaderItem,
          String("URL \"" + url_str +
                 "\" found in Speculation-Rules header is invalid.")));
      continue;
    }
    urls_.push_back(std::move(speculation_rule_url));
  }
}

void SpeculationRulesHeader::ReportErrors(LocalDOMWindow& window) {
  for (const auto& [outcome, error] : errors_) {
    CountSpeculationRulesLoadOutcome(outcome);

    if (error) {
      window.AddConsoleMessage(mojom::blink::ConsoleMessageSource::kOther,
                               mojom::blink::ConsoleMessageLevel::kWarning,
                               error);
    }
  }
}

void SpeculationRulesHeader::StartFetches(Document& document) {
  for (const KURL& speculation_rule_url : urls_) {
    ResourceRequest resource_request(speculation_rule_url);
    resource_request.SetPrefetchMaybeForTopLevelNavigation(false);
    resource_request.SetFetchPriorityHint(
        mojom::blink::FetchPriorityHint::kLow);

    // Always use CORS. Adopt new best practices for subresources: CORS requests
    // with same-origin credentials only.
    auto* origin = document.GetExecutionContext()->GetSecurityOrigin();
    resource_request.SetMode(network::mojom::RequestMode::kCors);
    resource_request.SetCredentialsMode(
        network::mojom::CredentialsMode::kSameOrigin);
    resource_request.RemoveUserAndPassFromURL();
    resource_request.SetRequestorOrigin(origin);
    resource_request.SetHTTPOrigin(origin);

    resource_request.SetRequestContext(
        mojom::blink::RequestContextType::SPECULATION_RULES);
    resource_request.SetRequestDestination(
        network::mojom::blink::RequestDestination::kSpeculationRules);

    ResourceLoaderOptions options(
        document.GetExecutionContext()->GetCurrentWorld());
    options.initiator_info.name = fetch_initiator_type_names::kOther;

    FetchParameters speculation_rule_params(std::move(resource_request),
                                            options);

    SpeculationRulesResource* resource = SpeculationRulesResource::Fetch(
        speculation_rule_params, document.Fetcher());

    SpeculationRuleLoader* speculation_rule_loader =
        MakeGarbageCollected<SpeculationRuleLoader>(document);
    speculation_rule_loader->LoadResource(resource);
  }
}

}  // namespace blink
