// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_NEGOTIATING_HOST_AUTHENTICATOR_H_
#define REMOTING_PROTOCOL_NEGOTIATING_HOST_AUTHENTICATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/negotiating_authenticator_base.h"
#include "remoting/protocol/pairing_registry.h"
#include "remoting/protocol/third_party_host_authenticator.h"

namespace remoting {

class RsaKeyPair;

namespace protocol {

class TokenValidatorFactory;

// Host-side implementation of NegotiatingAuthenticatorBase.
// See comments in negotiating_authenticator_base.h for a general explanation.
class NegotiatingHostAuthenticator : public NegotiatingAuthenticatorBase {
 public:
  NegotiatingHostAuthenticator(const NegotiatingHostAuthenticator&) = delete;
  NegotiatingHostAuthenticator& operator=(const NegotiatingHostAuthenticator&) =
      delete;

  ~NegotiatingHostAuthenticator() override;

  // Creates a host authenticator, using a PIN or access code. If
  // |pairing_registry| is non-nullptr then the paired methods will be offered,
  // supporting PIN-less authentication.
  static std::unique_ptr<NegotiatingHostAuthenticator> CreateWithSharedSecret(
      const std::string& local_id,
      const std::string& remote_id,
      const std::string& local_cert,
      scoped_refptr<RsaKeyPair> key_pair,
      const std::string& pin_hash,
      scoped_refptr<PairingRegistry> pairing_registry);

  // Creates a host authenticator, using third party authentication.
  static std::unique_ptr<NegotiatingHostAuthenticator> CreateWithThirdPartyAuth(
      const std::string& local_id,
      const std::string& remote_id,
      const std::string& local_cert,
      scoped_refptr<RsaKeyPair> key_pair,
      scoped_refptr<TokenValidatorFactory> token_validator_factory);

  // NegotiatingAuthenticatorBase:
  void ProcessMessage(const jingle_xmpp::XmlElement* message,
                      base::OnceClosure resume_callback) override;
  std::unique_ptr<jingle_xmpp::XmlElement> GetNextMessage() override;

 private:
  NegotiatingHostAuthenticator(const std::string& local_id,
                               const std::string& remote_id,
                               const std::string& local_cert,
                               scoped_refptr<RsaKeyPair> key_pair);

  // (Asynchronously) creates an authenticator, and stores it in
  // |current_authenticator_|. Authenticators that can be started in either
  // state will be created in |preferred_initial_state|.
  void CreateAuthenticator(Authenticator::State preferred_initial_state,
                           base::OnceClosure resume_callback);

  std::string local_id_;
  std::string remote_id_;

  std::string local_cert_;
  scoped_refptr<RsaKeyPair> local_key_pair_;

  // Used only for shared secret host authenticators.
  std::string shared_secret_hash_;

  // Used only for third party host authenticators.
  scoped_refptr<TokenValidatorFactory> token_validator_factory_;

  // Used only for pairing authenticators.
  scoped_refptr<PairingRegistry> pairing_registry_;

  std::string client_id_;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_NEGOTIATING_HOST_AUTHENTICATOR_H_
