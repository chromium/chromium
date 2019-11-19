// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/me2me_host_authenticator_factory.h"

#include <utility>

#include "base/base64.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/protocol/channel_authenticator.h"
#include "remoting/protocol/negotiating_host_authenticator.h"
#include "remoting/protocol/rejecting_authenticator.h"
#include "remoting/protocol/token_validator.h"
#include "remoting/signaling/signaling_address.h"
#include "remoting/signaling/signaling_id_util.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting {
namespace protocol {

// static
std::unique_ptr<AuthenticatorFactory>
Me2MeHostAuthenticatorFactory::CreateWithPin(
    const std::string& host_owner,
    const std::string& local_cert,
    scoped_refptr<RsaKeyPair> key_pair,
    std::vector<std::string> required_client_domain_list,
    const std::string& pin_hash,
    scoped_refptr<PairingRegistry> pairing_registry) {
  std::unique_ptr<Me2MeHostAuthenticatorFactory> result(
      new Me2MeHostAuthenticatorFactory());
  result->canonical_host_owner_email_ = GetCanonicalEmail(host_owner);
  result->local_cert_ = local_cert;
  result->key_pair_ = key_pair;
  result->required_client_domain_list_ = std::move(required_client_domain_list);
  result->pin_hash_ = pin_hash;
  result->pairing_registry_ = pairing_registry;
  return std::move(result);
}

// static
std::unique_ptr<AuthenticatorFactory>
Me2MeHostAuthenticatorFactory::CreateWithThirdPartyAuth(
    const std::string& host_owner,
    const std::string& local_cert,
    scoped_refptr<RsaKeyPair> key_pair,
    std::vector<std::string> required_client_domain_list,
    scoped_refptr<TokenValidatorFactory> token_validator_factory) {
  std::unique_ptr<Me2MeHostAuthenticatorFactory> result(
      new Me2MeHostAuthenticatorFactory());
  result->canonical_host_owner_email_ = GetCanonicalEmail(host_owner);
  result->local_cert_ = local_cert;
  result->key_pair_ = key_pair;
  result->required_client_domain_list_ = std::move(required_client_domain_list);
  result->token_validator_factory_ = token_validator_factory;
  return std::move(result);
}

Me2MeHostAuthenticatorFactory::Me2MeHostAuthenticatorFactory() = default;

Me2MeHostAuthenticatorFactory::~Me2MeHostAuthenticatorFactory() = default;

std::unique_ptr<Authenticator>
Me2MeHostAuthenticatorFactory::CreateAuthenticator(
    const std::string& original_local_jid,
    const std::string& original_remote_jid) {
  std::string local_jid = NormalizeSignalingId(original_local_jid);
  std::string remote_jid = NormalizeSignalingId(original_remote_jid);

  // Verify that the client's jid is an ASCII string, and then check that the
  // client JID has the expected prefix. Comparison is case insensitive.
  if (!base::IsStringASCII(remote_jid) ||
      !base::StartsWith(remote_jid, canonical_host_owner_email_ + '/',
                        base::CompareCase::INSENSITIVE_ASCII)) {
    LOG(ERROR) << "Rejecting incoming connection from " << remote_jid
               << ": Prefix mismatch.  Expected: "
               << canonical_host_owner_email_;
    return base::WrapUnique(
        new RejectingAuthenticator(Authenticator::INVALID_CREDENTIALS));
  }

  // If necessary, verify that the client's jid belongs to the correct domain.
  if (!required_client_domain_list_.empty()) {
    std::string client_username = remote_jid;
    size_t pos = client_username.find('/');
    if (pos != std::string::npos) {
      client_username.replace(pos, std::string::npos, "");
    }
    bool matched = false;
    for (const std::string& domain : required_client_domain_list_) {
      if (base::EndsWith(client_username, std::string("@") + domain,
                         base::CompareCase::INSENSITIVE_ASCII)) {
        matched = true;
        break;
      }
    }
    if (!matched) {
      LOG(ERROR) << "Rejecting incoming connection from " << remote_jid
                 << ": Domain not allowed.";
      return std::make_unique<RejectingAuthenticator>(
          Authenticator::INVALID_ACCOUNT);
    }
  }

  if (!local_cert_.empty() && key_pair_.get()) {
    std::string normalized_local_jid = NormalizeSignalingId(local_jid);
    std::string normalized_remote_jid = NormalizeSignalingId(remote_jid);

    if (token_validator_factory_) {
      return NegotiatingHostAuthenticator::CreateWithThirdPartyAuth(
          normalized_local_jid, normalized_remote_jid, local_cert_, key_pair_,
          token_validator_factory_);
    }

    return NegotiatingHostAuthenticator::CreateWithSharedSecret(
        normalized_local_jid, normalized_remote_jid, local_cert_, key_pair_,
        pin_hash_, pairing_registry_);
  }

  return base::WrapUnique(
      new RejectingAuthenticator(Authenticator::INVALID_CREDENTIALS));
}

}  // namespace protocol
}  // namespace remoting
