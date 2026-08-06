// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/client_session.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/strings/stringprintf.h"
#include "remoting/base/errors.h"
#include "remoting/base/local_session_policies_provider.h"
#include "remoting/base/logging.h"
#include "remoting/base/session_policies.h"
#include "remoting/host/base/desktop_environment_options.h"
#include "remoting/host/host_extension.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/connection_to_client.h"
#include "remoting/protocol/errors.h"
#include "remoting/protocol/ice_config_fetcher.h"
#include "remoting/protocol/session.h"

namespace remoting {

ClientSession::ClientSession(
    EventHandler* event_handler,
    std::unique_ptr<protocol::Session> session,
    PeerSessionFactory* peer_session_factory,
    const DesktopEnvironmentOptions& desktop_environment_options,
    const std::vector<raw_ptr<HostExtension, VectorExperimental>>& extensions,
    const LocalSessionPoliciesProvider* local_session_policies_provider)
    : event_handler_(event_handler),
      desktop_environment_options_(desktop_environment_options),
      extensions_(extensions),
      peer_session_factory_(peer_session_factory),
      session_(std::move(session)),
      client_jid_(session_->jid()),
      local_session_policies_provider_(local_session_policies_provider) {
  session_->AddPlugin(&host_experiment_session_plugin_);
  session_->SetEventHandler(this);
}

ClientSession::~ClientSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

const std::string& ClientSession::client_jid() const {
  return client_jid_;
}

void ClientSession::DisconnectSession(ErrorCode error,
                                      std::string_view error_details,
                                      const SourceLocation& error_location) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (peer_session_) {
    peer_session_->DisconnectSession(error, error_details, error_location);
    return;
  }
  OnSessionClosed(error, std::string(error_details), error_location);
}

void ClientSession::OnSessionStateChange(protocol::Session::State state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (state) {
    case protocol::Session::INITIALIZING:
    case protocol::Session::CONNECTING:
    case protocol::Session::ACCEPTING:
    case protocol::Session::ACCEPTED:
      // Don't care about these events.
      break;

    case protocol::Session::AUTHENTICATING:
      OnConnectionAuthenticating();
      break;

    case protocol::Session::AUTHENTICATED:
      OnConnectionAuthenticated(session_->authenticator().GetSessionPolicies());
      break;

    case protocol::Session::CLOSED:
    case protocol::Session::FAILED: {
      ErrorCode error = (state == protocol::Session::CLOSED || !session_)
                            ? ErrorCode::OK
                            : session_->error();
      // DisconnectSession() notifies event_handler_->OnSessionClosed(), which
      // executes session teardown.
      DisconnectSession(error, /* error_details= */ {}, FROM_HERE);
      break;
    }
  }
}

void ClientSession::OnConnectionAuthenticating() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  event_handler_->OnSessionAuthenticating(this);
}

void ClientSession::OnConnectionAuthenticated(
    const SessionPolicies* session_policies) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HOST_LOG << "Client authenticated: " << client_jid_;

  if (session_policies) {
    HOST_LOG << "Connection authenticated with remote session policies: "
             << *session_policies;
    effective_policies_ = *session_policies;
  } else if (local_session_policies_provider_) {
    effective_policies_ =
        local_session_policies_provider_->get_local_policies();
    HOST_LOG << "Connection authenticated with local session policies: "
             << effective_policies_;
    local_session_policy_update_subscription_ =
        local_session_policies_provider_->AddLocalPoliciesChangedCallback(
            base::BindRepeating(&ClientSession::OnLocalSessionPoliciesChanged,
                                weak_factory_.GetWeakPtr()));
  }

  // TODO(crbug.com/382334458): Include error details and location in the
  // validation result.
  std::optional<ErrorCode> validation_result =
      event_handler_->OnSessionPoliciesReceived(effective_policies_);
  if (validation_result.has_value()) {
    std::string error_details = base::StringPrintf(
        "Session policies disallowed by validator. Error code: %d",
        static_cast<int>(*validation_result));
    DisconnectSession(*validation_result, error_details, FROM_HERE);
    return;
  }

  is_authenticated_ = true;

  const SessionOptions session_options =
      SessionOptions::Parse(host_experiment_session_plugin_.configuration());
  DesktopEnvironmentOptions desktop_environment_options =
      desktop_environment_options_;
  desktop_environment_options.ApplySessionOptions(session_options);

  peer_session_ = peer_session_factory_->Create();

  session_->SetTransport(peer_session_->transport());

  std::vector<HostExtension*> extension_ptrs;
  extension_ptrs.assign(extensions_.begin(), extensions_.end());

  peer_session_->Start(this, client_jid_, desktop_environment_options,
                       extension_ptrs, effective_policies_, session_options);

  for (auto& receiver : pending_session_services_receivers_) {
    peer_session_->OnSessionServicesClientConnected(std::move(receiver));
  }
  pending_session_services_receivers_.clear();

  event_handler_->OnSessionAuthenticated(this);
}

void ClientSession::OnSessionServicesClientConnected(
    mojo::PendingReceiver<mojom::ChromotingSessionServices> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (peer_session_) {
    peer_session_->OnSessionServicesClientConnected(std::move(receiver));
  } else {
    pending_session_services_receivers_.push_back(std::move(receiver));
  }
}

void ClientSession::OnSessionChannelsConnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  channels_connected_ = true;
  event_handler_->OnSessionChannelsConnected(this);
}

void ClientSession::OnSessionClosed(protocol::ErrorCode error,
                                    const std::string& error_details,
                                    const SourceLocation& error_location) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_closing_) {
    return;
  }
  is_closing_ = true;

  if (session_) {
    session_->Close(error, error_details, error_location);
  }

  // If the client never authenticated then the session failed.
  if (!is_authenticated_) {
    event_handler_->OnSessionAuthenticationFailed(this);
  }

  event_handler_->OnSessionClosed(this);
}

void ClientSession::OnSessionRouteChange(
    const std::string& channel_name,
    const protocol::TransportRoute& route) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  event_handler_->OnSessionRouteChange(this, channel_name, route);
}

void ClientSession::OnLocalSessionPoliciesChanged(
    const SessionPolicies& new_policies) {
  DCHECK(local_session_policy_update_subscription_);
  DisconnectSession(ErrorCode::SESSION_POLICIES_CHANGED,
                    "Effective policies have changed. Terminating session.",
                    FROM_HERE);
}

}  // namespace remoting
