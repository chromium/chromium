// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_THIRD_PARTY_HOST_AUTHENTICATOR_H_
#define REMOTING_PROTOCOL_THIRD_PARTY_HOST_AUTHENTICATOR_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "remoting/protocol/third_party_authenticator_base.h"
#include "remoting/protocol/token_validator.h"

namespace remoting::protocol {

// Implements the host side of the third party authentication mechanism.
// The host authenticator sends the |token_url| and |scope| obtained from the
// |TokenValidator| to the client, and expects a |token| in response.
// Once that token is received, it calls |TokenValidator| asynchronously to
// validate it, and exchange it for a |shared_secret|. Once the |TokenValidator|
// returns, the host uses the |shared_secret| to create an underlying
// SPAKE2 authenticator, which is used to establish the encrypted connection.
class ThirdPartyHostAuthenticator : public ThirdPartyAuthenticatorBase {
 public:
  // Creates a third-party host authenticator.
  // |create_base_authenticator_callback| is used to create the base
  // authenticator. |token_validator| contains the token parameters to be sent
  // to the client and is used to obtain the shared secret.
  ThirdPartyHostAuthenticator(
      const CreateBaseAuthenticatorCallback& create_base_authenticator_callback,
      std::unique_ptr<TokenValidator> token_validator);

  ThirdPartyHostAuthenticator(const ThirdPartyHostAuthenticator&) = delete;
  ThirdPartyHostAuthenticator& operator=(const ThirdPartyHostAuthenticator&) =
      delete;

  ~ThirdPartyHostAuthenticator() override;

 protected:
  // ThirdPartyAuthenticator implementation.
  void ProcessTokenMessage(const jingle_xmpp::XmlElement* message,
                           base::OnceClosure resume_callback) override;
  void AddTokenElements(jingle_xmpp::XmlElement* message) override;

 private:
  void OnThirdPartyTokenValidated(
      const jingle_xmpp::XmlElement* message,
      base::OnceClosure resume_callback,
      const TokenValidator::ValidationResult& validation_result);

  CreateBaseAuthenticatorCallback create_base_authenticator_callback_;
  std::unique_ptr<TokenValidator> token_validator_;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_THIRD_PARTY_HOST_AUTHENTICATOR_H_
