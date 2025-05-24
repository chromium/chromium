// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/host_authentication_config.h"

#include "base/check.h"
#include "remoting/base/name_value_map.h"

namespace remoting::protocol {

HostAuthenticationConfig::HostAuthenticationConfig(
    std::string_view local_cert,
    scoped_refptr<RsaKeyPair> key_pair)
    : local_cert(local_cert), key_pair(key_pair) {}

HostAuthenticationConfig::~HostAuthenticationConfig() = default;
HostAuthenticationConfig::HostAuthenticationConfig(
    const HostAuthenticationConfig&) = default;
HostAuthenticationConfig::HostAuthenticationConfig(HostAuthenticationConfig&&) =
    default;

void HostAuthenticationConfig::AddSessionAuthzAuth(
    scoped_refptr<SessionAuthzServiceClientFactory> factory) {
  session_authz_client_factory = factory;
}

void HostAuthenticationConfig::AddPairingAuth(
    scoped_refptr<PairingRegistry> registry) {
  pairing_registry = registry;
}

void HostAuthenticationConfig::AddSharedSecretAuth(std::string_view hash) {
  shared_secret_hash = hash;
}

std::vector<AuthenticationMethod>
HostAuthenticationConfig::GetSupportedMethods() {
  std::vector<AuthenticationMethod> methods;
  if (session_authz_client_factory) {
    methods.push_back(session_authz_client_factory->method());
  }
  if (!shared_secret_hash.empty()) {
    // Pairing auth requires a non-empty shared secret hash.
    if (pairing_registry) {
      methods.push_back(AuthenticationMethod::PAIRED_SPAKE2_CURVE25519);
    }
    methods.push_back(AuthenticationMethod::SHARED_SECRET_SPAKE2_CURVE25519);
  }
  return methods;
}

}  // namespace remoting::protocol
