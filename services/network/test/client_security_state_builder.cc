// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/client_security_state_builder.h"

#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"

namespace network {

ClientSecurityStateBuilder&
ClientSecurityStateBuilder::WithPrivateNetworkRequestPolicy(
    network::mojom::PrivateNetworkRequestPolicy policy) {
  state_.private_network_request_policy = policy;
  return *this;
}

ClientSecurityStateBuilder& ClientSecurityStateBuilder::WithIPAddressSpace(
    network::mojom::IPAddressSpace space) {
  state_.ip_address_space = space;
  return *this;
}

ClientSecurityStateBuilder& ClientSecurityStateBuilder::WithIsSecureContext(
    bool is_secure_context) {
  state_.is_web_secure_context = is_secure_context;
  return *this;
}

ClientSecurityStateBuilder&
ClientSecurityStateBuilder::WithCrossOriginEmbedderPolicy(
    network::CrossOriginEmbedderPolicy policy) {
  state_.cross_origin_embedder_policy = policy;
  return *this;
}

network::mojom::ClientSecurityStatePtr ClientSecurityStateBuilder::Build()
    const {
  return state_.Clone();
}

}  // namespace network
