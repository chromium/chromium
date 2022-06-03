// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/third_party_authenticator_base.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/check_op.h"
#include "remoting/base/constants.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/protocol/channel_authenticator.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting {
namespace protocol {

// static
const jingle_xmpp::StaticQName ThirdPartyAuthenticatorBase::kTokenUrlTag =
    { remoting::kChromotingXmlNamespace, "third-party-token-url" };
const jingle_xmpp::StaticQName ThirdPartyAuthenticatorBase::kTokenScopeTag =
    { remoting::kChromotingXmlNamespace, "third-party-token-scope" };
const jingle_xmpp::StaticQName ThirdPartyAuthenticatorBase::kTokenTag =
    { remoting::kChromotingXmlNamespace, "third-party-token" };

ThirdPartyAuthenticatorBase::ThirdPartyAuthenticatorBase(
    Authenticator::State initial_state)
    : token_state_(initial_state),
      started_(false),
      rejection_reason_(INVALID_CREDENTIALS) {
}

ThirdPartyAuthenticatorBase::~ThirdPartyAuthenticatorBase() = default;

bool ThirdPartyAuthenticatorBase::started() const {
  return started_;
}

Authenticator::State ThirdPartyAuthenticatorBase::state() const {
  if (token_state_ == ACCEPTED)
    return underlying_->state();
  return token_state_;
}

Authenticator::RejectionReason
ThirdPartyAuthenticatorBase::rejection_reason() const {
  DCHECK_EQ(state(), REJECTED);

  if (token_state_ == REJECTED)
    return rejection_reason_;
  return underlying_->rejection_reason();
}

void ThirdPartyAuthenticatorBase::ProcessMessage(
    const jingle_xmpp::XmlElement* message,
    base::OnceClosure resume_callback) {
  DCHECK_EQ(state(), WAITING_MESSAGE);

  if (token_state_ == WAITING_MESSAGE) {
    ProcessTokenMessage(message, std::move(resume_callback));
  } else {
    DCHECK_EQ(token_state_, ACCEPTED);
    DCHECK(underlying_);
    DCHECK_EQ(underlying_->state(), WAITING_MESSAGE);
    underlying_->ProcessMessage(message, std::move(resume_callback));
  }
}

std::unique_ptr<jingle_xmpp::XmlElement>
ThirdPartyAuthenticatorBase::GetNextMessage() {
  DCHECK_EQ(state(), MESSAGE_READY);

  std::unique_ptr<jingle_xmpp::XmlElement> message;
  if (underlying_ && underlying_->state() == MESSAGE_READY) {
    message = underlying_->GetNextMessage();
  } else {
    message = CreateEmptyAuthenticatorMessage();
  }

  if (token_state_ == MESSAGE_READY) {
    AddTokenElements(message.get());
    started_ = true;
  }
  return message;
}

const std::string& ThirdPartyAuthenticatorBase::GetAuthKey() const {
  DCHECK_EQ(state(), ACCEPTED);

  return underlying_->GetAuthKey();
}

std::unique_ptr<ChannelAuthenticator>
ThirdPartyAuthenticatorBase::CreateChannelAuthenticator() const {
  DCHECK_EQ(state(), ACCEPTED);

  return underlying_->CreateChannelAuthenticator();
}

}  // namespace protocol
}  // namespace remoting
