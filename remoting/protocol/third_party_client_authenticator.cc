// Copyright 2013 The Chromium Authors. All rights reserved.
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

namespace remoting {
namespace protocol {

ThirdPartyClientAuthenticator::ThirdPartyClientAuthenticator(
    const CreateBaseAuthenticatorCallback& create_base_authenticator_callback,
    const FetchThirdPartyTokenCallback& fetch_token_callback)
    : ThirdPartyAuthenticatorBase(WAITING_MESSAGE),
      create_base_authenticator_callback_(create_base_authenticator_callback),
      fetch_token_callback_(std::move(fetch_token_callback)) {}

ThirdPartyClientAuthenticator::~ThirdPartyClientAuthenticator() = default;

void ThirdPartyClientAuthenticator::ProcessTokenMessage(
    const jingle_xmpp::XmlElement* message,
    const base::Closure& resume_callback) {
  std::string token_url = message->TextNamed(kTokenUrlTag);
  std::string token_scope = message->TextNamed(kTokenScopeTag);

  if (token_url.empty() || token_scope.empty()) {
    LOG(ERROR) << "Third-party authentication protocol error: "
        "missing token verification URL or scope.";
    token_state_ = REJECTED;
    rejection_reason_ = PROTOCOL_ERROR;
    resume_callback.Run();
    return;
  }

  token_state_ = PROCESSING_MESSAGE;

  fetch_token_callback_.Run(
      token_url, token_scope,
      base::Bind(&ThirdPartyClientAuthenticator::OnThirdPartyTokenFetched,
                 weak_factory_.GetWeakPtr(), resume_callback));
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
    const base::Closure& resume_callback,
    const std::string& third_party_token,
    const std::string& shared_secret) {
  token_ = third_party_token;
  if (token_.empty() || shared_secret.empty()) {
    token_state_ = REJECTED;
    rejection_reason_ = INVALID_CREDENTIALS;
  } else {
    token_state_ = MESSAGE_READY;
    underlying_ =
        create_base_authenticator_callback_.Run(shared_secret, MESSAGE_READY);
  }
  resume_callback.Run();
}

}  // namespace protocol
}  // namespace remoting
