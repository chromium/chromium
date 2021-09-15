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

PrivateNetworkAccessCheckResult PrivateNetworkAccessCheckInternal(
    const network::mojom::ClientSecurityState* client_security_state,
    int32_t url_load_options,
    mojom::IPAddressSpace resource_address_space) {
  if (url_load_options & mojom::kURLLoadOptionBlockLocalRequest &&
      IsLessPublicAddressSpace(resource_address_space,
                               network::mojom::IPAddressSpace::kPublic)) {
    return PrivateNetworkAccessCheckResult::kBlockedByLoadOption;
  }

  if (!client_security_state) {
    return PrivateNetworkAccessCheckResult::kAllowedMissingClientSecurityState;
  }

  if (!IsLessPublicAddressSpace(resource_address_space,
                                client_security_state->ip_address_space)) {
    return PrivateNetworkAccessCheckResult::kAllowedNoLessPublic;
  }

  // We use a switch statement to force this code to be amended when values are
  // added to the `PrivateNetworkRequestPolicy` enum.
  switch (client_security_state->private_network_request_policy) {
    case mojom::PrivateNetworkRequestPolicy::kAllow:
      return PrivateNetworkAccessCheckResult::kAllowedByPolicyAllow;
    case mojom::PrivateNetworkRequestPolicy::kWarn:
      return PrivateNetworkAccessCheckResult::kAllowedByPolicyWarn;
    case mojom::PrivateNetworkRequestPolicy::kBlock:
      return PrivateNetworkAccessCheckResult::kBlockedByPolicyBlock;
  }
}

}  // namespace

PrivateNetworkAccessCheckResult PrivateNetworkAccessCheck(
    const network::mojom::ClientSecurityState* client_security_state,
    int32_t url_load_options,
    mojom::IPAddressSpace resource_address_space) {
  PrivateNetworkAccessCheckResult result = PrivateNetworkAccessCheckInternal(
      client_security_state, url_load_options, resource_address_space);
  base::UmaHistogramEnumeration("Security.PrivateNetworkAccess.CheckResult",
                                result);
  return result;
}

}  // namespace network
