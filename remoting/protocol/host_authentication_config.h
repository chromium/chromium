// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_HOST_AUTHENTICATION_CONFIG_H_
#define REMOTING_PROTOCOL_HOST_AUTHENTICATION_CONFIG_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "remoting/base/authentication_method.h"
#include "remoting/base/corp_session_authz_service_client_factory.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/base/session_authz_service_client_factory.h"
#include "remoting/protocol/pairing_registry.h"

namespace remoting::protocol {

// Configuration for host authentication. The list of supported methods will
// change based on the fields being set. Please see the comments on the fields.
struct HostAuthenticationConfig {
  HostAuthenticationConfig(std::string_view local_cert,
                           scoped_refptr<RsaKeyPair> key_pair);

  HostAuthenticationConfig(const HostAuthenticationConfig&);
  HostAuthenticationConfig(HostAuthenticationConfig&&);

  ~HostAuthenticationConfig();

  void AddSessionAuthzAuth(
      scoped_refptr<SessionAuthzServiceClientFactory> factory);

  // Note that pairing auth does not work without a shared secret hash, so
  // AddSharedSecretAuth() must also be called in order for
  // GetSupportedMethods() to return `PAIRED_...`.
  void AddPairingAuth(scoped_refptr<PairingRegistry> registry);

  void AddSharedSecretAuth(std::string_view hash);

  // Returns a list of supported methods based on the config. Note that the
  // order of the returned methods are NOT significant. Instead, the first
  // mutually supported method in the client's list of supported methods will be
  // used.
  std::vector<AuthenticationMethod> GetSupportedMethods();

  std::string local_cert;
  scoped_refptr<RsaKeyPair> key_pair;

  // Used for SessionAuthz authentication.
  scoped_refptr<SessionAuthzServiceClientFactory> session_authz_client_factory;

  // Used for pairing authentication.
  scoped_refptr<PairingRegistry> pairing_registry;

  // Used for shared secret authentication.
  std::string shared_secret_hash;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_HOST_AUTHENTICATION_CONFIG_H_
