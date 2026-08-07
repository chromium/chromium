// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IPC_PEER_SESSION_H_
#define REMOTING_HOST_IPC_PEER_SESSION_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "remoting/host/mojom/desktop_session.mojom.h"
#include "remoting/host/mojom/peer_session.mojom.h"
#include "remoting/host/peer_session.h"

namespace remoting {

// Proxy implementation of `PeerSession` that communicates with the remote
// `PeerSessionImpl` in the dedicated Peer Connection process over Mojo IPC.
class IpcPeerSession : public PeerSession {
 public:
  using GetDesktopSessionCallback = base::RepeatingCallback<void(
      mojo::PendingReceiver<mojom::DesktopSession>,
      mojo::PendingRemote<mojom::DesktopSessionEvents>,
      mojom::DesktopSessionOptionsPtr)>;

  IpcPeerSession(mojo::PendingRemote<mojom::PeerSession> peer_session_remote,
                 GetDesktopSessionCallback get_desktop_session_callback);
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
  void OnPeerSessionDisconnected();
  void OnEventHandlerDisconnected();
  void NotifySessionClosed(protocol::ErrorCode error,
                           const std::string& error_details,
                           const SourceLocation& error_location);
  void DoNotifySessionClosed(protocol::ErrorCode error,
                             const std::string& error_details,
                             const SourceLocation& error_location);

  SEQUENCE_CHECKER(sequence_checker_);

  mojo::Remote<mojom::PeerSession> remote_;
  std::unique_ptr<mojo::Receiver<mojom::PeerSessionEventHandler>>
      event_handler_receiver_;
  raw_ptr<EventHandler> event_handler_ = nullptr;
  GetDesktopSessionCallback get_desktop_session_callback_;

  base::WeakPtrFactory<IpcPeerSession> weak_factory_{this};
};

// Factory implementation for creating `IpcPeerSession` instances over an
// associated Mojo interface to `PeerSessionManager` in `DaemonProcess`.
class IpcPeerSessionFactory : public PeerSessionFactory {
 public:
  IpcPeerSessionFactory(
      mojo::PendingAssociatedRemote<mojom::PeerSessionManager>
          peer_session_manager,
      mojo::PendingAssociatedRemote<mojom::DesktopSessionManager>
          desktop_session_manager);
  IpcPeerSessionFactory(const IpcPeerSessionFactory&) = delete;
  IpcPeerSessionFactory& operator=(const IpcPeerSessionFactory&) = delete;
  ~IpcPeerSessionFactory() override;

  // PeerSessionFactory interface:
  std::unique_ptr<PeerSession> Create() override;

  // If set to a non-empty value, the login user of the desktop session must
  // match `username`.
  void SetRequiredUsername(std::string_view username);

 private:
  void EnsureBound();
  void OnPeerSessionManagerDisconnected();
  void OnDesktopSessionManagerDisconnected();

  void GetDesktopSession(
      mojo::PendingReceiver<mojom::DesktopSession> control_receiver,
      mojo::PendingRemote<mojom::DesktopSessionEvents> events_remote,
      mojom::DesktopSessionOptionsPtr options);

  SEQUENCE_CHECKER(sequence_checker_);

  std::string required_username_;

  mojo::PendingAssociatedRemote<mojom::PeerSessionManager>
      pending_peer_session_manager_;
  mojo::PendingAssociatedRemote<mojom::DesktopSessionManager>
      pending_desktop_session_manager_;
  mojo::AssociatedRemote<mojom::PeerSessionManager> peer_session_manager_;
  mojo::AssociatedRemote<mojom::DesktopSessionManager> desktop_session_manager_;

  base::WeakPtrFactory<IpcPeerSessionFactory> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_IPC_PEER_SESSION_H_
