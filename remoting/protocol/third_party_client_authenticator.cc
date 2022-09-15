// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/third_party_client_authenticator.h"

#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "remoting/base/constants.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/protocol/channel_authenticator.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting::protocol {

ThirdPartyClientAuthenticator::ThirdPartyClientAuthenticator(
    const CreateBaseAuthenticatorCallback& create_base_authenticator_callback,
    const FetchThirdPartyTokenCallback& fetch_token_callback)
    : ThirdPartyAuthenticatorBase(WAITING_MESSAGE),
      create_base_authenticator_callback_(create_base_authenticator_callback),
      fetch_token_callback_(std::move(fetch_token_callback)) {}

ThirdPartyClientAuthenticator::~ThirdPartyClientAuthenticator() = default;

void ThirdPartyClientAuthenticator::ProcessTokenMessage(
    const jingle_xmpp::XmlElement* message,
    base::OnceClosure resume_callback) {
  std::string token_url = message->TextNamed(kTokenUrlTag);
  std::string token_scope = message->TextNamed(kTokenScopeTag);

  if (token_url.empty() || token_scope.empty()) {
    LOG(ERROR) << "Third-party authentication protocol error: "
        "missing token verification URL or scope.";
    token_state_ = REJECTED;
    rejection_reason_ = RejectionReason::PROTOCOL_ERROR;
    std::move(resume_callback).Run();
    return;
  }

  token_state_ = PROCESSING_MESSAGE;

  fetch_token_callback_.Run(
      token_url, token_scope,
      base::BindRepeating(
          &ThirdPartyClientAuthenticator::OnThirdPartyTokenFetched,
          weak_factory_.GetWeakPtr(),
          base::Passed(std::move(resume_callback))));
}

void ThirdPartyClientAuthenticator::AddTokenElements(
    jingle_xmpp::XmlElement* message) {
  DCHECK_EQ(token_state_, MESSAGE_READY);
  DCHECK(!token_.empty());

  jingle_xmpp::XmlElement* token_tag = new jingle_xmpp::XmlElement(kTokenTag);
  token_tag->SetBodyText(token_);
  message->AddElement(token_tag);
  token_state_ = ACCEPTED;
}

void ThirdPartyClientAuthenticator::OnThirdPartyTokenFetched(
    base::OnceClosure resume_callback,
    const std::string& third_party_token,
    const TokenValidator::ValidationResult& validation_result) {
  token_ = third_party_token;
  if (token_.empty() || validation_result.is_error()) {
    token_state_ = REJECTED;
    rejection_reason_ = validation_result.is_error()
                            ? validation_result.error()
                            : RejectionReason::INVALID_CREDENTIALS;
  } else {
    DCHECK(!validation_result.success().empty());
    token_state_ = MESSAGE_READY;
    underlying_ = create_base_authenticator_callback_.Run(
        validation_result.success(), MESSAGE_READY);
  }
  std::move(resume_callback).Run();
}

}  // namespace remoting::protocol
