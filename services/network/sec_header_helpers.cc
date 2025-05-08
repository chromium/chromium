// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/sec_header_helpers.h"

#include <algorithm>
#include <string>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/types/optional_ref.h"
#include "net/base/isolation_info.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "net/cookies/cookie_util.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/cors/origin_access_list.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/initiator_lock_compatibility.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/request_destination.h"
#include "services/network/public/cpp/request_mode.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace network {

namespace {

constexpr std::string_view kSecFetchMode = "Sec-Fetch-Mode";
constexpr std::string_view kSecFetchSite = "Sec-Fetch-Site";
constexpr std::string_view kSecFetchUser = "Sec-Fetch-User";
constexpr std::string_view kSecFetchDest = "Sec-Fetch-Dest";
constexpr std::string_view kSecFetchStorageAccess = "Sec-Fetch-Storage-Access";
constexpr std::string_view kSecFetchFrameTop = "Sec-Fetch-Frame-Top";

constexpr char kSecFetchStorageAccessOutcomeHistogram[] =
    "API.StorageAccessHeader.SecFetchStorageAccessOutcome";

std::string_view OriginRelationString(
    std::optional<net::OriginRelation> relation) {
  if (!relation.has_value()) {
    return "none";
  }

  switch (relation.value()) {
    case net::OriginRelation::kSameOrigin:
      return "same-origin";
    case net::OriginRelation::kSameSite:
      return "same-site";
    case net::OriginRelation::kCrossSite:
      return "cross-site";
  }
}

// Walk through a URL chain and a pending redirect url to calculate their
// relationship to the origin.
net::OriginRelation GetRelationOfURLChainToOrigin(
    const std::vector<GURL>& request_chain,
    const url::Origin& origin,
    base::optional_ref<const GURL> pending_redirect_url) {
  CHECK(!request_chain.empty());

  auto origin_relation = net::OriginRelation::kSameOrigin;
  for (const GURL& target_url : request_chain) {
    origin_relation =
        std::max(origin_relation, net::GetOriginRelation(target_url, origin));
  }

  if (pending_redirect_url.has_value()) {
    origin_relation =
        std::max(origin_relation,
                 net::GetOriginRelation(pending_redirect_url.value(), origin));
  }
  return origin_relation;
}

// Returns the relationship between a request (including its url and url_chain)
// and the request's top_frame_origin in the form of a
// net::OriginRelation.
std::optional<net::OriginRelation> GetFrameTopRelation(
    const net::URLRequest& request,
    base::optional_ref<const GURL> pending_redirect_url) {
  if (request.isolation_info().IsEmpty() ||
      request.isolation_info().request_type() ==
          net::IsolationInfo::RequestType::kMainFrame) {
    return std::nullopt;
  }

  const url::Origin& top_frame_origin =
      request.isolation_info().top_frame_origin().value();

  return GetRelationOfURLChainToOrigin(request.url_chain(), top_frame_origin,
                                       pending_redirect_url);
}

std::optional<net::OriginRelation> GetInitiatorRelation(
    const net::URLRequest& request,
    base::optional_ref<const GURL> pending_redirect_url,
    const mojom::URLLoaderFactoryParams& factory_params,
    const cors::OriginAccessList& origin_access_list) {
  // Browser-initiated requests with no initiator origin will send
  // `Sec-Fetch-Site: None`.
  if (!request.initiator().has_value()) {
    // CorsURLLoaderFactory::IsValidRequest verifies that only the browser
    // process may initiate requests with no request initiator.
    DCHECK_EQ(factory_params.process_id, mojom::kBrowserProcessId);

    return std::nullopt;
  }
  const url::Origin& initiator = request.initiator().value();

  // Privileged requests initiated from a "non-webby" context will send
  // `Sec-Fetch-Site: None` while unprivileged ones will send
  // `Sec-Fetch-Site: cross-site`.
  if (factory_params.unsafe_non_webby_initiator) {
    cors::OriginAccessList::AccessState access_state =
        origin_access_list.CheckAccessState(initiator, request.url());
    bool is_privileged =
        (access_state == cors::OriginAccessList::AccessState::kAllowed);

    if (is_privileged) {
      return std::nullopt;
    }

    return net::OriginRelation::kCrossSite;
  }

  // Other requests default to `kSameOrigin`, and walk through the request's URL
  // chain to calculate the correct value.
  return GetRelationOfURLChainToOrigin(request.url_chain(), initiator,
                                       pending_redirect_url);
}

char const* GetSecFetchStorageAccessHeaderValue(
    net::cookie_util::StorageAccessStatus storage_access_status) {
  switch (storage_access_status) {
    case net::cookie_util::StorageAccessStatus::kInactive:
      return "inactive";
    case net::cookie_util::StorageAccessStatus::kActive:
      return "active";
    case net::cookie_util::StorageAccessStatus::kNone:
      return "none";
  }
  NOTREACHED();
}

net::cookie_util::SecFetchStorageAccessOutcome
ComputeSecFetchStorageAccessOutcome(const net::URLRequest& request,
                                    mojom::CredentialsMode credentials_mode) {
  if (request.storage_access_status().IsSet() &&
      !request.storage_access_status().GetStatusForThirdPartyContext()) {
    return net::cookie_util::SecFetchStorageAccessOutcome::
        kOmittedStatusMissing;
  }
  if (credentials_mode != mojom::CredentialsMode::kInclude) {
    return net::cookie_util::SecFetchStorageAccessOutcome::
        kOmittedRequestOmitsCredentials;
  }
  CHECK(request.storage_access_status().IsSet());
  switch (
      request.storage_access_status().GetStatusForThirdPartyContext().value()) {
    case net::cookie_util::StorageAccessStatus::kInactive:
      return net::cookie_util::SecFetchStorageAccessOutcome::kValueInactive;
    case net::cookie_util::StorageAccessStatus::kActive:
      return net::cookie_util::SecFetchStorageAccessOutcome::kValueActive;
    case net::cookie_util::StorageAccessStatus::kNone:
      return net::cookie_util::SecFetchStorageAccessOutcome::kValueNone;
  }
  NOTREACHED();
}

// Sec-Fetch-Site
void SetSecFetchSiteHeader(net::URLRequest& request,
                           base::optional_ref<const GURL> pending_redirect_url,
                           const mojom::URLLoaderFactoryParams& factory_params,
                           const cors::OriginAccessList& origin_access_list) {
  std::optional<net::OriginRelation> relation = GetInitiatorRelation(
      request, pending_redirect_url, factory_params, origin_access_list);

  request.SetExtraRequestHeaderByName(kSecFetchSite,
                                      OriginRelationString(relation),
                                      /* overwrite = */ true);
}

// Sec-Fetch-Mode
void SetSecFetchModeHeader(net::URLRequest& request,
                           network::mojom::RequestMode mode) {
  std::string header_value = RequestModeToString(mode);

  request.SetExtraRequestHeaderByName(kSecFetchMode, header_value, false);
}

// Sec-Fetch-User
void SetSecFetchUserHeader(net::URLRequest& request, bool has_user_activation) {
  if (has_user_activation)
    request.SetExtraRequestHeaderByName(kSecFetchUser, "?1", true);
  else
    request.RemoveRequestHeaderByName(kSecFetchUser);
}

// Sec-Fetch-Dest
void SetSecFetchDestHeader(net::URLRequest& request,
                           network::mojom::RequestDestination dest) {
  // https://w3c.github.io/webappsec-fetch-metadata/#abstract-opdef-set-dest
  // If r's destination is the empty string, set header's value to the string
  // "empty". Otherwise, set header's value to r's destination.
  std::string header_value = RequestDestinationToString(
      dest, EmptyRequestDestinationOption::kUseFiveCharEmptyString);
  request.SetExtraRequestHeaderByName(kSecFetchDest, header_value, true);
}

// Sec-Fetch-Storage-Access
void SetSecFetchStorageAccessHeader(net::URLRequest& request,
                                    mojom::CredentialsMode credentials_mode) {
  base::UmaHistogramEnumeration(
      kSecFetchStorageAccessOutcomeHistogram,
      ComputeSecFetchStorageAccessOutcome(request, credentials_mode));

  if (credentials_mode != mojom::CredentialsMode::kInclude ||
      (request.storage_access_status().IsSet() &&
       !request.storage_access_status().GetStatusForThirdPartyContext())) {
    // A credentials mode of "same-origin" or "omit" prevents including cookies
    // on the request in the first place, so we don't bother to include the
    // `Sec-Fetch-Storage-Access` header in that case.
    //
    // To ensure that an erroneous value isn't sent by mistake (and that
    // consumers aren't allowed to override the correct "omitted" value), we
    // clear any existing value.
    request.RemoveRequestHeaderByName(kSecFetchStorageAccess);
    return;
  }
  CHECK(request.storage_access_status().IsSet());
  request.SetExtraRequestHeaderByName(
      kSecFetchStorageAccess,
      GetSecFetchStorageAccessHeaderValue(request.storage_access_status()
                                              .GetStatusForThirdPartyContext()
                                              .value()),
      /*overwrite=*/true);
}

// Sec-Fetch-Frame-Top
void SetSecFetchFrameTop(net::URLRequest& request,
                         base::optional_ref<const GURL> pending_redirect_url) {
  if (!base::FeatureList::IsEnabled(features::kFrameAncestorHeaders)) {
    return;
  }

  std::optional<net::OriginRelation> relation =
      GetFrameTopRelation(request, pending_redirect_url);
  if (!relation.has_value()) {
    return;
  }

  request.SetExtraRequestHeaderByName(kSecFetchFrameTop,
                                      OriginRelationString(relation),
                                      /*overwrite=*/true);
}

}  // namespace

void SetFetchMetadataHeaders(
    net::URLRequest& request,
    network::mojom::RequestMode mode,
    bool has_user_activation,
    network::mojom::RequestDestination dest,
    base::optional_ref<const GURL> pending_redirect_url,
    const mojom::URLLoaderFactoryParams& factory_params,
    const cors::OriginAccessList& origin_access_list,
    mojom::CredentialsMode credentials_mode) {
  DCHECK_NE(0u, request.url_chain().size());

  // Only append the header to potentially trustworthy URLs.
  const GURL& target_url = pending_redirect_url.has_value()
                               ? pending_redirect_url.value()
                               : request.url();
  if (!IsUrlPotentiallyTrustworthy(target_url))
    return;

  SetSecFetchSiteHeader(request, pending_redirect_url, factory_params,
                        origin_access_list);
  SetSecFetchModeHeader(request, mode);
  SetSecFetchUserHeader(request, has_user_activation);
  SetSecFetchDestHeader(request, dest);
  SetSecFetchStorageAccessHeader(request, credentials_mode);
  SetSecFetchFrameTop(request, pending_redirect_url);
}

void MaybeRemoveSecHeaders(net::URLRequest& request,
                           const GURL& pending_redirect_url) {
  // If our redirect destination is not trusted it would not have had sec-ch-
  // or sec-fetch- prefixed headers added to it. Our previous hops may have
  // added these headers if the current url is trustworthy though so we should
  // try to remove these now.
  if (IsUrlPotentiallyTrustworthy(request.url()) &&
      !IsUrlPotentiallyTrustworthy(pending_redirect_url)) {
    // Check each of our request headers and if it is a "sec-ch-" or
    // "sec-fetch-" prefixed header we'll remove it.
    const net::HttpRequestHeaders::HeaderVector request_headers =
        request.extra_request_headers().GetHeaderVector();
    for (const auto& header : request_headers) {
      if (StartsWith(header.key, "sec-ch-",
                     base::CompareCase::INSENSITIVE_ASCII) ||
          StartsWith(header.key, "sec-fetch-",
                     base::CompareCase::INSENSITIVE_ASCII)) {
        request.RemoveRequestHeaderByName(header.key);
      }
    }
  }
}

}  // namespace network
