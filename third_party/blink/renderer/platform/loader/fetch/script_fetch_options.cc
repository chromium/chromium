// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/script_fetch_options.h"

#include <utility>

#include "third_party/blink/renderer/platform/loader/attribution_header_constants.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

// https://html.spec.whatwg.org/C/#fetch-a-classic-script
FetchParameters ScriptFetchOptions::CreateFetchParameters(
    const KURL& url,
    const SecurityOrigin* security_origin,
    scoped_refptr<const DOMWrapperWorld> world_for_csp,
    CrossOriginAttributeValue cross_origin,
    const WTF::TextEncoding& encoding,
    FetchParameters::DeferOption defer) const {
  // Step 1. Let request be the result of creating a potential-CORS request
  // given url, ... [spec text]
  ResourceRequest resource_request(url);

  // Step 1. ... "script", ... [spec text]
  ResourceLoaderOptions resource_loader_options(std::move(world_for_csp));
  resource_loader_options.initiator_info.name = "script";
  resource_loader_options.reject_coep_unsafe_none = reject_coep_unsafe_none_;
  FetchParameters params(std::move(resource_request), resource_loader_options);
  params.SetRequestContext(mojom::blink::RequestContextType::SCRIPT);
  params.SetRequestDestination(network::mojom::RequestDestination::kScript);
  params.SetRenderBlockingBehavior(render_blocking_behavior_);

  // Step 1. ... and CORS setting. [spec text]
  if (cross_origin != kCrossOriginAttributeNotSet)
    params.SetCrossOriginAccessControl(security_origin, cross_origin);

  // Step 2. Set request's client to settings object. [spec text]
  // Note: Implemented at ClassicPendingScript::Fetch().

  // Step 3. Set up the classic script request given request and options. [spec
  // text]
  //
  // https://html.spec.whatwg.org/C/#set-up-the-classic-script-request
  // Set request's cryptographic nonce metadata to options's cryptographic
  // nonce, [spec text]
  params.SetContentSecurityPolicyNonce(Nonce());

  // its integrity metadata to options's integrity metadata, [spec text]
  params.SetIntegrityMetadata(GetIntegrityMetadata());
  params.MutableResourceRequest().SetFetchIntegrity(
      GetIntegrityAttributeValue());

  // its parser metadata to options's parser metadata, [spec text]
  params.SetParserDisposition(ParserState());

  // https://wicg.github.io/priority-hints/#script
  // set request’s priority to option’s fetchpriority
  params.MutableResourceRequest().SetFetchPriorityHint(fetch_priority_hint_);

  // its referrer policy to options's referrer policy. [spec text]
  params.MutableResourceRequest().SetReferrerPolicy(referrer_policy_);

  params.SetCharset(encoding);

  // This DeferOption logic is only for classic scripts, as we always set
  // |kLazyLoad| for module scripts in ModuleScriptLoader.
  params.SetDefer(defer);

  // Steps 4- are Implemented at ClassicPendingScript::Fetch().

  // TODO(crbug.com/1338976): Add correct spec comments here.
  if (attribution_reporting_eligibility_ ==
      AttributionReportingEligibility::kEligible) {
    params.MutableResourceRequest().SetHttpHeaderField(
        http_names::kAttributionReportingEligible,
        kAttributionEligibleEventSourceAndTrigger);
  }

  return params;
}

}  // namespace blink
