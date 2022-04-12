// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/private_network_access_check.h"

#include "base/metrics/histogram_functions.h"
#include "services/network/public/cpp/ip_address_space_util.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace network {
namespace {

using Policy = mojom::PrivateNetworkRequestPolicy;
using Result = PrivateNetworkAccessCheckResult;

Result PrivateNetworkAccessCheckInternal(
    const mojom::ClientSecurityState* client_security_state,
    mojom::IPAddressSpace target_address_space,
    absl::optional<mojom::IPAddressSpace> previous_response_address_space,
    int32_t url_load_options,
    mojom::IPAddressSpace resource_address_space) {
  if (url_load_options & mojom::kURLLoadOptionBlockLocalRequest &&
      IsLessPublicAddressSpace(resource_address_space,
                               mojom::IPAddressSpace::kPublic)) {
    return Result::kBlockedByLoadOption;
  }

  if (target_address_space != mojom::IPAddressSpace::kUnknown) {
    if (resource_address_space == target_address_space) {
      return Result::kAllowedByTargetIpAddressSpace;
    }

    if (client_security_state &&
        client_security_state->private_network_request_policy ==
            mojom::PrivateNetworkRequestPolicy::kPreflightWarn) {
      return Result::kAllowedByPolicyPreflightWarn;
    }

    return Result::kBlockedByTargetIpAddressSpace;
  }

  if (previous_response_address_space.has_value() &&
      resource_address_space != *previous_response_address_space) {
    // `previous_response_address_space` behaves similarly to
    // `target_address_space`, except `kUnknown` is also subject to checks
    // (instead absl::nullopt indicates that no check should be performed).
    //
    // If the policy is `kPreflightWarn`, the request should not fail just
    // because of this check - PNA checks are only experimentally turned on
    // for this request. Further checks should not be run, otherwise we might
    // return `kBlockedByPolicyPreflightWarn` and trigger a new preflight to be
    // sent, thus causing https://crbug.com/1279376 all over again.
    if (client_security_state &&
        client_security_state->private_network_request_policy ==
            mojom::PrivateNetworkRequestPolicy::kPreflightWarn) {
      return Result::kAllowedByPolicyPreflightWarn;
    }

    return Result::kBlockedByInconsistentIpAddressSpace;
  }

  if (!client_security_state) {
    return Result::kAllowedMissingClientSecurityState;
  }

  if (!IsLessPublicAddressSpace(resource_address_space,
                                client_security_state->ip_address_space)) {
    return Result::kAllowedNoLessPublic;
  }

  // We use a switch statement to force this code to be amended when values are
  // added to the `PrivateNetworkRequestPolicy` enum.
  switch (client_security_state->private_network_request_policy) {
    case Policy::kAllow:
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

}  // namespace

Result PrivateNetworkAccessCheck(
    const mojom::ClientSecurityState* client_security_state,
    mojom::IPAddressSpace target_address_space,
    absl::optional<mojom::IPAddressSpace> previous_response_address_space,
    int32_t url_load_options,
    mojom::IPAddressSpace resource_address_space) {
  Result result = PrivateNetworkAccessCheckInternal(
      client_security_state, target_address_space,
      previous_response_address_space, url_load_options,
      resource_address_space);
  base::UmaHistogramEnumeration("Security.PrivateNetworkAccess.CheckResult",
                                result);
  return result;
}

}  // namespace network
