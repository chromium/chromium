// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cross_origin_resource_policy.h"

#include <string>

#include "base/feature_list.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/http/http_response_headers.h"
#include "services/network/initiator_lock_compatibility.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_response_info.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace network {

namespace {

const char kHeaderName[] = "Cross-Origin-Resource-Policy";

// https://fetch.spec.whatwg.org/#cross-origin-resource-policy-header says:
// > ABNF:
// >  Cross-Origin-Resource-Policy = %s"same-origin" / %s"same-site"
// >  ; case-sensitive
//
// https://tools.ietf.org/html/rfc7405 says:
// > The following prefixes are allowed:
// >      %s          =  case-sensitive
// >      %i          =  case-insensitive
CrossOriginResourcePolicy::ParsedHeader ParseHeader(
    const net::HttpResponseHeaders* headers) {
  std::string header_value;
  if (!headers || !headers->GetNormalizedHeader(kHeaderName, &header_value))
    return CrossOriginResourcePolicy::kNoHeader;

  if (header_value == "same-origin")
    return CrossOriginResourcePolicy::kSameOrigin;

  if (header_value == "same-site")
    return CrossOriginResourcePolicy::kSameSite;

  if (base::FeatureList::IsEnabled(features::kCrossOriginIsolation) &&
      header_value == "cross-origin") {
    return CrossOriginResourcePolicy::kCrossOrigin;
  }

  // TODO(lukasza): Once https://github.com/whatwg/fetch/issues/760 gets
  // resolved, add support for parsing specific origins.
  return CrossOriginResourcePolicy::kParsingError;
}

std::string GetDomain(const url::Origin& origin) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      origin.host(),
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
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

}  // namespace

// static
CrossOriginResourcePolicy::VerificationResult CrossOriginResourcePolicy::Verify(
    const GURL& request_url,
    const base::Optional<url::Origin>& request_initiator,
    const ResourceResponseInfo& response,
    mojom::RequestMode request_mode,
    base::Optional<url::Origin> request_initiator_site_lock,
    mojom::CrossOriginEmbedderPolicy embedder_policy) {
  // From https://fetch.spec.whatwg.org/#cross-origin-resource-policy-header:
  // > 1. If request’s mode is not "no-cors", then return allowed.
  if (request_mode != mojom::RequestMode::kNoCors)
    return kAllow;

  // From https://fetch.spec.whatwg.org/#cross-origin-resource-policy-header:
  // > 3. Let policy be the result of getting `Cross-Origin-Resource-Policy`
  //      from response’s header list.
  //
  // We parse the header earlier than requested by the spec (i.e. we swap steps
  // 2 and 3 from the spec), to return early if there was no header (before
  // slightly more expensive steps needed to extract the origins below).
  ParsedHeader policy = ParseHeader(response.headers.get());

  // COEP https://mikewest.github.io/corpp/#corp-check
  if ((policy == kNoHeader || policy == kParsingError) &&
      embedder_policy == mojom::CrossOriginEmbedderPolicy::kRequireCorp) {
    DCHECK(base::FeatureList::IsEnabled(features::kCrossOriginIsolation));
    policy = kSameOrigin;
  }

  if (policy == kNoHeader || policy == kParsingError ||
      policy == kCrossOrigin) {
    // The algorithm only returns kBlock from steps 4 and 6, when policy is
    // either kSameOrigin or kSameSite.  For other policy values we can
    // immediately execute step 7 and return kAllow.
    //
    // From https://fetch.spec.whatwg.org/#cross-origin-resource-policy-header:
    // > 7.  Return allowed.
    return kAllow;
  }

  // From https://fetch.spec.whatwg.org/#cross-origin-resource-policy-header:
  // > 2. If request’s origin is same origin with request’s current URL’s
  //      origin, then return allowed.
  url::Origin target_origin = url::Origin::Create(request_url);
  url::Origin initiator =
      GetTrustworthyInitiator(request_initiator_site_lock, request_initiator);
  if (initiator == target_origin)
    return kAllow;

  // From https://fetch.spec.whatwg.org/#cross-origin-resource-policy-header:
  // > 4. If policy is `same-origin`, then return blocked.
  if (policy == kSameOrigin)
    return kBlock;

  // From https://fetch.spec.whatwg.org/#cross-origin-resource-policy-header:
  // > 5. If the following are true
  // >      * request’s origin’s host is same site with request’s current URL’s
  // >        host
  // >      * request’s origin’s scheme is "https" or response’s HTTPS state is
  // >      "none"
  // >    then return allowed.
  if (ShouldAllowSameSite(initiator, target_origin))
    return kAllow;

  // From https://fetch.spec.whatwg.org/#cross-origin-resource-policy-header:
  // > 6.  If policy is `same-site`, then return blocked.
  DCHECK_EQ(kSameSite, policy);
  return kBlock;
}

// static
CrossOriginResourcePolicy::ParsedHeader
CrossOriginResourcePolicy::ParseHeaderForTesting(
    const net::HttpResponseHeaders* headers) {
  return ParseHeader(headers);
}

// static
bool CrossOriginResourcePolicy::ShouldAllowSameSiteForTesting(
    const url::Origin& initiator,
    const url::Origin& target_origin) {
  return ShouldAllowSameSite(initiator, target_origin);
}

}  // namespace network
