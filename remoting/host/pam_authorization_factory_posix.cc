// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/pam_authorization_factory_posix.h"

#include <security/pam_appl.h>

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/environment.h"
#include "remoting/base/logging.h"
#include "remoting/host/username.h"
#include "remoting/protocol/channel_authenticator.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting {

namespace {
class PamAuthorizer : public protocol::Authenticator {
 public:
  explicit PamAuthorizer(std::unique_ptr<protocol::Authenticator> underlying);
  ~PamAuthorizer() override;

  // protocol::Authenticator:
  State state() const override;
  bool started() const override;
  RejectionReason rejection_reason() const override;
  void ProcessMessage(const jingle_xmpp::XmlElement* message,
                      base::OnceClosure resume_callback) override;
  std::unique_ptr<jingle_xmpp::XmlElement> GetNextMessage() override;
  const std::string& GetAuthKey() const override;
  std::unique_ptr<protocol::ChannelAuthenticator> CreateChannelAuthenticator()
      const override;

 private:
  void MaybeCheckLocalLogin();
  bool IsLocalLoginAllowed();
  void OnMessageProcessed(base::OnceClosure resume_callback);

  static int PamConversation(int num_messages,
                             const struct pam_message** messages,
                             struct pam_response** responses,
                             void* context);

  std::unique_ptr<protocol::Authenticator> underlying_;
  enum { NOT_CHECKED, ALLOWED, DISALLOWED } local_login_status_;
};

}  // namespace

PamAuthorizer::PamAuthorizer(
    std::unique_ptr<protocol::Authenticator> underlying)
    : underlying_(std::move(underlying)), local_login_status_(NOT_CHECKED) {}

PamAuthorizer::~PamAuthorizer() {}

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

protocol::Authenticator::RejectionReason
PamAuthorizer::rejection_reason() const {
  if (local_login_status_ == DISALLOWED) {
    return INVALID_CREDENTIALS;
  } else {
    return underlying_->rejection_reason();
  }
}

void PamAuthorizer::ProcessMessage(const jingle_xmpp::XmlElement* message,
                                   base::OnceClosure resume_callback) {
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

std::unique_ptr<jingle_xmpp::XmlElement> PamAuthorizer::GetNextMessage() {
  std::unique_ptr<jingle_xmpp::XmlElement> result(underlying_->GetNextMessage());
  MaybeCheckLocalLogin();
  return result;
}

const std::string& PamAuthorizer::GetAuthKey() const {
  return underlying_->GetAuthKey();
}

std::unique_ptr<protocol::ChannelAuthenticator>
PamAuthorizer::CreateChannelAuthenticator() const {
  return underlying_->CreateChannelAuthenticator();
}

void PamAuthorizer::MaybeCheckLocalLogin() {
  if (local_login_status_ == NOT_CHECKED && state() == ACCEPTED) {
    local_login_status_ = IsLocalLoginAllowed() ? ALLOWED : DISALLOWED;
  }
}

bool PamAuthorizer::IsLocalLoginAllowed() {
  std::string username = GetUsername();
  if (username.empty()) {
    return false;
  }
  struct pam_conv conv = { PamConversation, nullptr };
  pam_handle_t* handle = nullptr;
  int result = pam_start("chrome-remote-desktop", username.c_str(),
                         &conv, &handle);
  if (result == PAM_SUCCESS) {
    result = pam_acct_mgmt(handle, 0);
  }
  pam_end(handle, result);

  HOST_LOG << "Local login check for " << username
            << (result == PAM_SUCCESS ? " succeeded." : " failed.");

  return result == PAM_SUCCESS;
}

int PamAuthorizer::PamConversation(int num_messages,
                                   const struct pam_message** messages,
                                   struct pam_response** responses,
                                   void* context) {
  // Assume we're only being asked to log messages, in which case our response
  // need to be free()-able zero-initialized memory.
  *responses = static_cast<struct pam_response*>(
      calloc(num_messages, sizeof(struct pam_response)));

  // We don't expect this function to be called. Since we have no easy way
  // of returning a response, we consider it to be an error if we're asked
  // for one and abort. Informational and error messages are logged.
  for (int i = 0; i < num_messages; ++i) {
    const struct pam_message* message = messages[i];
    switch (message->msg_style) {
      case PAM_ERROR_MSG:
        LOG(ERROR) << "PAM conversation error message: " << message->msg;
        break;
      case PAM_TEXT_INFO:
        HOST_LOG << "PAM conversation message: " << message->msg;
        break;
      default:
        LOG(FATAL) << "Unexpected PAM conversation response required: "
                   << message->msg << "; msg_style = " << message->msg_style;
    }
  }
  return PAM_SUCCESS;
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

}  // namespace remoting
