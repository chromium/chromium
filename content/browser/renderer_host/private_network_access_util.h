// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef CONTENT_BROWSER_RENDERER_HOST_PRIVATE_NETWORK_ACCESS_UTIL_H_
#define CONTENT_BROWSER_RENDERER_HOST_PRIVATE_NETWORK_ACCESS_UTIL_H_

#include "services/network/public/mojom/client_security_state.mojom-forward.h"

namespace content {

struct PolicyContainerPolicies;

// Returns the policy to use for private network requests, given `policies`.
//
// The policy depends on whether the initiating context is secure, its address
// space, as well as the state of various feature flags. It does not take
// Origin/Deprecation Trials into account.
network::mojom::PrivateNetworkRequestPolicy DerivePrivateNetworkRequestPolicy(
    const PolicyContainerPolicies& policies);

}  // namespace content

#endif  //  CONTENT_BROWSER_RENDERER_HOST_PRIVATE_NETWORK_ACCESS_UTIL_H_
