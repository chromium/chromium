// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PRIVATE_NETWORK_ACCESS_CHECK_H_
#define SERVICES_NETWORK_PRIVATE_NETWORK_ACCESS_CHECK_H_

#include "base/component_export.h"
#include "services/network/public/cpp/private_network_access_check_result.h"
#include "services/network/public/mojom/ip_address_space.mojom-forward.h"

namespace network {

namespace mojom {

class ClientSecurityState;

}  // namespace mojom

// Returns whether a request client with the given `client_security_state`
// should be allowed to make requests to an endpoint in `resource_address_space`
// with a `URLLoader` configured with the given `url_load_options`.
//
// Implements the following spec:
// https://wicg.github.io/private-network-access/#private-network-access-check
PrivateNetworkAccessCheckResult COMPONENT_EXPORT(NETWORK_SERVICE)
    PrivateNetworkAccessCheck(
        const network::mojom::ClientSecurityState* client_security_state,
        int32_t url_load_options,
        mojom::IPAddressSpace resource_address_space);

}  // namespace network

#endif  // SERVICES_NETWORK_PRIVATE_NETWORK_ACCESS_CHECK_H_
