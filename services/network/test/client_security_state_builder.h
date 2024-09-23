// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TEST_CLIENT_SECURITY_STATE_BUILDER_H_
#define SERVICES_NETWORK_TEST_CLIENT_SECURITY_STATE_BUILDER_H_

#include "services/network/public/mojom/client_security_state.mojom-forward.h"
#include "services/network/public/mojom/client_security_state.mojom.h"

namespace network {

namespace mojom {
enum class PrivateNetworkRequestPolicy;
enum class IPAddressSpace;
}  // namespace mojom

struct CrossOriginEmbedderPolicy;

class ClientSecurityStateBuilder {
 public:
  ClientSecurityStateBuilder() = default;
  ~ClientSecurityStateBuilder() = default;

  ClientSecurityStateBuilder& WithPrivateNetworkRequestPolicy(
      network::mojom::PrivateNetworkRequestPolicy policy);

  ClientSecurityStateBuilder& WithIPAddressSpace(
      network::mojom::IPAddressSpace space);

  ClientSecurityStateBuilder& WithIsSecureContext(bool is_secure_context);

  ClientSecurityStateBuilder& WithCrossOriginEmbedderPolicy(
      network::CrossOriginEmbedderPolicy policy);

  network::mojom::ClientSecurityStatePtr Build() const;

 private:
  network::mojom::ClientSecurityState state_;
};
}  // namespace network

#endif  // SERVICES_NETWORK_TEST_CLIENT_SECURITY_STATE_BUILDER_H_
