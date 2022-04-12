// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PRIVATE_NETWORK_ACCESS_CHECK_H_
#define SERVICES_NETWORK_PRIVATE_NETWORK_ACCESS_CHECK_H_

#include "base/component_export.h"
#include "services/network/public/cpp/private_network_access_check_result.h"
#include "services/network/public/mojom/ip_address_space.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {

namespace mojom {

class ClientSecurityState;

}  // namespace mojom

// Returns whether a request client with the given `client_security_state`,
// expecting a resource in `target_address_space`, having observed a previous
// part of the response coming from `previous_response_address_space`, should be
// allowed to make requests to an endpoint in `resource_address_space` with a
// `URLLoader` configured with the given `url_load_options`.
//
// `target_address_space` is ignored if set to `kUnknown`.
//
// Implements the following spec:
// https://wicg.github.io/private-network-access/#private-network-access-check
PrivateNetworkAccessCheckResult COMPONENT_EXPORT(NETWORK_SERVICE)
    PrivateNetworkAccessCheck(
        const mojom::ClientSecurityState* client_security_state,
        mojom::IPAddressSpace target_address_space,
        absl::optional<mojom::IPAddressSpace> previous_response_address_space,
        int32_t url_load_options,
        mojom::IPAddressSpace resource_address_space);

}  // namespace network

#endif  // SERVICES_NETWORK_PRIVATE_NETWORK_ACCESS_CHECK_H_
