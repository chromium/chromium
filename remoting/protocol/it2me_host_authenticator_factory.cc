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
    const std::string& local_cert,
    scoped_refptr<RsaKeyPair> key_pair,
    const std::string& access_code_hash,
    const ValidatingAuthenticator::ValidationCallback& callback)
    : local_cert_(local_cert),
      key_pair_(key_pair),
      access_code_hash_(access_code_hash),
      validation_callback_(callback) {}

It2MeHostAuthenticatorFactory::~It2MeHostAuthenticatorFactory() = default;

std::unique_ptr<Authenticator>
It2MeHostAuthenticatorFactory::CreateAuthenticator(
    const std::string& local_jid,
    const std::string& remote_jid) {
  auto auth_config =
      std::make_unique<HostAuthenticationConfig>(local_cert_, key_pair_);
  auth_config->AddSharedSecretAuth(access_code_hash_);
  auto authenticator = std::make_unique<NegotiatingHostAuthenticator>(
      local_jid, remote_jid, std::move(auth_config));
  return std::make_unique<ValidatingAuthenticator>(
      remote_jid, std::move(validation_callback_), std::move(authenticator));
}

}  // namespace remoting::protocol
