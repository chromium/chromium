// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/validating_authenticator.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/channel_authenticator.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting {
namespace protocol {

ValidatingAuthenticator::ValidatingAuthenticator(
    const std::string& remote_jid,
    const ValidationCallback& validation_callback,
    std::unique_ptr<Authenticator> current_authenticator)
    : remote_jid_(remote_jid),
      validation_callback_(validation_callback),
      current_authenticator_(std::move(current_authenticator)) {
  DCHECK(!remote_jid_.empty());
  DCHECK(validation_callback_);
  DCHECK(current_authenticator_);
}

ValidatingAuthenticator::~ValidatingAuthenticator() = default;

Authenticator::State ValidatingAuthenticator::state() const {
  return pending_auth_message_ ? MESSAGE_READY : state_;
}

bool ValidatingAuthenticator::started() const {
  return current_authenticator_->started();
}

Authenticator::RejectionReason ValidatingAuthenticator::rejection_reason()
    const {
  return rejection_reason_;
}

const std::string& ValidatingAuthenticator::GetAuthKey() const {
  return current_authenticator_->GetAuthKey();
}

std::unique_ptr<ChannelAuthenticator>
ValidatingAuthenticator::CreateChannelAuthenticator() const {
  return current_authenticator_->CreateChannelAuthenticator();
}

void ValidatingAuthenticator::ProcessMessage(
    const jingle_xmpp::XmlElement* message,
    const base::Closure& resume_callback) {
  DCHECK_EQ(state_, WAITING_MESSAGE);
  state_ = PROCESSING_MESSAGE;

  current_authenticator_->ProcessMessage(
      message, base::Bind(&ValidatingAuthenticator::UpdateState,
                          weak_factory_.GetWeakPtr(), resume_callback));
}

std::unique_ptr<jingle_xmpp::XmlElement> ValidatingAuthenticator::GetNextMessage() {
  if (pending_auth_message_) {
    DCHECK(state_ == ACCEPTED || state_ == WAITING_MESSAGE);
    return std::move(pending_auth_message_);
  }

  std::unique_ptr<jingle_xmpp::XmlElement> result(
      current_authenticator_->GetNextMessage());
  state_ = current_authenticator_->state();
  DCHECK(state_ == ACCEPTED || state_ == WAITING_MESSAGE);

  return result;
}

void ValidatingAuthenticator::OnValidateComplete(const base::Closure& callback,
                                                 Result validation_result) {
  // Map |rejection_reason_| to a known reason, set |state_| to REJECTED and
  // notify the listener of the connection error via the callback.
  switch (validation_result) {
    case Result::SUCCESS:
      state_ = ACCEPTED;
      callback.Run();
      return;

    case Result::ERROR_INVALID_CREDENTIALS:
      rejection_reason_ = Authenticator::INVALID_CREDENTIALS;
      break;

    case Result::ERROR_INVALID_ACCOUNT:
      rejection_reason_ = Authenticator::INVALID_ACCOUNT;
      break;

    case Result::ERROR_TOO_MANY_CONNECTIONS:
      rejection_reason_ = Authenticator::TOO_MANY_CONNECTIONS;
      break;

    case Result::ERROR_REJECTED_BY_USER:
      rejection_reason_ = Authenticator::REJECTED_BY_USER;
      break;
  }

  state_ = Authenticator::REJECTED;
  callback.Run();
}

void ValidatingAuthenticator::UpdateState(
    const base::Closure& resume_callback) {
  DCHECK_EQ(state_, PROCESSING_MESSAGE);

  // Update our current state before running |resume_callback|.
  state_ = current_authenticator_->state();
  if (state_ == REJECTED) {
    rejection_reason_ = current_authenticator_->rejection_reason();
  } else if (state_ == MESSAGE_READY) {
    DCHECK(!pending_auth_message_);
    pending_auth_message_ = current_authenticator_->GetNextMessage();
    state_ = current_authenticator_->state();
  }

  if (state_ == ACCEPTED) {
    state_ = PROCESSING_MESSAGE;
    validation_callback_.Run(
        remote_jid_, base::Bind(&ValidatingAuthenticator::OnValidateComplete,
                                weak_factory_.GetWeakPtr(), resume_callback));
  } else {
    resume_callback.Run();
  }
}

}  // namespace protocol
}  // namespace remoting
