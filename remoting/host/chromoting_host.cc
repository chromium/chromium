// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromoting_host.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/named_mojo_ipc_server/connection_info.h"
#include "components/named_mojo_ipc_server/endpoint_options.h"
#include "components/named_mojo_ipc_server/named_mojo_ipc_server.h"
#include "components/webrtc/thread_wrapper.h"
#include "remoting/base/constants.h"
#include "remoting/base/local_session_policies_provider.h"
#include "remoting/base/logging.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/host_config.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/ipc_constants.h"
#include "remoting/host/mojo_caller_security_checker.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/ice_connection_to_client.h"
#include "remoting/protocol/input_stub.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/protocol/webrtc_connection_to_client.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/strings/stringprintf.h"
#include "base/win/win_util.h"
#endif

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
    const DesktopEnvironmentOptions& options,
    const LocalSessionPoliciesProvider* local_session_policies_provider)
    : desktop_environment_factory_(desktop_environment_factory),
      session_manager_(std::move(session_manager)),
      transport_context_(transport_context),
      audio_task_runner_(audio_task_runner),
      video_encode_task_runner_(video_encode_task_runner),
      status_monitor_(new HostStatusMonitor()),
      login_backoff_(&kDefaultBackoffPolicy),
      desktop_environment_options_(options),
      local_session_policies_provider_(local_session_policies_provider) {
  webrtc::ThreadWrapper::EnsureForCurrentMessageLoop();
}

ChromotingHost::~ChromotingHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Disconnect all of the clients.
  while (!clients_.empty()) {
    clients_.front()->DisconnectSession(ErrorCode::OK);
  }

  // Destroy the session manager to make sure that |signal_strategy_| does not
  // have any listeners registered.
  session_manager_.reset();

  // Notify observers.
  if (started_) {
    for (auto& observer : status_monitor_->observers()) {
      observer.OnHostShutdown();
    }
  }
}

void ChromotingHost::Start(const std::string& host_owner_email) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!started_);

  HOST_LOG << "Starting host";
  started_ = true;
  for (auto& observer : status_monitor_->observers()) {
    observer.OnHostStarted(host_owner_email);
  }

  session_manager_->AcceptIncoming(base::BindRepeating(
      &ChromotingHost::OnIncomingSession, base::Unretained(this)));
}

#if BUILDFLAG(IS_LINUX)
void ChromotingHost::StartChromotingHostServices() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!ipc_server_);

  ipc_server_ =
      std::make_unique<ChromotingHostServicesServer>(base::BindRepeating(
          &ChromotingHost::BindChromotingHostServices, base::Unretained(this)));
  ipc_server_->StartServer();
  HOST_LOG << "ChromotingHostServices IPC server has been started.";
}
#endif

void ChromotingHost::BindChromotingHostServices(
    mojo::PendingReceiver<mojom::ChromotingHostServices> receiver,
    base::ProcessId peer_pid) {
  receivers_.Add(this, std::move(receiver), peer_pid);
}

void ChromotingHost::AddExtension(std::unique_ptr<HostExtension> extension) {
  extensions_.push_back(std::move(extension));
}

void ChromotingHost::SetAuthenticatorFactory(
    std::unique_ptr<protocol::AuthenticatorFactory> authenticator_factory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  session_manager_->set_authenticator_factory(std::move(authenticator_factory));
}

////////////////////////////////////////////////////////////////////////////
// protocol::ClientSession::EventHandler implementation.
void ChromotingHost::OnSessionAuthenticating(ClientSession* client) {
  // We treat each incoming connection as a failure to authenticate,
  // and clear the backoff when a connection successfully
  // authenticates. This allows the backoff to protect from parallel
  // connection attempts as well as sequential ones.
  if (login_backoff_.ShouldRejectRequest()) {
    LOG(WARNING) << "Disconnecting client " << client->client_jid()
                 << " due to"
                    " an overload of failed login attempts.";
    client->DisconnectSession(ErrorCode::HOST_OVERLOAD);
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
        ErrorCode::OK);

    // Quit if the host was destroyed.
    if (!self) {
      return;
    }
  }

  // Disconnects above must have destroyed all other clients.
  DCHECK_EQ(clients_.size(), 1U);
  DCHECK(clients_.front().get() == client);

  // Notify observers that there is at least one authenticated client.
  for (auto& observer : status_monitor_->observers()) {
    observer.OnClientAuthenticated(client->client_jid());
  }
}

void ChromotingHost::OnSessionChannelsConnected(ClientSession* client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Notify observers.
  for (auto& observer : status_monitor_->observers()) {
    observer.OnClientConnected(client->client_jid());
  }
}

void ChromotingHost::OnSessionAuthenticationFailed(ClientSession* client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Notify observers.
  for (auto& observer : status_monitor_->observers()) {
    observer.OnClientAccessDenied(client->client_jid());
  }
}

void ChromotingHost::OnSessionClosed(ClientSession* client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = base::ranges::find(clients_, client,
                               &std::unique_ptr<ClientSession>::get);
  CHECK(it != clients_.end());

  bool was_authenticated = client->is_authenticated();
  std::string jid = client->client_jid();
  clients_.erase(it);

  if (was_authenticated) {
    for (auto& observer : status_monitor_->observers()) {
      observer.OnClientDisconnected(jid);
    }
  }
}

void ChromotingHost::OnSessionRouteChange(
    ClientSession* session,
    const std::string& channel_name,
    const protocol::TransportRoute& route) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : status_monitor_->observers()) {
    observer.OnClientRouteChange(session->client_jid(), channel_name, route);
  }
}

void ChromotingHost::BindSessionServices(
    mojo::PendingReceiver<mojom::ChromotingSessionServices> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ClientSession* connected_client = GetConnectedClientSession();
  if (!connected_client) {
    LOG(WARNING) << "Session services bind request rejected: "
                 << "No connected remote desktop client was found.";
    return;
  }
#if BUILDFLAG(IS_WIN)
  DWORD peer_session_id;
  if (!ProcessIdToSessionId(receivers_.current_context(), &peer_session_id)) {
    PLOG(ERROR) << "Session services bind request rejected: "
                   "ProcessIdToSessionId failed";
    return;
  }
  if (connected_client->desktop_session_id() != peer_session_id) {
    LOG(WARNING)
        << "Session services bind request rejected: "
        << "Remote desktop client is not connected to the current session.";
    return;
  }
#endif
  connected_client->BindReceiver(std::move(receiver));
  VLOG(1) << "Session services bound for receiver ID: "
          << receivers_.current_receiver();
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
    connection = std::make_unique<protocol::WebrtcConnectionToClient>(
        base::WrapUnique(session), transport_context_, audio_task_runner_);
  } else {
    connection = std::make_unique<protocol::IceConnectionToClient>(
        base::WrapUnique(session), transport_context_,
        video_encode_task_runner_, audio_task_runner_);
  }

  // Create a ClientSession object.
  std::vector<raw_ptr<HostExtension, VectorExperimental>> extension_ptrs;
  for (const auto& extension : extensions_) {
    extension_ptrs.push_back(extension.get());
  }
  clients_.push_back(std::make_unique<ClientSession>(
      this, std::move(connection), desktop_environment_factory_,
      desktop_environment_options_, pairing_registry_, extension_ptrs,
      local_session_policies_provider_));
}

ClientSession* ChromotingHost::GetConnectedClientSession() const {
  ClientSession* connected_client = nullptr;
  for (auto& client : clients_) {
    if (client->channels_connected()) {
      if (connected_client) {
        LOG(DFATAL) << "More than one connected client is found.";
        return nullptr;
      }
      connected_client = client.get();
    }
  }
  return connected_client;
}

}  // namespace remoting
