// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/local_network_access_checker.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "net/base/transport_info.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/ip_address_space_util.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"

namespace network {
namespace {

using Result = LocalNetworkAccessCheckResult;
using Policy = mojom::LocalNetworkRequestPolicy;

mojom::ClientSecurityStatePtr GetRequestClientSecurityState(
    const ResourceRequest& request) {
  if (!request.trusted_params.has_value()) {
    return nullptr;
  }

  return request.trusted_params->client_security_state.Clone();
}

// WARNING: This should be kept in sync with similar logic in
// `network::cors::CorsURLLoader::GetClientSecurityState()`.
const mojom::ClientSecurityState* ChooseClientSecurityState(
    const mojom::ClientSecurityState* factory_client_security_state,
    const mojom::ClientSecurityState* request_client_security_state) {
  if (factory_client_security_state) {
    // Enforce that only one ClientSecurityState is ever given to us, as this
    // is an invariant in the current codebase. In case of a compromised
    // renderer process, we might be passed both, in which case we prefer to
    // use the factory params' value: contrary to the request params, it is
    // always sourced from the browser process.
    DCHECK(!request_client_security_state)
        << "Must not provide a ClientSecurityState in both "
           "URLLoaderFactoryParams and ResourceRequest::TrustedParams.";

    return factory_client_security_state;
  }

  return request_client_security_state;
}

absl::optional<net::IPAddress> ParseLocalIpFromUrl(const GURL& url) {
  net::IPAddress address;
  if (!address.AssignFromIPLiteral(url.HostNoBracketsPiece())) {
    return absl::nullopt;
  }

  if (IPAddressToIPAddressSpace(address) != mojom::IPAddressSpace::kLocal) {
    return absl::nullopt;
  }

  return address;
}

}  // namespace

LocalNetworkAccessChecker::LocalNetworkAccessChecker(
    const ResourceRequest& request,
    const mojom::ClientSecurityState* factory_client_security_state,
    int32_t url_load_options)
    : request_client_security_state_(GetRequestClientSecurityState(request)),
      client_security_state_(
          ChooseClientSecurityState(factory_client_security_state,
                                    request_client_security_state_.get())),
      should_block_local_request_(url_load_options &
                                  mojom::kURLLoadOptionBlockLocalRequest),
      target_address_space_(request.target_ip_address_space),
      request_initiator_(request.request_initiator) {
  SetRequestUrl(request.url);

  if (!client_security_state_ ||
      client_security_state_->local_network_request_policy ==
          mojom::LocalNetworkRequestPolicy::kAllow) {
    // No client security state means PNA is implicitly disabled. A policy of
    // `kAllow` means PNA is explicitly disabled. In both cases, the target IP
    // address space should not be set on the request.
    DCHECK_EQ(target_address_space_, mojom::IPAddressSpace::kUnknown)
        << request.url;
  }
}

LocalNetworkAccessChecker::~LocalNetworkAccessChecker() = default;

Result LocalNetworkAccessChecker::Check(
    const net::TransportInfo& transport_info) {
  // If the request URL host was a local IP, record whether we ended up
  // connecting to that IP address, unless connecting through a proxy.
  // See https://crbug.com/1381471#c2.
  if (request_url_local_ip_.has_value() &&
      transport_info.type != net::TransportType::kProxied) {
    base::UmaHistogramBoolean(
        "Security.PrivateNetworkAccess.PrivateIpResolveMatch",
        *request_url_local_ip_ == transport_info.endpoint.address());
  }

  mojom::IPAddressSpace resource_address_space =
      TransportInfoToIPAddressSpace(transport_info);

  // If we are connecting to a local IP endpoint over HTTP without a target IP
  // address space, record whether we could have successfully inferred the
  // target IP address space from the request URL.
  if (resource_address_space == mojom::IPAddressSpace::kLocal &&
      is_request_url_scheme_http_ &&
      target_address_space_ == mojom::IPAddressSpace::kUnknown) {
    base::UmaHistogramBoolean(
        "Security.PrivateNetworkAccess.PrivateIpInferrable",
        request_url_local_ip_.has_value());
  }

  auto result = CheckInternal(resource_address_space);

  base::UmaHistogramEnumeration("Security.PrivateNetworkAccess.CheckResult",
                                result);

  if (transport_info.type == net::TransportType::kCached) {
    base::UmaHistogramEnumeration(
        "Security.PrivateNetworkAccess.CachedResourceCheckResult", result);
  }

  response_address_space_ = resource_address_space;
  return result;
}

void LocalNetworkAccessChecker::ResetForRedirect(const GURL& new_url) {
  SetRequestUrl(new_url);
  ResetForRetry();
}

void LocalNetworkAccessChecker::ResetForRetry() {
  // The target IP address space is no longer relevant, it only applied to the
  // URL before the first redirect/retry. Consider the following scenario:
  //
  // 1. `https://public.example` fetches `http://localhost/foo`
  // 2. `OnConnected()` notices that the remote endpoint's IP address space is
  //    `kLoopback`, fails the request with
  //    `CorsError::UnexpectedPrivateNetworkAccess`.
  // 3. A preflight request is sent with `target_ip_address_space_` set to
  //    `kLoopback`, succeeds.
  // 4. `http://localhost/foo` redirects the GET request to
  //    `https://public2.example/bar`.
  //
  // The target IP address space `kLoopback` should not be applied to the new
  // connection obtained to `https://public2.example`.
  //
  // See also: https://crbug.com/1293891
  target_address_space_ = mojom::IPAddressSpace::kUnknown;

  response_address_space_ = absl::nullopt;
}

mojom::ClientSecurityStatePtr
LocalNetworkAccessChecker::CloneClientSecurityState() const {
  if (!client_security_state_) {
    return nullptr;
  }

  return client_security_state_->Clone();
}

mojom::IPAddressSpace LocalNetworkAccessChecker::ClientAddressSpace() const {
  if (!client_security_state_) {
    return mojom::IPAddressSpace::kUnknown;
  }

  return client_security_state_->ip_address_space;
}

bool LocalNetworkAccessChecker::IsPolicyPreflightWarn() const {
  return client_security_state_ &&
         client_security_state_->local_network_request_policy ==
             mojom::LocalNetworkRequestPolicy::kPreflightWarn;
}

Result LocalNetworkAccessChecker::CheckInternal(
    mojom::IPAddressSpace resource_address_space) {
  if (should_block_local_request_ &&
      IsLessPublicAddressSpace(resource_address_space,
                               mojom::IPAddressSpace::kPublic)) {
    return Result::kBlockedByLoadOption;
  }

  if (is_potentially_trustworthy_same_origin_ &&
      base::FeatureList::IsEnabled(
          features::kLocalNetworkAccessAllowPotentiallyTrustworthySameOrigin)) {
    return Result::kAllowedPotentiallyTrustworthySameOrigin;
  }

  if (!client_security_state_) {
    return Result::kAllowedMissingClientSecurityState;
  }

  mojom::LocalNetworkRequestPolicy policy =
      client_security_state_->local_network_request_policy;

  if (policy == mojom::LocalNetworkRequestPolicy::kAllow) {
    return Result::kAllowedByPolicyAllow;
  }

  if (target_address_space_ != mojom::IPAddressSpace::kUnknown) {
    if (resource_address_space == target_address_space_) {
      return Result::kAllowedByTargetIpAddressSpace;
    }

    if (policy == mojom::LocalNetworkRequestPolicy::kPreflightWarn) {
      return Result::kAllowedByPolicyPreflightWarn;
    }

    return Result::kBlockedByTargetIpAddressSpace;
  }

  // A single response may connect to two different IP address spaces without
  // a redirect in between. This can happen due to split range requests, where
  // a single `URLRequest` issues multiple network transactions, or when we
  // create a new connection after auth credentials have been provided, etc.
  //
  // `response_address_space_` behaves similarly to `target_address_space_`,
  // except `kUnknown` is also subject to checks (instead
  // `response_address_space_ == absl::nullopt` indicates that no check
  // should be performed).
  if (response_address_space_.has_value() &&
      resource_address_space != *response_address_space_) {
    // If the policy is `kWarn` or `kPreflightWarn`, the request should not fail
    // just because of this check - PNA checks are only experimentally turned on
    // for this request. Further checks should not be run, otherwise we might
    // return `kBlockedByPolicyPreflightWarn` and trigger a new preflight to be
    // sent, thus causing https://crbug.com/1279376 all over again.
    if (policy == mojom::LocalNetworkRequestPolicy::kPreflightWarn) {
      return Result::kAllowedByPolicyPreflightWarn;
    }

    // See also https://crbug.com/1334689.
    if (policy == mojom::LocalNetworkRequestPolicy::kWarn) {
      return Result::kAllowedByPolicyWarn;
    }

    return Result::kBlockedByInconsistentIpAddressSpace;
  }

  if (!IsLessPublicAddressSpace(resource_address_space,
                                client_security_state_->ip_address_space)) {
    return Result::kAllowedNoLessPublic;
  }

  // We use a switch statement to force this code to be amended when values are
  // added to the `LocalNetworkRequestPolicy` enum.
  switch (policy) {
    case Policy::kAllow:
      NOTREACHED();  // Should have been handled by the if statement above.
      return Result::kAllowedByPolicyAllow;
    case Policy::kWarn:
      return Result::kAllowedByPolicyWarn;
    case Policy::kBlock:
      return Result::kBlockedByPolicyBlock;
    case Policy::kPreflightWarn:
      return Result::kBlockedByPolicyPreflightWarn;
    case Policy::kPreflightBlock:
      return Result::kBlockedByPolicyPreflightBlock;
  }
}

void LocalNetworkAccessChecker::SetRequestUrl(const GURL& url) {
  is_request_url_scheme_http_ = url.scheme_piece() == url::kHttpScheme;
  request_url_local_ip_ = ParseLocalIpFromUrl(url);

  is_potentially_trustworthy_same_origin_ =
      IsUrlPotentiallyTrustworthy(url) && request_initiator_.has_value() &&
      request_initiator_.value().IsSameOriginWith(url);
}

}  // namespace network
