// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/pam_authorization_factory_posix.h"

#include <security/pam_appl.h>

#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/environment.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "remoting/base/logging.h"
#include "remoting/base/username.h"
#include "remoting/host/pam_utils.h"
#include "remoting/protocol/channel_authenticator.h"

namespace remoting {

namespace {
class PamAuthorizer : public protocol::Authenticator {
 public:
  explicit PamAuthorizer(std::unique_ptr<protocol::Authenticator> underlying);
  ~PamAuthorizer() override;

  // protocol::Authenticator:
  protocol::CredentialsType credentials_type() const override;
  const Authenticator& implementing_authenticator() const override;
  State state() const override;
  bool started() const override;
  RejectionReason rejection_reason() const override;
  RejectionDetails rejection_details() const override;
  void ProcessMessage(const JingleAuthentication& message,
                      base::OnceClosure resume_callback) override;
  JingleAuthentication GetNextMessage() override;
  const std::string& GetAuthKey() const override;
  const SessionPolicies* GetSessionPolicies() const override;
  std::unique_ptr<protocol::ChannelAuthenticator> CreateChannelAuthenticator()
      const override;

 private:
  void MaybeCheckLocalLogin();
  void OnMessageProcessed(base::OnceClosure resume_callback);

  std::unique_ptr<protocol::Authenticator> underlying_;
  enum { NOT_CHECKED, ALLOWED, DISALLOWED } local_login_status_;
};

}  // namespace

PamAuthorizer::PamAuthorizer(
    std::unique_ptr<protocol::Authenticator> underlying)
    : underlying_(std::move(underlying)), local_login_status_(NOT_CHECKED) {
  ChainStateChangeAfterAcceptedWithUnderlying(*underlying_);
}

PamAuthorizer::~PamAuthorizer() {}

protocol::CredentialsType PamAuthorizer::credentials_type() const {
  return underlying_->credentials_type();
}

const protocol::Authenticator& PamAuthorizer::implementing_authenticator()
    const {
  return underlying_->implementing_authenticator();
}

protocol::Authenticator::State PamAuthorizer::state() const {
  if (local_login_status_ == DISALLOWED) {
    return REJECTED;
  } else {
    return underlying_->state();
  }
}

bool PamAuthorizer::started() const {
  return underlying_->started();
}

protocol::Authenticator::RejectionReason PamAuthorizer::rejection_reason()
    const {
  if (local_login_status_ == DISALLOWED) {
    return RejectionReason::INVALID_CREDENTIALS;
  } else {
    return underlying_->rejection_reason();
  }
}

protocol::Authenticator::RejectionDetails PamAuthorizer::rejection_details()
    const {
  if (local_login_status_ == DISALLOWED) {
    return RejectionDetails("Local login check failed.");
  }
  return underlying_->rejection_details();
}

void PamAuthorizer::ProcessMessage(const JingleAuthentication& message,
                                   base::OnceClosure resume_callback) {
  // Always delegate to the underlying authenticator and let it manage its own
  // state machine.
  // |underlying_| is owned, so Unretained() is safe here.
  underlying_->ProcessMessage(
      message,
      base::BindOnce(&PamAuthorizer::OnMessageProcessed, base::Unretained(this),
                     std::move(resume_callback)));
}

void PamAuthorizer::OnMessageProcessed(base::OnceClosure resume_callback) {
  MaybeCheckLocalLogin();
  std::move(resume_callback).Run();
}

JingleAuthentication PamAuthorizer::GetNextMessage() {
  JingleAuthentication result = underlying_->GetNextMessage();
  // PAM check may be performed once the state has transitioned to ACCEPTED.
  MaybeCheckLocalLogin();
  return result;
}

const std::string& PamAuthorizer::GetAuthKey() const {
  return underlying_->GetAuthKey();
}

const SessionPolicies* PamAuthorizer::GetSessionPolicies() const {
  return underlying_->GetSessionPolicies();
}

std::unique_ptr<protocol::ChannelAuthenticator>
PamAuthorizer::CreateChannelAuthenticator() const {
  return underlying_->CreateChannelAuthenticator();
}

void PamAuthorizer::MaybeCheckLocalLogin() {
  if (local_login_status_ == NOT_CHECKED && state() == ACCEPTED) {
    std::string username = GetUsername();
    if (username.empty()) {
      LOG(ERROR) << "Failed to get username.";
      local_login_status_ = DISALLOWED;
      return;
    }
    local_login_status_ = IsLocalLoginAllowed(username) ? ALLOWED : DISALLOWED;
  }
}

PamAuthorizationFactory::PamAuthorizationFactory(
    std::unique_ptr<protocol::AuthenticatorFactory> underlying)
    : underlying_(std::move(underlying)) {}

PamAuthorizationFactory::~PamAuthorizationFactory() {}

std::unique_ptr<protocol::Authenticator>
PamAuthorizationFactory::CreateAuthenticator(const std::string& local_jid,
                                             const std::string& remote_jid) {
  std::unique_ptr<protocol::Authenticator> authenticator(
      underlying_->CreateAuthenticator(local_jid, remote_jid));
  return std::make_unique<PamAuthorizer>(std::move(authenticator));
}

std::unique_ptr<protocol::AuthenticatorFactory> PamAuthorizationFactory::Clone()
    const {
  return std::make_unique<PamAuthorizationFactory>(underlying_->Clone());
}

}  // namespace remoting
