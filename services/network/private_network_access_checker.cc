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

}  // namespace

PrivateNetworkAccessChecker::PrivateNetworkAccessChecker(
    const ResourceRequest& request,
    const mojom::ClientSecurityState* client_security_state,
    int32_t url_load_options)
    : client_security_state_(client_security_state),
      should_block_local_request_(url_load_options &
                                  mojom::kURLLoadOptionBlockLocalRequest),
      request_initiator_(request.request_initiator),
      required_address_space_(request.required_ip_address_space) {
  SetRequestUrl(request.url);
}

PrivateNetworkAccessChecker::PrivateNetworkAccessChecker(
    const GURL& url,
    const std::optional<url::Origin>& request_initiator,
    mojom::IPAddressSpace required_ip_address_space,
    const mojom::ClientSecurityState* client_security_state,
    int32_t url_load_options)
    : client_security_state_(client_security_state),
      should_block_local_request_(url_load_options &
                                  mojom::kURLLoadOptionBlockLocalRequest),
      request_initiator_(request_initiator),
      required_address_space_(required_ip_address_space) {
  SetRequestUrl(url);
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

  auto result = CheckAddressSpace(resource_address_space);

  base::UmaHistogramEnumeration("Security.PrivateNetworkAccess.CheckResult",
                                result);

  response_address_space_ = resource_address_space;
  return result;
}

PrivateNetworkAccessCheckResult PrivateNetworkAccessChecker::Check(
    const net::IPEndPoint& server_address) {
  mojom::IPAddressSpace resource_address_space =
      IPEndPointToIPAddressSpace(server_address);

  auto result = CheckAddressSpace(resource_address_space);

  base::UmaHistogramEnumeration("Security.PrivateNetworkAccess.CheckResult",
                                result);

  response_address_space_ = resource_address_space;
  return result;
}

Result PrivateNetworkAccessChecker::CheckAddressSpace(
    mojom::IPAddressSpace resource_address_space) {
  if (should_block_local_request_ &&
      IsLessPublicAddressSpace(resource_address_space,
                               mojom::IPAddressSpace::kPublic)) {
    return Result::kBlockedByLoadOption;
  }

  if (is_potentially_trustworthy_same_origin_) {
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

  // A single response may connect to two different IP address spaces without
  // a redirect in between. This can happen due to split range requests, where
  // a single `URLRequest` issues multiple network transactions, or when we
  // create a new connection after auth credentials have been provided, etc.
  if (response_address_space_.has_value() &&
      resource_address_space != *response_address_space_) {
    // See also https://crbug.com/1334689.
    if (policy == mojom::PrivateNetworkRequestPolicy::kWarn) {
      return Result::kAllowedByPolicyWarn;
    }

    return Result::kBlockedByInconsistentIpAddressSpace;
  }

  // `required_address_space_` is the IP address space the website claimed the
  // subresource to be. If it doesn't meet the real situation, then we should
  // fail the request.
  //
  // TODO(crbug.com/395895368): consider collapsing the address spaces for LNA
  // checks.
  if (base::FeatureList::IsEnabled(features::kLocalNetworkAccessChecks) &&
      required_address_space_ != mojom::IPAddressSpace::kUnknown &&
      resource_address_space != required_address_space_) {
    return Result::kBlockedByRequiredIpAddressSpaceMismatch;
  }

  // Currently for LNA we are only blocking public -> local/private/loopback
  // requests. Requests from local -> loopback (or private -> local in PNA
  // terminology) are not blocked at present.
  if (base::FeatureList::IsEnabled(features::kLocalNetworkAccessChecks)) {
    if (!IsLessPublicAddressSpaceLNA(
            resource_address_space, client_security_state_->ip_address_space)) {
      return Result::kAllowedNoLessPublic;
    }
  } else {
    if (!IsLessPublicAddressSpace(resource_address_space,
                                  client_security_state_->ip_address_space)) {
      return Result::kAllowedNoLessPublic;
    }
  }

  // We use a switch statement to force this code to be amended when values are
  // added to the `PrivateNetworkRequestPolicy` enum.
  switch (policy) {
    case Policy::kAllow:
      NOTREACHED();  // Should have been handled by the if statement above.
    case Policy::kWarn:
      return Result::kAllowedByPolicyWarn;
    case Policy::kBlock:
      return Result::kBlockedByPolicyBlock;
    case Policy::kPermissionBlock:
      return Result::kLNAPermissionRequired;
    case Policy::kPermissionWarn:
      return Result::kLNAAllowedByPolicyWarn;
  }
}

void PrivateNetworkAccessChecker::ResetForRedirect(const GURL& new_url) {
  SetRequestUrl(new_url);
  ResetForRetry();
}

void PrivateNetworkAccessChecker::ResetForRetry() {
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

void PrivateNetworkAccessChecker::SetRequestUrl(const GURL& url) {
  is_request_url_scheme_http_ = url.scheme() == url::kHttpScheme;
  request_url_private_ip_ = ParsePrivateIpFromUrl(url);

  is_potentially_trustworthy_same_origin_ =
      IsUrlPotentiallyTrustworthy(url) && request_initiator_.has_value() &&
      request_initiator_.value().IsSameOriginWith(url);
}

}  // namespace network
