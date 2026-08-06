// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ipc_peer_session.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace remoting {

IpcPeerSession::IpcPeerSession(
    mojo::PendingRemote<mojom::PeerSession> peer_session_remote) {
  if (peer_session_remote) {
    remote_.Bind(std::move(peer_session_remote));
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
}

void IpcPeerSession::DisconnectSession(protocol::ErrorCode error,
                                       std::string_view error_details,
                                       const SourceLocation& error_location) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void IpcPeerSession::OnSessionServicesClientConnected(
    mojo::PendingReceiver<mojom::ChromotingSessionServices> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

protocol::Transport* IpcPeerSession::transport() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return nullptr;
}

IpcPeerSessionFactory::IpcPeerSessionFactory(
    mojo::PendingAssociatedRemote<mojom::PeerSessionManager>
        peer_session_manager)
    : peer_session_manager_(std::move(peer_session_manager)) {
  if (peer_session_manager_.is_bound()) {
    peer_session_manager_.set_disconnect_handler(
        base::BindOnce(&IpcPeerSessionFactory::OnPeerSessionManagerDisconnected,
                       base::Unretained(this)));
  }
}

IpcPeerSessionFactory::~IpcPeerSessionFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void IpcPeerSessionFactory::OnPeerSessionManagerDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(ERROR) << "PeerSessionManager associated remote disconnected.";
  peer_session_manager_.reset();
}

std::unique_ptr<PeerSession> IpcPeerSessionFactory::Create() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!peer_session_manager_.is_bound()) {
    LOG(ERROR) << "PeerSessionManager is not bound.";
    return nullptr;
  }
  mojo::PendingRemote<mojom::PeerSession> pending_remote;
  peer_session_manager_->LaunchPeerSession(
      pending_remote.InitWithNewPipeAndPassReceiver());
  return std::make_unique<IpcPeerSession>(std::move(pending_remote));
}

}  // namespace remoting
