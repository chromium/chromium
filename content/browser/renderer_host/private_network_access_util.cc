// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "content/browser/renderer_host/policy_container_host.h"
#include "content/public/common/content_features.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"

namespace content {

using Policy = network::mojom::PrivateNetworkRequestPolicy;

Policy DerivePrivateNetworkRequestPolicy(
    const PolicyContainerPolicies& policies) {
  // The goal is to eliminate occurrences of this case as much as possible,
  // before removing this special case.
  if (policies.ip_address_space == network::mojom::IPAddressSpace::kUnknown) {
    if (!policies.is_web_secure_context &&
        base::FeatureList::IsEnabled(
            features::kBlockInsecurePrivateNetworkRequestsFromUnknown)) {
      return Policy::kBlock;
    }

    return Policy::kAllow;
  }

  // The rest of this function enumerates cases from the strictest policy
  // (`kBlock`) to the least strict (`kAllow`).

  // Apply the secure context restriction, if enabled.
  if (!policies.is_web_secure_context) {
    if (policies.ip_address_space == network::mojom::IPAddressSpace::kPrivate) {
      // Requests from the `private` address space to localhost are blocked if
      // the right feature is enabled and the initiating context is not secure.
      // This is controlled separately because private network websites face
      // additional hurdles compared to public websites. See crbug.com/1234044.
      if (base::FeatureList::IsEnabled(
              features::kBlockInsecurePrivateNetworkRequestsFromPrivate)) {
        return Policy::kBlock;
      }
    } else if (base::FeatureList::IsEnabled(
                   features::kBlockInsecurePrivateNetworkRequests)) {
      // Private network requests from the `public` address space are blocked if
      // the right feature is enabled and the initiating context is not secure.
      //
      // NOTE: We also set this when `policies.ip_address_space` is `kLocal`,
      // but that has no effect. Indeed, requests initiated from the local
      // address space are never considered private network requests - they
      // cannot target more-private address spaces.
      return Policy::kBlock;
    }
  }

  if (base::FeatureList::IsEnabled(
          features::kPrivateNetworkAccessRespectPreflightResults)) {
    return Policy::kPreflightBlock;
  }

  if (base::FeatureList::IsEnabled(
          features::kPrivateNetworkAccessSendPreflights)) {
    return Policy::kPreflightWarn;
  }

  if (!policies.is_web_secure_context ||
      base::FeatureList::IsEnabled(
          features::kWarnAboutSecurePrivateNetworkRequests)) {
    return Policy::kWarn;
  }

  return Policy::kAllow;
}

}  // namespace content
