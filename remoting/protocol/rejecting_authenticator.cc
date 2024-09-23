// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/rejecting_authenticator.h"

#include "base/check_op.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "remoting/protocol/channel_authenticator.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting::protocol {

RejectingAuthenticator::RejectingAuthenticator(RejectionReason rejection_reason)
    : rejection_reason_(rejection_reason) {}

RejectingAuthenticator::~RejectingAuthenticator() = default;

CredentialsType RejectingAuthenticator::credentials_type() const {
  return CredentialsType::UNKNOWN;
}

const Authenticator& RejectingAuthenticator::implementing_authenticator()
    const {
  return *this;
}

Authenticator::State RejectingAuthenticator::state() const {
  return state_;
}
bool RejectingAuthenticator::started() const {
  return true;
}

Authenticator::RejectionReason RejectingAuthenticator::rejection_reason()
    const {
  DCHECK_EQ(state_, REJECTED);
  return rejection_reason_;
}

void RejectingAuthenticator::ProcessMessage(
    const jingle_xmpp::XmlElement* message,
    base::OnceClosure resume_callback) {
  DCHECK_EQ(state_, WAITING_MESSAGE);
  state_ = REJECTED;
  std::move(resume_callback).Run();
}

std::unique_ptr<jingle_xmpp::XmlElement>
RejectingAuthenticator::GetNextMessage() {
  NOTREACHED();
}

const std::string& RejectingAuthenticator::GetAuthKey() const {
  NOTREACHED();
}

const SessionPolicies* RejectingAuthenticator::GetSessionPolicies() const {
  NOTREACHED();
}

std::unique_ptr<ChannelAuthenticator>
RejectingAuthenticator::CreateChannelAuthenticator() const {
  NOTREACHED();
}

}  // namespace remoting::protocol
