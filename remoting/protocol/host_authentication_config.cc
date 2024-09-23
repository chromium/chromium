// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/host_authentication_config.h"

#include "base/check.h"
#include "remoting/base/name_value_map.h"

namespace remoting::protocol {

namespace {

const NameMapElement<HostAuthenticationConfig::Method>
    kAuthenticationMethodStrings[] = {
        {HostAuthenticationConfig::Method::SHARED_SECRET_SPAKE2_CURVE25519,
         "spake2_curve25519"},
        {HostAuthenticationConfig::Method::PAIRED_SPAKE2_CURVE25519,
         "pair_spake2_curve25519"},
        {HostAuthenticationConfig::Method::CORP_SESSION_AUTHZ_SPAKE2_CURVE25519,
         "corp_session_authz_spake2_curve25519"},
};

}  // namespace

// static
HostAuthenticationConfig::Method HostAuthenticationConfig::ParseMethodString(
    std::string_view value) {
  Method result;
  if (!NameToValue(kAuthenticationMethodStrings, value, &result)) {
    return Method::INVALID;
  }
  return result;
}

// static
std::string HostAuthenticationConfig::MethodToString(Method method) {
  return ValueToName(kAuthenticationMethodStrings, method);
}

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

std::vector<HostAuthenticationConfig::Method>
HostAuthenticationConfig::GetSupportedMethods() {
  std::vector<Method> methods;
  if (session_authz_client_factory) {
    methods.push_back(Method::CORP_SESSION_AUTHZ_SPAKE2_CURVE25519);
  }
  if (!shared_secret_hash.empty()) {
    // Pairing auth requires a non-empty shared secret hash.
    if (pairing_registry) {
      methods.push_back(Method::PAIRED_SPAKE2_CURVE25519);
    }
    methods.push_back(Method::SHARED_SECRET_SPAKE2_CURVE25519);
  }
  return methods;
}

}  // namespace remoting::protocol
