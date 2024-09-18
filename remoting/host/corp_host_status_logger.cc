// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/corp_host_status_logger.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "remoting/base/corp_auth_util.h"
#include "remoting/base/corp_logging_service_client.h"
#include "remoting/base/logging.h"
#include "remoting/base/protobuf_http_status.h"
#include "remoting/base/session_policies.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/credentials_type.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/session_authz_authenticator.h"
#include "remoting/protocol/session_authz_reauthorizer.h"
#include "remoting/protocol/session_manager.h"

namespace remoting {

CorpHostStatusLogger::CorpHostStatusLogger(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const LocalSessionPoliciesProvider* local_session_policies_provider,
    const std::string& service_account_email,
    const std::string& refresh_token)
    : CorpHostStatusLogger(std::make_unique<CorpLoggingServiceClient>(
                               url_loader_factory,
                               CreateCorpTokenGetter(url_loader_factory,
                                                     service_account_email,
                                                     refresh_token)),
                           local_session_policies_provider) {}

CorpHostStatusLogger::CorpHostStatusLogger(
    std::unique_ptr<LoggingServiceClient> service_client,
    const LocalSessionPoliciesProvider* local_session_policies_provider)
    : service_client_(std::move(service_client)),
      local_session_policies_provider_(local_session_policies_provider) {}

CorpHostStatusLogger::~CorpHostStatusLogger() = default;

void CorpHostStatusLogger::StartObserving(
    protocol::SessionManager& session_manager) {
  observer_subscription_ = session_manager.AddSessionObserver(this);
}

void CorpHostStatusLogger::OnSessionStateChange(
    const protocol::Session& session,
    protocol::Session::State state) {
  if (state != protocol::Session::State::CLOSED &&
      state != protocol::Session::State::FAILED) {
    return;
  }
  if (session.authenticator().credentials_type() !=
      protocol::CredentialsType::CORP_SESSION_AUTHZ) {
    VLOG(1) << "Current session is not authenticated with SessionAuthz. "
            << "Disconnect event not logged.";
    return;
  }
  const auto& authenticator =
      static_cast<const protocol::SessionAuthzAuthenticator&>(
          session.authenticator().implementing_authenticator());
  const std::string& session_id = authenticator.session_id();
  if (session_id.empty()) {
    LOG(WARNING) << "SessionAuthz ID is not known. Disconnect event not "
                 << "logged.";
    return;
  }
  internal::ReportSessionDisconnectedRequestStruct request{
      .session_authz_id = session_id,
      .session_authz_reauth_token =
          authenticator.reauthorizer()
              ? authenticator.reauthorizer()->session_reauth_token()
              : "",
      .error_code = session.error(),
  };
  // The effective session policies are technically held by ClientSession, but
  // it's difficult to plumb it through multiple layers of abstraction, so we
  // just figure out the effective session policies ourselves here.
  // If the authenticator state is not `ACCEPTED`, then we don't know whether
  // the effective policies come from SessionAuthz or the local store, so we
  // keep it unset.
  if (session.authenticator().state() == protocol::Authenticator::ACCEPTED) {
    const SessionPolicies* session_policies_from_authenticator =
        session.authenticator().GetSessionPolicies();
    request.effective_session_policies =
        session_policies_from_authenticator
            ? *session_policies_from_authenticator
            : local_session_policies_provider_->get_local_policies();
  };
  service_client_->ReportSessionDisconnected(
      request, base::BindOnce([](const ProtobufHttpStatus& status) {
        if (status.ok()) {
          HOST_LOG << "Disconnect event logged.";
        } else {
          LOG(ERROR) << "Failed to log disconnect event: "
                     << status.error_message() << '('
                     << static_cast<int>(status.error_code()) << ')';
        }
      }));
}

}  // namespace remoting
