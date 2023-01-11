// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/third_party_host_authenticator.h"

#include <utility>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/token_validator.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting::protocol {

ThirdPartyHostAuthenticator::ThirdPartyHostAuthenticator(
    const CreateBaseAuthenticatorCallback& create_base_authenticator_callback,
    std::unique_ptr<TokenValidator> token_validator)
    : ThirdPartyAuthenticatorBase(MESSAGE_READY),
      create_base_authenticator_callback_(create_base_authenticator_callback),
      token_validator_(std::move(token_validator)) {}

ThirdPartyHostAuthenticator::~ThirdPartyHostAuthenticator() = default;

void ThirdPartyHostAuthenticator::ProcessTokenMessage(
    const jingle_xmpp::XmlElement* message,
    base::OnceClosure resume_callback) {
  // Host has already sent the URL and expects a token from the client.
  std::string token = message->TextNamed(kTokenTag);
  if (token.empty()) {
    LOG(ERROR) << "Third-party authentication protocol error: missing token.";
    token_state_ = REJECTED;
    rejection_reason_ = RejectionReason::PROTOCOL_ERROR;
    std::move(resume_callback).Run();
    return;
  }

  token_state_ = PROCESSING_MESSAGE;

  // This message also contains the client's first SPAKE message. Copy the
  // message into the callback, so that OnThirdPartyTokenValidated can give it
  // to the underlying SPAKE authenticator that will be created.
  // |token_validator_| is owned, so Unretained() is safe here.
  token_validator_->ValidateThirdPartyToken(
      token,
      base::BindOnce(&ThirdPartyHostAuthenticator::OnThirdPartyTokenValidated,
                     base::Unretained(this),
                     base::Owned(new jingle_xmpp::XmlElement(*message)),
                     std::move(resume_callback)));
}

void ThirdPartyHostAuthenticator::AddTokenElements(
    jingle_xmpp::XmlElement* message) {
  DCHECK_EQ(token_state_, MESSAGE_READY);
  DCHECK(token_validator_->token_url().is_valid());
  DCHECK(!token_validator_->token_scope().empty());

  jingle_xmpp::XmlElement* token_url_tag =
      new jingle_xmpp::XmlElement(kTokenUrlTag);
  token_url_tag->SetBodyText(token_validator_->token_url().spec());
  message->AddElement(token_url_tag);
  jingle_xmpp::XmlElement* token_scope_tag =
      new jingle_xmpp::XmlElement(kTokenScopeTag);
  token_scope_tag->SetBodyText(token_validator_->token_scope());
  message->AddElement(token_scope_tag);
  token_state_ = WAITING_MESSAGE;
}

void ThirdPartyHostAuthenticator::OnThirdPartyTokenValidated(
    const jingle_xmpp::XmlElement* message,
    base::OnceClosure resume_callback,
    const TokenValidator::ValidationResult& validation_result) {
  if (validation_result.is_error()) {
    token_state_ = REJECTED;
    rejection_reason_ = validation_result.error();
    std::move(resume_callback).Run();
    return;
  }

  // The other side already started the SPAKE authentication.
  DCHECK(!validation_result.success().empty());
  token_state_ = ACCEPTED;
  underlying_ = create_base_authenticator_callback_.Run(
      validation_result.success(), WAITING_MESSAGE);
  underlying_->ProcessMessage(message, std::move(resume_callback));
}

}  // namespace remoting::protocol
