// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/it2me_host_authenticator_factory.h"

#include <memory>
#include <string>
#include <utility>

#include "remoting/base/rsa_key_pair.h"
#include "remoting/protocol/host_authentication_config.h"
#include "remoting/protocol/negotiating_host_authenticator.h"
#include "remoting/protocol/validating_authenticator.h"

namespace remoting::protocol {

It2MeHostAuthenticatorFactory::It2MeHostAuthenticatorFactory(
    std::string_view local_cert,
    scoped_refptr<RsaKeyPair> key_pair,
    const ValidatingAuthenticator::ValidationCallback& callback)
    : auth_config_(local_cert, key_pair), validation_callback_(callback) {}

It2MeHostAuthenticatorFactory::~It2MeHostAuthenticatorFactory() = default;

void It2MeHostAuthenticatorFactory::AddSharedSecretAuth(
    std::string_view access_code_hash) {
  auth_config_.AddSharedSecretAuth(access_code_hash);
}

void It2MeHostAuthenticatorFactory::AddSessionAuthzAuth(
    scoped_refptr<SessionAuthzServiceClientFactory> factory) {
  auth_config_.AddSessionAuthzAuth(factory);
}

std::unique_ptr<Authenticator>
It2MeHostAuthenticatorFactory::CreateAuthenticator(
    const std::string& local_jid,
    const std::string& remote_jid) {
  auto authenticator = std::make_unique<NegotiatingHostAuthenticator>(
      local_jid, remote_jid,
      std::make_unique<HostAuthenticationConfig>(auth_config_));
  return std::make_unique<ValidatingAuthenticator>(
      remote_jid, std::move(validation_callback_), std::move(authenticator));
}

}  // namespace remoting::protocol
