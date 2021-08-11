// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "content/browser/renderer_host/policy_container_host.h"
#include "content/public/common/content_features.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"

namespace content {

network::mojom::PrivateNetworkRequestPolicy DerivePrivateNetworkRequestPolicy(
    const PolicyContainerPolicies& policies) {
  // For now, we always allow private network requests from secure contexts.
  // We will eventually want to send CORS preflight requests for these and gate
  // the actual request on a successful preflight response. Before then, we will
  // want to warn developers about the upcoming change using a feature flag.
  if (policies.is_web_secure_context) {
    return base::FeatureList::IsEnabled(
               features::kWarnAboutSecurePrivateNetworkRequests)
               ? network::mojom::PrivateNetworkRequestPolicy::kWarn
               : network::mojom::PrivateNetworkRequestPolicy::kAllow;
  }

  // Requests from non-secure contexts in the unknown address space are allowed
  // so as not to break anything unexpectedly. The goal is to eliminate
  // occurrences of this case as much as possible.
  if (policies.ip_address_space == network::mojom::IPAddressSpace::kUnknown) {
    return network::mojom::PrivateNetworkRequestPolicy::kAllow;
  }

  // Requests from the `private` address space to localhost are blocked if the
  // right feature is enabled and the initiating context is not secure. This is
  // controlled separately because private network websites face additional
  // hurdles compared to public websites. See crbug.com/1234044.
  if (policies.ip_address_space == network::mojom::IPAddressSpace::kPrivate) {
    return base::FeatureList::IsEnabled(
               features::kBlockInsecurePrivateNetworkRequestsFromPrivate)
               ? network::mojom::PrivateNetworkRequestPolicy::kBlock
               : network::mojom::PrivateNetworkRequestPolicy::kWarn;
  }

  // Private network requests from the `public` address space are blocked if the
  // right feature is enabled and the initiating context is not secure.
  //
  // NOTE: We also set this when `policies.ip_address_space` is `kLocal`, but
  // that has no effect. Indeed, requests initiated from the local address space
  // are never considered private network requests - they cannot target
  // more-private address spaces.
  return base::FeatureList::IsEnabled(
             features::kBlockInsecurePrivateNetworkRequests)
             ? network::mojom::PrivateNetworkRequestPolicy::kBlock
             : network::mojom::PrivateNetworkRequestPolicy::kWarn;
}

}  // namespace content
