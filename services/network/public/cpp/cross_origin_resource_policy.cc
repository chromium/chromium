// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cross_origin_resource_policy.h"

#include <string>

#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "services/network/public/cpp/document_isolation_policy.h"
#include "services/network/public/cpp/initiator_lock_compatibility.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/cross_origin_embedder_policy.mojom.h"
#include "services/network/public/mojom/document_isolation_policy.mojom-forward.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace network {

namespace {

// https://fetch.spec.whatwg.org/#cross-origin-resource-policy-header says:
// > ABNF:
// >  Cross-Origin-Resource-Policy = %s"same-origin" / %s"same-site"
// >  ; case-sensitive
//
// https://tools.ietf.org/html/rfc7405 says:
// > The following prefixes are allowed:
// >      %s          =  case-sensitive
// >      %i          =  case-insensitive
CrossOriginResourcePolicy::ParsedHeader ParseHeaderByString(
    std::optional<std::string> header_value) {
  if (!header_value)
    return CrossOriginResourcePolicy::kNoHeader;

  if (header_value == "same-origin")
    return CrossOriginResourcePolicy::kSameOrigin;

  if (header_value == "same-site")
    return CrossOriginResourcePolicy::kSameSite;

  if (header_value == "cross-origin")
    return CrossOriginResourcePolicy::kCrossOrigin;

  // TODO(lukasza): Once https://github.com/whatwg/fetch/issues/760 gets
  // resolved, add support for parsing specific origins.
  return CrossOriginResourcePolicy::kParsingError;
}

CrossOriginResourcePolicy::ParsedHeader ParseHeaderByHttpResponseHeaders(
    const net::HttpResponseHeaders* headers) {
  std::string header_value;
  if (!headers || !headers->GetNormalizedHeader(
                      CrossOriginResourcePolicy::kHeaderName, &header_value))
    return CrossOriginResourcePolicy::kNoHeader;
  return ParseHeaderByString(header_value);
}

std::string GetDomain(const url::Origin& origin) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      origin, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

bool ShouldAllowSameSite(const url::Origin& initiator,
                         const url::Origin& target_origin) {
  // Different sites might be served from the same IP address - they should
  // still be considered to be different sites - see also
  // https://url.spec.whatwg.org/#host-same-site which excludes IP addresses by
  // imposing the requirement that one of the addresses has to have a non-null
  // registrable domain.
  if (initiator.GetURL().HostIsIPAddress() ||
      target_origin.GetURL().HostIsIPAddress()) {
    return false;
  }

  // https://fetch.spec.whatwg.org/#cross-origin-resource-policy-header, step 5
  // says to allow "CORP: same-site" responses
  // > [...] If the following are true:
  // > - request’s origin’s host is same site with request’s current URL’s host
  // > - [...]
  if (GetDomain(initiator) != GetDomain(target_origin))
    return false;

  // https://fetch.spec.whatwg.org/#cross-origin-resource-policy-header, step 5
  // says to allow "CORP: same-site" responses
  // > [...] If the following are true:
  // > - [...]
  // > - request’s origin’s scheme is "https" or response’s HTTPS state is
  //     "none"
  //
  // |target_origin.scheme() != url::kHttpsScheme| is pretty much equivalent to
  // |response’s HTTPS state is "none"| based on the following spec snippets:
  // - https://fetch.spec.whatwg.org/#http-network-fetch says:
  //     > If response was retrieved over HTTPS, set its HTTPS state to either
  //       "deprecated" or "modern".
  //   and the same spec section also hints that broken responses should result
  //   in network errors (rather than "none" or other http state):
  //     > User agents are strongly encouraged to only succeed HTTPS connections
  //       with strong security properties and return network errors otherwise.
  return initiator.scheme() == url::kHttpsScheme ||
         target_origin.scheme() != url::kHttpsScheme;
}

std::optional<mojom::BlockedByResponseReason> IsBlockedInternal(
    CrossOriginResourcePolicy::ParsedHeader policy,
    const GURL& request_url,
    const std::optional<url::Origin>& request_initiator,
    mojom::RequestMode request_mode,
    bool request_include_credentials,
    mojom::CrossOriginEmbedderPolicyValue embedder_policy,
    mojom::DocumentIsolationPolicyValue document_isolation_policy) {
  // Browser-initiated requests are not subject to Cross-Origin-Resource-Policy.
  if (!request_initiator.has_value())
    return std::nullopt;
  const url::Origin& initiator = request_initiator.value();

  bool require_corp_due_to_coep;
  switch (embedder_policy) {
    case mojom::CrossOriginEmbedderPolicyValue::kNone:
      require_corp_due_to_coep = false;
      break;

    case mojom::CrossOriginEmbedderPolicyValue::kCredentialless:
      require_corp_due_to_coep =
          request_mode == mojom::RequestMode::kNavigate ||
          request_include_credentials;
      break;

    case mojom::CrossOriginEmbedderPolicyValue::kRequireCorp:
      require_corp_due_to_coep = true;
      break;
  }

  bool require_corp_due_to_dip;
  switch (document_isolation_policy) {
    case mojom::DocumentIsolationPolicyValue::kNone:
      require_corp_due_to_dip = false;
      break;

    case mojom::DocumentIsolationPolicyValue::kIsolateAndCredentialless:
      require_corp_due_to_dip = request_mode == mojom::RequestMode::kNavigate ||
                                request_include_credentials;
      break;

    case mojom::DocumentIsolationPolicyValue::kIsolateAndRequireCorp:
      require_corp_due_to_dip = true;
      break;
  }

  // COEP https://mikewest.github.io/corpp/#corp-check
  bool upgrade_to_same_origin_due_to_coep = false;
  bool upgrade_to_same_origin_due_to_dip = false;
  if ((policy == CrossOriginResourcePolicy::kNoHeader ||
       policy == CrossOriginResourcePolicy::kParsingError) &&
      (require_corp_due_to_coep || require_corp_due_to_dip)) {
    policy = CrossOriginResourcePolicy::kSameOrigin;
    upgrade_to_same_origin_due_to_coep = require_corp_due_to_coep;
    upgrade_to_same_origin_due_to_dip = require_corp_due_to_dip;
  }

  if (policy == CrossOriginResourcePolicy::kNoHeader ||
      policy == CrossOriginResourcePolicy::kParsingError ||
      policy == CrossOriginResourcePolicy::kCrossOrigin) {
    // The algorithm only returns kBlock from steps 4 and 6, when policy is
    // either kSameOrigin or kSameSite.  For other policy values we can
    // immediately execute step 7 and return kAllow.
    //
    // From https://fetch.spec.whatwg.org/#cross-origin-resource-policy-header:
    // > 7.  Return allowed.
    return std::nullopt;
  }

  // From https://fetch.spec.whatwg.org/#cross-origin-resource-policy-header:
  // > 2. If request’s origin is same origin with request’s current URL’s
  //      origin, then return allowed.
  url::Origin target_origin = url::Origin::Create(request_url);
  if (initiator == target_origin)
    return std::nullopt;

  // From https://fetch.spec.whatwg.org/#cross-origin-resource-policy-header:
  // > 4. If policy is `same-origin`, then return blocked.
  if (policy == CrossOriginResourcePolicy::kSameOrigin) {
    if (upgrade_to_same_origin_due_to_coep &&
        upgrade_to_same_origin_due_to_dip) {
      return mojom::BlockedByResponseReason::
          kCorpNotSameOriginAfterDefaultedToSameOriginByCoepAndDip;
    }

    if (upgrade_to_same_origin_due_to_coep) {
      return mojom::BlockedByResponseReason::
          kCorpNotSameOriginAfterDefaultedToSameOriginByCoep;
    }

    if (upgrade_to_same_origin_due_to_dip) {
      return mojom::BlockedByResponseReason::
          kCorpNotSameOriginAfterDefaultedToSameOriginByDip;
    }
    return mojom::BlockedByResponseReason::kCorpNotSameOrigin;
  }

  // From https://fetch.spec.whatwg.org/#cross-origin-resource-policy-header:
  // > 5. If the following are true
  // >      * request’s origin’s host is same site with request’s current URL’s
  // >        host
  // >      * request’s origin’s scheme is "https" or response’s HTTPS state is
  // >      "none"
  // >    then return allowed.
  if (ShouldAllowSameSite(initiator, target_origin))
    return std::nullopt;

  // From https://fetch.spec.whatwg.org/#cross-origin-resource-policy-header:
  // > 6.  If policy is `same-site`, then return blocked.
  DCHECK_EQ(CrossOriginResourcePolicy::kSameSite, policy);
  return mojom::BlockedByResponseReason::kCorpNotSameSite;
}

std::optional<mojom::BlockedByResponseReason> IsBlockedInternalWithReporting(
    CrossOriginResourcePolicy::ParsedHeader policy,
    const GURL& request_url,
    const GURL& original_url,
    const std::optional<url::Origin>& request_initiator,
    mojom::RequestMode request_mode,
    mojom::RequestDestination request_destination,
    bool request_include_credentials,
    const CrossOriginEmbedderPolicy& embedder_policy,
    mojom::CrossOriginEmbedderPolicyReporter* reporter,
    const DocumentIsolationPolicy& document_isolation_policy) {
  constexpr auto kBlockedDueToCoep = mojom::BlockedByResponseReason::
      kCorpNotSameOriginAfterDefaultedToSameOriginByCoep;
  constexpr auto kBlockedDueToCoepAndDip = mojom::BlockedByResponseReason::
      kCorpNotSameOriginAfterDefaultedToSameOriginByCoepAndDip;
  if ((embedder_policy.report_only_value ==
           mojom::CrossOriginEmbedderPolicyValue::kRequireCorp ||
       (embedder_policy.report_only_value ==
            mojom::CrossOriginEmbedderPolicyValue::kCredentialless &&
        request_mode == mojom::RequestMode::kNavigate)) &&
      reporter) {
    const auto result = IsBlockedInternal(
        policy, request_url, request_initiator, request_mode,
        request_include_credentials, embedder_policy.report_only_value,
        document_isolation_policy.report_only_value);
    if (result == kBlockedDueToCoep || result == kBlockedDueToCoepAndDip ||
        (result.has_value() && request_mode == mojom::RequestMode::kNavigate)) {
      reporter->QueueCorpViolationReport(original_url, request_destination,
                                         /*report_only=*/true);
    }

    // TODO(https://issues.chromium.org/issues/333029815)
    // Emit a report when Document-Isolation-Policy enforcement would block
    // subresource load.
  }

  if (request_mode == mojom::RequestMode::kNavigate &&
      embedder_policy.value == mojom::CrossOriginEmbedderPolicyValue::kNone) {
    return std::nullopt;
  }

  const auto result =
      IsBlockedInternal(policy, request_url, request_initiator, request_mode,
                        request_include_credentials, embedder_policy.value,
                        document_isolation_policy.value);
  if (reporter &&
      (result == kBlockedDueToCoep || result == kBlockedDueToCoepAndDip ||
       (result.has_value() && request_mode == mojom::RequestMode::kNavigate))) {
    reporter->QueueCorpViolationReport(original_url, request_destination,
                                       /*report_only=*/false);
  }

  // TODO(https://issues.chromium.org/issues/333029815)
  // Emit a report when Document-Isolation-Policy enforcement blocks
  // subresource load.
  return result;
}

}  // namespace

// static
const char CrossOriginResourcePolicy::kHeaderName[] =
    "Cross-Origin-Resource-Policy";

// static
std::optional<mojom::BlockedByResponseReason>
CrossOriginResourcePolicy::IsBlocked(
    const GURL& request_url,
    const GURL& original_url,
    const std::optional<url::Origin>& request_initiator,
    const network::mojom::URLResponseHead& response,
    mojom::RequestMode request_mode,
    mojom::RequestDestination request_destination,
    const CrossOriginEmbedderPolicy& embedder_policy,
    mojom::CrossOriginEmbedderPolicyReporter* reporter,
    const DocumentIsolationPolicy& document_isolation_policy) {
  // From https://fetch.spec.whatwg.org/#cross-origin-resource-policy-header:
  // > 1. If request’s mode is not "no-cors", then return allowed.
  if (request_mode != mojom::RequestMode::kNoCors)
    return std::nullopt;

  // From https://fetch.spec.whatwg.org/#cross-origin-resource-policy-header:
  // > 3. Let policy be the result of getting `Cross-Origin-Resource-Policy`
  //      from response’s header list.
  //
  // We parse the header earlier than requested by the spec (i.e. we swap steps
  // 2 and 3 from the spec), to return early if there was no header (before
  // slightly more expensive steps needed to extract the origins below).
  ParsedHeader policy =
      ParseHeaderByHttpResponseHeaders(response.headers.get());

  return IsBlockedInternalWithReporting(
      policy, request_url, original_url, request_initiator, request_mode,
      request_destination, response.request_include_credentials,
      embedder_policy, reporter, document_isolation_policy);
}

// static
std::optional<mojom::BlockedByResponseReason>
CrossOriginResourcePolicy::IsBlockedByHeaderValue(
    const GURL& request_url,
    const GURL& original_url,
    const std::optional<url::Origin>& request_initiator,
    std::optional<std::string> corp_header_value,
    mojom::RequestMode request_mode,
    mojom::RequestDestination request_destination,
    bool request_include_credentials,
    const CrossOriginEmbedderPolicy& embedder_policy,
    mojom::CrossOriginEmbedderPolicyReporter* reporter,
    const DocumentIsolationPolicy& document_isolation_policy) {
  // From https://fetch.spec.whatwg.org/#cross-origin-resource-policy-header:
  // > 1. If request’s mode is not "no-cors", then return allowed.
  if (request_mode != mojom::RequestMode::kNoCors)
    return std::nullopt;

  ParsedHeader policy = ParseHeaderByString(corp_header_value);

  return IsBlockedInternalWithReporting(
      policy, request_url, original_url, request_initiator, request_mode,
      request_destination, request_include_credentials, embedder_policy,
      reporter, document_isolation_policy);
}

// static
std::optional<mojom::BlockedByResponseReason>
CrossOriginResourcePolicy::IsNavigationBlocked(
    const GURL& request_url,
    const GURL& original_url,
    const std::optional<url::Origin>& request_initiator,
    const network::mojom::URLResponseHead& response,
    mojom::RequestDestination request_destination,
    const CrossOriginEmbedderPolicy& embedder_policy,
    mojom::CrossOriginEmbedderPolicyReporter* reporter) {
  ParsedHeader policy =
      ParseHeaderByHttpResponseHeaders(response.headers.get());

  return IsBlockedInternalWithReporting(
      policy, request_url, original_url, request_initiator,
      mojom::RequestMode::kNavigate, request_destination,
      response.request_include_credentials, embedder_policy, reporter,
      DocumentIsolationPolicy());
}

// static
CrossOriginResourcePolicy::ParsedHeader
CrossOriginResourcePolicy::ParseHeaderForTesting(
    const net::HttpResponseHeaders* headers) {
  return ParseHeaderByHttpResponseHeaders(headers);
}

// static
bool CrossOriginResourcePolicy::ShouldAllowSameSiteForTesting(
    const url::Origin& initiator,
    const url::Origin& target_origin) {
  return ShouldAllowSameSite(initiator, target_origin);
}

}  // namespace network
