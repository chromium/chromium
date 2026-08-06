// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IPC_PEER_SESSION_H_
#define REMOTING_HOST_IPC_PEER_SESSION_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "remoting/host/mojom/peer_session.mojom.h"
#include "remoting/host/peer_session.h"

namespace remoting {

// Proxy implementation of `PeerSession` that communicates with the remote
// `PeerSessionImpl` in the dedicated Peer Connection process over Mojo IPC.
class IpcPeerSession : public PeerSession {
 public:
  explicit IpcPeerSession(
      mojo::PendingRemote<mojom::PeerSession> peer_session_remote);
  IpcPeerSession(const IpcPeerSession&) = delete;
  IpcPeerSession& operator=(const IpcPeerSession&) = delete;
  ~IpcPeerSession() override;

  // PeerSession interface:
  void Start(EventHandler* event_handler,
             std::string_view client_jid,
             const DesktopEnvironmentOptions& desktop_environment_options,
             const std::vector<HostExtension*>& extensions,
             const SessionPolicies& session_policies,
             const SessionOptions& session_options) override;
  void DisconnectSession(protocol::ErrorCode error,
                         std::string_view error_details,
                         const SourceLocation& error_location) override;
  void OnSessionServicesClientConnected(
      mojo::PendingReceiver<mojom::ChromotingSessionServices> receiver)
      override;
  protocol::Transport* transport() const override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  mojo::Remote<mojom::PeerSession> remote_;
};

// Factory implementation for creating `IpcPeerSession` instances over an
// associated Mojo interface to `PeerSessionManager` in `DaemonProcess`.
class IpcPeerSessionFactory : public PeerSessionFactory {
 public:
  explicit IpcPeerSessionFactory(
      mojo::PendingAssociatedRemote<mojom::PeerSessionManager>
          peer_session_manager);
  IpcPeerSessionFactory(const IpcPeerSessionFactory&) = delete;
  IpcPeerSessionFactory& operator=(const IpcPeerSessionFactory&) = delete;
  ~IpcPeerSessionFactory() override;

  // PeerSessionFactory interface:
  std::unique_ptr<PeerSession> Create() override;

 private:
  void OnPeerSessionManagerDisconnected();

  SEQUENCE_CHECKER(sequence_checker_);

  mojo::AssociatedRemote<mojom::PeerSessionManager> peer_session_manager_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_IPC_PEER_SESSION_H_
