// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/negotiating_authenticator_base.h"

#include <algorithm>
#include <sstream>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_split.h"
#include "remoting/base/constants.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/channel_authenticator.h"
#include "remoting/protocol/credentials_type.h"

namespace remoting::protocol {

const char NegotiatingAuthenticatorBase::kSupportedMethodsSeparator = ',';

NegotiatingAuthenticatorBase::NegotiatingAuthenticatorBase(
    Authenticator::State initial_state)
    : state_(initial_state) {}

NegotiatingAuthenticatorBase::~NegotiatingAuthenticatorBase() = default;

CredentialsType NegotiatingAuthenticatorBase::credentials_type() const {
  if (!current_authenticator_) {
    return CredentialsType::UNKNOWN;
  }
  return current_authenticator_->credentials_type();
}

const Authenticator& NegotiatingAuthenticatorBase::implementing_authenticator()
    const {
  return current_authenticator_
             ? current_authenticator_->implementing_authenticator()
             : *this;
}

Authenticator::State NegotiatingAuthenticatorBase::state() const {
  return state_;
}

bool NegotiatingAuthenticatorBase::started() const {
  if (!current_authenticator_) {
    return false;
  }
  return current_authenticator_->started();
}

Authenticator::RejectionReason NegotiatingAuthenticatorBase::rejection_reason()
    const {
  return rejection_reason_;
}

Authenticator::RejectionDetails
NegotiatingAuthenticatorBase::rejection_details() const {
  return rejection_details_;
}

void NegotiatingAuthenticatorBase::ProcessMessageInternal(
    const JingleAuthentication& message,
    base::OnceClosure resume_callback) {
  DCHECK_EQ(state_, PROCESSING_MESSAGE);

  if (current_authenticator_->state() == WAITING_MESSAGE) {
    // If the message was not discarded and the authenticator is waiting for it,
    // give it to the underlying authenticator to process.
    // |current_authenticator_| is owned, so Unretained() is safe here.
    current_authenticator_->ProcessMessage(
        message,
        base::BindOnce(&NegotiatingAuthenticatorBase::UpdateState,
                       base::Unretained(this), std::move(resume_callback)));
  } else {
    // Otherwise, just discard the message.
    UpdateState(std::move(resume_callback));
  }
}

void NegotiatingAuthenticatorBase::UpdateState(
    base::OnceClosure resume_callback) {
  DCHECK_EQ(state_, PROCESSING_MESSAGE);

  // After the underlying authenticator finishes processing the message, the
  // NegotiatingAuthenticatorBase must update its own state before running the
  // |resume_callback| to resume the session negotiation.
  state_ = current_authenticator_->state();

  // Verify that this is a valid state transition.
  DCHECK(state_ == MESSAGE_READY || state_ == ACCEPTED || state_ == REJECTED)
      << "State: " << state_;

  if (state_ == REJECTED) {
    rejection_reason_ = current_authenticator_->rejection_reason();
    rejection_details_ = current_authenticator_->rejection_details();
  }

  std::move(resume_callback).Run();
}

JingleAuthentication NegotiatingAuthenticatorBase::GetNextMessageInternal() {
  DCHECK_EQ(state(), MESSAGE_READY);
  DCHECK(current_method_ != AuthenticationMethod::INVALID);

  JingleAuthentication result;
  if (current_authenticator_->state() == MESSAGE_READY) {
    result = current_authenticator_->GetNextMessage();
  }
  state_ = current_authenticator_->state();
  // |state_| may be MESSAGE_READY if the underlying authenticator has
  // multiple messages to send.
  DCHECK(state_ == ACCEPTED || state_ == WAITING_MESSAGE);
  if (!result.is_empty()) {
    result.method = current_method_;
  }
  return result;
}

void NegotiatingAuthenticatorBase::NotifyStateChangeAfterAccepted() {
  state_ = current_authenticator_->state();
  if (state_ == REJECTED) {
    rejection_reason_ = current_authenticator_->rejection_reason();
    rejection_details_ = current_authenticator_->rejection_details();
  }
  Authenticator::NotifyStateChangeAfterAccepted();
}

void NegotiatingAuthenticatorBase::AddMethod(AuthenticationMethod method) {
  DCHECK(method != AuthenticationMethod::INVALID);
  methods_.push_back(method);
}

const std::string& NegotiatingAuthenticatorBase::GetAuthKey() const {
  DCHECK_EQ(state(), ACCEPTED);
  return current_authenticator_->GetAuthKey();
}

const SessionPolicies* NegotiatingAuthenticatorBase::GetSessionPolicies()
    const {
  DCHECK_EQ(state(), ACCEPTED);
  return current_authenticator_->GetSessionPolicies();
}

std::unique_ptr<ChannelAuthenticator>
NegotiatingAuthenticatorBase::CreateChannelAuthenticator() const {
  DCHECK_EQ(state(), ACCEPTED);
  return current_authenticator_->CreateChannelAuthenticator();
}

}  // namespace remoting::protocol
