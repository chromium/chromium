// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/private_network_access_checker.h"

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

using Result = PrivateNetworkAccessCheckResult;
using Policy = mojom::PrivateNetworkRequestPolicy;

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

std::optional<net::IPAddress> ParsePrivateIpFromUrl(const GURL& url) {
  net::IPAddress address;
  if (!address.AssignFromIPLiteral(url.HostNoBracketsPiece())) {
    return std::nullopt;
  }

  if (IPAddressToIPAddressSpace(address) != mojom::IPAddressSpace::kPrivate) {
    return std::nullopt;
  }

  return address;
}

}  // namespace

PrivateNetworkAccessChecker::PrivateNetworkAccessChecker(
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
      request_initiator_(request.request_initiator),
      required_address_space_(request.required_ip_address_space) {
  SetRequestUrl(request.url);

  if (!client_security_state_ ||
      client_security_state_->private_network_request_policy ==
          mojom::PrivateNetworkRequestPolicy::kAllow) {
    // No client security state means PNA is implicitly disabled. A policy of
    // `kAllow` means PNA is explicitly disabled. In both cases, the target IP
    // address space should not be set on the request.
    DCHECK_EQ(target_address_space_, mojom::IPAddressSpace::kUnknown)
        << request.url;
  }
}

PrivateNetworkAccessChecker::~PrivateNetworkAccessChecker() = default;

PrivateNetworkAccessCheckResult PrivateNetworkAccessChecker::Check(
    const net::TransportInfo& transport_info) {
  // If the request URL host was a private IP, record whether we ended up
  // connecting to that IP address, unless connecting through a proxy.
  // See https://crbug.com/1381471#c2.
  if (request_url_private_ip_.has_value() &&
      transport_info.type != net::TransportType::kProxied) {
    base::UmaHistogramBoolean(
        "Security.PrivateNetworkAccess.PrivateIpResolveMatch",
        *request_url_private_ip_ == transport_info.endpoint.address());
  }

  mojom::IPAddressSpace resource_address_space =
      TransportInfoToIPAddressSpace(transport_info);

  // If we are connecting to a private IP endpoint over HTTP without a target IP
  // address space, record whether we could have successfully inferred the
  // target IP address space from the request URL.
  if (resource_address_space == mojom::IPAddressSpace::kPrivate &&
      is_request_url_scheme_http_ &&
      target_address_space_ == mojom::IPAddressSpace::kUnknown) {
    base::UmaHistogramBoolean(
        "Security.PrivateNetworkAccess.PrivateIpInferrable",
        request_url_private_ip_.has_value());
  }

  auto result = CheckInternal(resource_address_space);

  base::UmaHistogramEnumeration("Security.PrivateNetworkAccess.CheckResult",
                                result);

  response_address_space_ = resource_address_space;
  return result;
}

void PrivateNetworkAccessChecker::ResetForRedirect(const GURL& new_url) {
  SetRequestUrl(new_url);
  ResetForRetry();
}

void PrivateNetworkAccessChecker::ResetForRetry() {
  // The target IP address space is no longer relevant, it only applied to the
  // URL before the first redirect/retry. Consider the following scenario:
  //
  // 1. `https://public.example` fetches `http://localhost/foo`
  // 2. `OnConnected()` notices that the remote endpoint's IP address space is
  //    `kLocal`, fails the request with
  //    `CorsError::UnexpectedPrivateNetworkAccess`.
  // 3. A preflight request is sent with `target_ip_address_space_` set to
  //    `kLocal`, succeeds.
  // 4. `http://localhost/foo` redirects the GET request to
  //    `https://public2.example/bar`.
  //
  // The target IP address space `kLocal` should not be applied to the new
  // connection obtained to `https://public2.example`.
  //
  // See also: https://crbug.com/1293891
  target_address_space_ = mojom::IPAddressSpace::kUnknown;

  response_address_space_ = std::nullopt;
}

mojom::ClientSecurityStatePtr
PrivateNetworkAccessChecker::CloneClientSecurityState() const {
  if (!client_security_state_) {
    return nullptr;
  }

  return client_security_state_->Clone();
}

mojom::IPAddressSpace PrivateNetworkAccessChecker::ClientAddressSpace() const {
  if (!client_security_state_) {
    return mojom::IPAddressSpace::kUnknown;
  }

  return client_security_state_->ip_address_space;
}

bool PrivateNetworkAccessChecker::IsPolicyPreflightWarn() const {
  return client_security_state_ &&
         client_security_state_->private_network_request_policy ==
             mojom::PrivateNetworkRequestPolicy::kPreflightWarn;
}

Result PrivateNetworkAccessChecker::CheckInternal(
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

  mojom::PrivateNetworkRequestPolicy policy =
      client_security_state_->private_network_request_policy;

  if (policy == mojom::PrivateNetworkRequestPolicy::kAllow) {
    return Result::kAllowedByPolicyAllow;
  }

  if (target_address_space_ != mojom::IPAddressSpace::kUnknown) {
    if (resource_address_space == target_address_space_) {
      return Result::kAllowedByTargetIpAddressSpace;
    }

    if (policy == mojom::PrivateNetworkRequestPolicy::kPreflightWarn) {
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
  // `response_address_space_ == std::nullopt` indicates that no check
  // should be performed).
  if (response_address_space_.has_value() &&
      resource_address_space != *response_address_space_) {
    // If the policy is `kWarn` or `kPreflightWarn`, the request should not fail
    // just because of this check - PNA checks are only experimentally turned on
    // for this request. Further checks should not be run, otherwise we might
    // return `kBlockedByPolicyPreflightWarn` and trigger a new preflight to be
    // sent, thus causing https://crbug.com/1279376 all over again.
    if (policy == mojom::PrivateNetworkRequestPolicy::kPreflightWarn) {
      return Result::kAllowedByPolicyPreflightWarn;
    }

    // See also https://crbug.com/1334689.
    if (policy == mojom::PrivateNetworkRequestPolicy::kWarn) {
      return Result::kAllowedByPolicyWarn;
    }

    return Result::kBlockedByInconsistentIpAddressSpace;
  }

  // `required_address_space_` is the IP address space the website claimed the
  // subresource to be. If it doesn't meet the real situation, then we should
  // fail the request.
  if (base::FeatureList::IsEnabled(
          features::kPrivateNetworkAccessPermissionPrompt) &&
      required_address_space_ != mojom::IPAddressSpace::kUnknown &&
      resource_address_space != required_address_space_) {
    return Result::kBlockedByTargetIpAddressSpace;
  }

  if (!IsLessPublicAddressSpace(resource_address_space,
                                client_security_state_->ip_address_space)) {
    return Result::kAllowedNoLessPublic;
  }

  // We use a switch statement to force this code to be amended when values are
  // added to the `PrivateNetworkRequestPolicy` enum.
  switch (policy) {
    case Policy::kAllow:
      NOTREACHED_IN_MIGRATION();  // Should have been handled by the if
                                  // statement above.
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

void PrivateNetworkAccessChecker::SetRequestUrl(const GURL& url) {
  is_request_url_scheme_http_ = url.scheme_piece() == url::kHttpScheme;
  request_url_private_ip_ = ParsePrivateIpFromUrl(url);

  is_potentially_trustworthy_same_origin_ =
      IsUrlPotentiallyTrustworthy(url) && request_initiator_.has_value() &&
      request_initiator_.value().IsSameOriginWith(url);
}

bool PrivateNetworkAccessChecker::NeedPermission(
    const GURL& url,
    bool is_web_secure_context,
    mojom::IPAddressSpace target_address_space) {
  return base::FeatureList::IsEnabled(
             network::features::kPrivateNetworkAccessPermissionPrompt) &&
         is_web_secure_context && !network::IsUrlPotentiallyTrustworthy(url) &&
         (target_address_space == mojom::IPAddressSpace::kLocal ||
          target_address_space == mojom::IPAddressSpace::kPrivate);
}

}  // namespace network
