// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/speculation_rules/speculation_rules_header.h"

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "net/http/structured_headers.h"
#include "services/network/public/mojom/fetch_api.mojom-blink-forward.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/origin_trials/trial_token.h"
#include "third_party/blink/public/common/origin_trials/trial_token_result.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-shared.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-shared.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/loader/resource/speculation_rules_resource.h"
#include "third_party/blink/renderer/core/loader/speculation_rule_loader.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/speculation_rules/speculation_rules_metrics.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

namespace {

// These are the only trials which can be enabled via this approach.
// This is not a general-purpose way of enabling trials.
constexpr base::StringPiece kSpeculationRulesHeaderTrials[] = {
    "SpeculationRulesPrefetchFuture",
};

}  // namespace

SpeculationRulesHeader::SpeculationRulesHeader() = default;
SpeculationRulesHeader::~SpeculationRulesHeader() = default;

// static
void SpeculationRulesHeader::ProcessHeadersForDocumentResponse(
    const ResourceResponse& response,
    LocalDOMWindow& window) {
  // If speculation rules fetch from header isn't enabled, nor is the ability to
  // turn it on given an Origin-Trial token found in the same response, then we
  // should not process the Speculation-Rules header at all.
  const bool can_enable_origin_trial = base::FeatureList::IsEnabled(
      features::kSpeculationRulesHeaderEnableThirdPartyOriginTrial);
  if (!RuntimeEnabledFeatures::SpeculationRulesFetchFromHeaderEnabled(
          &window) &&
      !can_enable_origin_trial) {
    return;
  }

  // If the Speculation-Rules header isn't present at all, then there's nothing
  // to do.
  const AtomicString& header_value =
      response.HttpHeaderField(http_names::kSpeculationRules);
  if (!header_value)
    return;

  window.CountUse(WebFeature::kSpeculationRulesHeader);

  SpeculationRulesHeader self;
  self.ParseSpeculationRulesHeader(header_value, window.BaseURL());

  // If we might be able to enable an origin trial, parse the Origin-Trial
  // header to find the relevant tokens and load them in.
  if (can_enable_origin_trial) {
    self.ParseOriginTrialHeader(
        response.HttpHeaderField(http_names::kOriginTrial),
        window.GetSecurityContext());
    self.MaybeEnableFeatureFromOriginTrial(window);
  }

  // After doing so, if fetching Speculation-Rules from a header is enabled, we
  // can proceed.
  if (RuntimeEnabledFeatures::SpeculationRulesFetchFromHeaderEnabled(&window)) {
    self.ReportErrors(window);
    self.StartFetches(*window.document());
  }
}

void SpeculationRulesHeader::ParseSpeculationRulesHeader(
    const String& header_value,
    const KURL& base_url) {
  auto parsed_header = net::structured_headers::ParseList(header_value.Utf8());
  if (!parsed_header.has_value()) {
    errors_.push_back(std::pair(
        SpeculationRulesLoadOutcome::kUnparseableSpeculationRulesHeader,
        "Cannot parse Speculation-Rules header value."));
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
      errors_.push_back(std::pair(
          SpeculationRulesLoadOutcome::kInvalidSpeculationRulesHeaderItem,
          "Only strings are valid in Speculation-Rules header value "
          "and inner lists are ignored."));
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

// This extracts tokens from the Origin-Trial header and validates them.
//
// In particular, while OriginTrialContext takes care of the standard
// validation, we want to expressly check that only certain speculation rules
// related origin trials can be enabled in connection with the Speculation-Rules
// header.
//
// This is not intended be a general way of enabling origin trials; this applies
// only to certain trials, and then only to tokens which can be enabled via a
// third-party origin.
//
// To confirm this we need to invoke some of the validation code to obtain a
// parsed and validated token, and examine the contained feature name, before
// storing in the set of tokens which will be injected into OriginTrialContext.
void SpeculationRulesHeader::ParseOriginTrialHeader(
    const String& header_value,
    SecurityContext& security_context) {
  std::unique_ptr<Vector<String>> tokens =
      OriginTrialContext::ParseHeaderValue(header_value);
  if (!tokens || tokens->empty())
    return;

  // Use an opaque origin as the first-party origin, so that we won't end up
  // accepting any tokens which are valid as first-party. The normal code path
  // should handle those.
  TrialTokenValidator::OriginInfo first_party{
      SecurityOrigin::CreateUniqueOpaque()->ToUrlOrigin(),
      security_context.GetSecureContextMode() ==
          SecureContextMode::kSecureContext,
  };
  Vector<TrialTokenValidator::OriginInfo> third_parties;
  for (const KURL& url : urls_) {
    auto origin = SecurityOrigin::Create(url);
    if (origin->IsOpaque() ||
        origin->IsSameOriginWith(security_context.GetSecurityOrigin())) {
      continue;
    }
    third_parties.emplace_back(origin->ToUrlOrigin());
  }

  TrialTokenValidator validator;
  for (String& token : *tokens) {
    TrialTokenResult result = validator.ValidateTokenAndTrialWithOriginInfo(
        StringUTF8Adaptor(token).AsStringPiece(), first_party, third_parties,
        base::Time::Now());

    // Don't store tokens which don't validate.
    if (result.Status() != OriginTrialTokenStatus::kSuccess)
      continue;

    const auto& parsed_token = *result.ParsedToken();
    DCHECK(parsed_token.is_third_party());

    // Don't store tokens which correspond to a trial that is not in the allow
    // list.
    if (!base::Contains(kSpeculationRulesHeaderTrials,
                        parsed_token.feature_name())) {
      continue;
    }

    origin_trial_tokens_.push_back(std::move(token));
  }
}

void SpeculationRulesHeader::MaybeEnableFeatureFromOriginTrial(
    ExecutionContext& execution_context) {
  Vector<scoped_refptr<SecurityOrigin>> external_origins;
  for (const KURL& url : urls_) {
    auto origin = SecurityOrigin::Create(url);
    if (origin->IsOpaque() ||
        execution_context.GetSecurityOrigin()->IsSameOriginWith(origin.get())) {
      continue;
    }
    external_origins.push_back(std::move(origin));
  }
  if (external_origins.empty() || origin_trial_tokens_.empty())
    return;
  for (const String& token : origin_trial_tokens_) {
    execution_context.GetOriginTrialContext()->AddTokenFromExternalScript(
        token, external_origins);
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
