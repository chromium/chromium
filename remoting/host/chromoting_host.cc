// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromoting_host.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "jingle/glue/thread_wrapper.h"
#include "remoting/base/constants.h"
#include "remoting/base/logging.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/host_config.h"
#include "remoting/host/input_injector.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/ice_connection_to_client.h"
#include "remoting/protocol/input_stub.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/protocol/webrtc_connection_to_client.h"

using remoting::protocol::ConnectionToClient;
using remoting::protocol::InputStub;

namespace remoting {

namespace {

const net::BackoffEntry::Policy kDefaultBackoffPolicy = {
  // Number of initial errors (in sequence) to ignore before applying
  // exponential back-off rules.
  5,

  // Initial delay for exponential back-off in ms.
  2000,

  // Factor by which the waiting time will be multiplied.
  2,

  // Fuzzing percentage. ex: 10% will spread requests randomly
  // between 90%-100% of the calculated time.
  0,

  // Maximum amount of time we are willing to delay our request in ms.
  -1,

  // Time to keep an entry from being discarded even when it
  // has no significant state, -1 to never discard.
  -1,

  // Don't use initial delay unless the last request was an error.
  false,
};

}  // namespace

ChromotingHost::ChromotingHost(
    DesktopEnvironmentFactory* desktop_environment_factory,
    std::unique_ptr<protocol::SessionManager> session_manager,
    scoped_refptr<protocol::TransportContext> transport_context,
    scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> video_encode_task_runner,
    const DesktopEnvironmentOptions& options)
    : desktop_environment_factory_(desktop_environment_factory),
      session_manager_(std::move(session_manager)),
      transport_context_(transport_context),
      audio_task_runner_(audio_task_runner),
      video_encode_task_runner_(video_encode_task_runner),
      status_monitor_(new HostStatusMonitor()),
      login_backoff_(&kDefaultBackoffPolicy),
      desktop_environment_options_(options) {
  jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();
}

ChromotingHost::~ChromotingHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Disconnect all of the clients.
  while (!clients_.empty()) {
    clients_.front()->DisconnectSession(protocol::OK);
  }

  // Destroy the session manager to make sure that |signal_strategy_| does not
  // have any listeners registered.
  session_manager_.reset();

  // Notify observers.
  if (started_) {
    for (auto& observer : status_monitor_->observers())
      observer.OnShutdown();
  }
}

void ChromotingHost::Start(const std::string& host_owner_email) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!started_);

  HOST_LOG << "Starting host";
  started_ = true;
  for (auto& observer : status_monitor_->observers())
    observer.OnStart(host_owner_email);

  session_manager_->AcceptIncoming(
      base::Bind(&ChromotingHost::OnIncomingSession, base::Unretained(this)));
}

void ChromotingHost::AddExtension(std::unique_ptr<HostExtension> extension) {
  extensions_.push_back(std::move(extension));
}

void ChromotingHost::SetAuthenticatorFactory(
    std::unique_ptr<protocol::AuthenticatorFactory> authenticator_factory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  session_manager_->set_authenticator_factory(std::move(authenticator_factory));
}

void ChromotingHost::SetMaximumSessionDuration(
    const base::TimeDelta& max_session_duration) {
  max_session_duration_ = max_session_duration;
}

////////////////////////////////////////////////////////////////////////////
// protocol::ClientSession::EventHandler implementation.
void ChromotingHost::OnSessionAuthenticating(ClientSession* client) {
  // We treat each incoming connection as a failure to authenticate,
  // and clear the backoff when a connection successfully
  // authenticates. This allows the backoff to protect from parallel
  // connection attempts as well as sequential ones.
  if (login_backoff_.ShouldRejectRequest()) {
    LOG(WARNING) << "Disconnecting client " << client->client_jid() << " due to"
                    " an overload of failed login attempts.";
    client->DisconnectSession(protocol::HOST_OVERLOAD);
    return;
  }
  login_backoff_.InformOfRequest(false);
}

void ChromotingHost::OnSessionAuthenticated(ClientSession* client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  login_backoff_.Reset();

  // Disconnect all clients, except |client|.
  base::WeakPtr<ChromotingHost> self = weak_factory_.GetWeakPtr();
  while (clients_.size() > 1) {
    clients_[(clients_.front().get() == client) ? 1 : 0]->DisconnectSession(
        protocol::OK);

    // Quit if the host was destroyed.
    if (!self)
      return;
  }

  // Disconnects above must have destroyed all other clients.
  DCHECK_EQ(clients_.size(), 1U);
  DCHECK(clients_.front().get() == client);

  // Notify observers that there is at least one authenticated client.
  for (auto& observer : status_monitor_->observers())
    observer.OnClientAuthenticated(client->client_jid());
}

void ChromotingHost::OnSessionChannelsConnected(ClientSession* client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Notify observers.
  for (auto& observer : status_monitor_->observers())
    observer.OnClientConnected(client->client_jid());
}

void ChromotingHost::OnSessionAuthenticationFailed(ClientSession* client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Notify observers.
  for (auto& observer : status_monitor_->observers())
    observer.OnAccessDenied(client->client_jid());
}

void ChromotingHost::OnSessionClosed(ClientSession* client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = std::find_if(clients_.begin(), clients_.end(),
                         [client](const std::unique_ptr<ClientSession>& item) {
                           return item.get() == client;
                         });
  CHECK(it != clients_.end());

  bool was_authenticated = client->is_authenticated();
  std::string jid = client->client_jid();
  clients_.erase(it);

  if (was_authenticated) {
    for (auto& observer : status_monitor_->observers())
      observer.OnClientDisconnected(jid);
  }
}

void ChromotingHost::OnSessionRouteChange(
    ClientSession* session,
    const std::string& channel_name,
    const protocol::TransportRoute& route) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : status_monitor_->observers())
    observer.OnClientRouteChange(session->client_jid(), channel_name, route);
}

void ChromotingHost::OnIncomingSession(
      protocol::Session* session,
      protocol::SessionManager::IncomingSessionResponse* response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(started_);

  if (login_backoff_.ShouldRejectRequest()) {
    LOG(WARNING) << "Rejecting connection due to"
                    " an overload of failed login attempts.";
    *response = protocol::SessionManager::OVERLOAD;
    return;
  }

  *response = protocol::SessionManager::ACCEPT;

  HOST_LOG << "Client connected: " << session->jid();

  // Create either IceConnectionToClient or WebrtcConnectionToClient.
  // TODO(sergeyu): Move this logic to the protocol layer.
  std::unique_ptr<protocol::ConnectionToClient> connection;
  if (session->config().protocol() ==
      protocol::SessionConfig::Protocol::WEBRTC) {
    connection.reset(new protocol::WebrtcConnectionToClient(
        base::WrapUnique(session), transport_context_,
        video_encode_task_runner_, audio_task_runner_));
  } else {
    connection.reset(new protocol::IceConnectionToClient(
        base::WrapUnique(session), transport_context_,
        video_encode_task_runner_, audio_task_runner_));
  }

  // Create a ClientSession object.
  std::vector<HostExtension*> extension_ptrs;
  for (const auto& extension : extensions_)
    extension_ptrs.push_back(extension.get());
  clients_.push_back(std::make_unique<ClientSession>(
      this, std::move(connection), desktop_environment_factory_,
      desktop_environment_options_, max_session_duration_, pairing_registry_,
      extension_ptrs));
}

}  // namespace remoting
