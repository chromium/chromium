// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ipc_peer_session.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "remoting/signaling/signaling_id_util.h"

namespace remoting {

IpcPeerSession::IpcPeerSession(
    mojo::PendingRemote<mojom::PeerSession> peer_session_remote,
    GetDesktopSessionCallback get_desktop_session_callback)
    : get_desktop_session_callback_(std::move(get_desktop_session_callback)) {
  CHECK(get_desktop_session_callback_);
  if (peer_session_remote) {
    remote_.Bind(std::move(peer_session_remote));
    remote_.set_disconnect_handler(base::BindOnce(
        &IpcPeerSession::OnPeerSessionDisconnected, base::Unretained(this)));
  }
}

IpcPeerSession::~IpcPeerSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void IpcPeerSession::Start(
    EventHandler* event_handler,
    std::string_view client_jid,
    const DesktopEnvironmentOptions& desktop_environment_options,
    const std::vector<HostExtension*>& extensions,
    const SessionPolicies& session_policies,
    const SessionOptions& session_options) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(event_handler);
  event_handler_ = event_handler;
  if (!remote_.is_bound()) {
    LOG(WARNING) << "Start called when PeerSession remote is not bound.";
    NotifySessionClosed(protocol::ErrorCode::CHANNEL_CONNECTION_ERROR,
                        "PeerSession remote is not bound.", FROM_HERE);
    return;
  }

  DCHECK(!event_handler_receiver_);

  std::string client_id;
  SplitSignalingIdResource(client_jid, &client_id, /*resource=*/nullptr);
  // TODO(crbug.com/502281489): Populate `screen_resolution`.
  mojom::DesktopSessionOptionsPtr desktop_session_options =
      mojom::DesktopSessionOptions::New();
  desktop_session_options->is_curtained =
      desktop_environment_options.enable_curtaining();
  desktop_session_options->client_id = client_id;

  mojo::PendingRemote<mojom::DesktopSession> control_remote;
  mojo::PendingReceiver<mojom::DesktopSessionEvents> events_receiver;
  get_desktop_session_callback_.Run(
      control_remote.InitWithNewPipeAndPassReceiver(),
      events_receiver.InitWithNewPipeAndPassRemote(),
      std::move(desktop_session_options));

  mojo::PendingRemote<mojom::PeerSessionEventHandler> event_handler_remote;
  if (event_handler) {
    event_handler_receiver_ =
        std::make_unique<mojo::Receiver<mojom::PeerSessionEventHandler>>(
            event_handler);
    event_handler_remote = event_handler_receiver_->BindNewPipeAndPassRemote();
    event_handler_receiver_->set_disconnect_handler(base::BindOnce(
        &IpcPeerSession::OnEventHandlerDisconnected, base::Unretained(this)));
  }

  remote_->Start(std::move(event_handler_remote), std::string(client_jid),
                 std::move(control_remote), std::move(events_receiver),
                 desktop_environment_options);
}

void IpcPeerSession::DisconnectSession(protocol::ErrorCode error,
                                       std::string_view error_details,
                                       const SourceLocation& error_location) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!remote_.is_bound()) {
    LOG(WARNING)
        << "DisconnectSession called when PeerSession remote is not bound.";
    NotifySessionClosed(error, std::string(error_details), error_location);
    return;
  }
  remote_->DisconnectSession(error, std::string(error_details), error_location);
}

void IpcPeerSession::OnSessionServicesClientConnected(
    mojo::PendingReceiver<mojom::ChromotingSessionServices> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!remote_.is_bound()) {
    LOG(WARNING) << "OnSessionServicesClientConnected called when PeerSession "
                 << "remote is not bound.";
    return;
  }
  remote_->OnSessionServicesClientConnected(std::move(receiver));
}

protocol::Transport* IpcPeerSession::transport() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return nullptr;
}

void IpcPeerSession::OnPeerSessionDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(ERROR) << "PeerSession remote dropped unexpectedly.";
  remote_.reset();
  NotifySessionClosed(protocol::ErrorCode::CHANNEL_CONNECTION_ERROR,
                      "PeerSession remote dropped unexpectedly.", FROM_HERE);
}

void IpcPeerSession::OnEventHandlerDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(ERROR) << "PeerSessionEventHandler remote dropped unexpectedly.";
  event_handler_receiver_.reset();
  NotifySessionClosed(protocol::ErrorCode::CHANNEL_CONNECTION_ERROR,
                      "PeerSessionEventHandler dropped unexpectedly.",
                      FROM_HERE);
}

void IpcPeerSession::NotifySessionClosed(protocol::ErrorCode error,
                                         const std::string& error_details,
                                         const SourceLocation& error_location) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&IpcPeerSession::DoNotifySessionClosed,
                                weak_factory_.GetWeakPtr(), error,
                                error_details, error_location));
}

void IpcPeerSession::DoNotifySessionClosed(
    protocol::ErrorCode error,
    const std::string& error_details,
    const SourceLocation& error_location) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (event_handler_) {
    auto* handler = event_handler_.get();
    event_handler_ = nullptr;
    event_handler_receiver_.reset();
    handler->OnSessionClosed(error, error_details, error_location);
  }
}

IpcPeerSessionFactory::IpcPeerSessionFactory(
    mojo::PendingAssociatedRemote<mojom::PeerSessionManager>
        peer_session_manager,
    mojo::PendingAssociatedRemote<mojom::DesktopSessionManager>
        desktop_session_manager)
    : pending_peer_session_manager_(std::move(peer_session_manager)),
      pending_desktop_session_manager_(std::move(desktop_session_manager)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

IpcPeerSessionFactory::~IpcPeerSessionFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void IpcPeerSessionFactory::EnsureBound() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pending_peer_session_manager_) {
    peer_session_manager_.Bind(std::move(pending_peer_session_manager_));
    peer_session_manager_.set_disconnect_handler(
        base::BindOnce(&IpcPeerSessionFactory::OnPeerSessionManagerDisconnected,
                       base::Unretained(this)));
  }
  if (pending_desktop_session_manager_) {
    desktop_session_manager_.Bind(std::move(pending_desktop_session_manager_));
    desktop_session_manager_.set_disconnect_handler(base::BindOnce(
        &IpcPeerSessionFactory::OnDesktopSessionManagerDisconnected,
        base::Unretained(this)));
  }
}

void IpcPeerSessionFactory::OnPeerSessionManagerDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(ERROR) << "PeerSessionManager associated remote disconnected.";
  peer_session_manager_.reset();
}

void IpcPeerSessionFactory::OnDesktopSessionManagerDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(ERROR) << "DesktopSessionManager associated remote disconnected.";
  desktop_session_manager_.reset();
}

void IpcPeerSessionFactory::SetRequiredUsername(std::string_view username) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  required_username_ = std::string(username);
}

void IpcPeerSessionFactory::GetDesktopSession(
    mojo::PendingReceiver<mojom::DesktopSession> control_receiver,
    mojo::PendingRemote<mojom::DesktopSessionEvents> events_remote,
    mojom::DesktopSessionOptionsPtr options) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureBound();
  if (!desktop_session_manager_.is_bound()) {
    LOG(ERROR) << "DesktopSessionManager is not bound.";
    return;
  }
  options->required_username = required_username_;
  desktop_session_manager_->GetDesktopSession(std::move(control_receiver),
                                              std::move(events_remote),
                                              std::move(options));
}

std::unique_ptr<PeerSession> IpcPeerSessionFactory::Create() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureBound();
  if (!peer_session_manager_.is_bound()) {
    LOG(ERROR) << "PeerSessionManager is not bound.";
    return nullptr;
  }
  if (!desktop_session_manager_.is_bound()) {
    LOG(ERROR) << "DesktopSessionManager is not bound.";
    return nullptr;
  }

  mojo::PendingRemote<mojom::PeerSession> pending_remote;
  peer_session_manager_->LaunchPeerSession(
      pending_remote.InitWithNewPipeAndPassReceiver());

  return std::make_unique<IpcPeerSession>(
      std::move(pending_remote),
      base::BindRepeating(&IpcPeerSessionFactory::GetDesktopSession,
                          weak_factory_.GetWeakPtr()));
}

}  // namespace remoting
