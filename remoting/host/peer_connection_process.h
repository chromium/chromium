// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_PEER_CONNECTION_PROCESS_H_
#define REMOTING_HOST_PEER_CONNECTION_PROCESS_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "ipc/ipc_listener.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "remoting/base/source_location.h"
#include "remoting/host/base/desktop_environment_options.h"
#include "remoting/host/mojom/peer_session.mojom.h"
#include "remoting/host/peer_session.h"
#include "remoting/protocol/errors.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace IPC {
class ChannelProxy;
}  // namespace IPC

namespace remoting {

class IpcDesktopEnvironmentFactory;

// Implements the Peer Connection process. This process runs at lower privileges
// and hosts the WebRTC connection (signaling and data channels). It
// communicates with the privileged Daemon process via Mojo IPC.
class PeerConnectionProcess : public IPC::Listener,
                              public mojom::PeerConnectionProcessControl,
                              public mojom::PeerSession {
 public:
  PeerConnectionProcess(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  PeerConnectionProcess(const PeerConnectionProcess&) = delete;
  PeerConnectionProcess& operator=(const PeerConnectionProcess&) = delete;

  ~PeerConnectionProcess() override;

  // Start the process, connecting to the Daemon via the passed channel handle.
  bool Start(mojo::ScopedMessagePipeHandle channel_handle);

  // IPC::Listener implementation.
  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override;

  // mojom::PeerConnectionProcessControl implementation.
  void BindPeerSession(
      mojo::PendingReceiver<mojom::PeerSession> session_receiver) override;

  // mojom::PeerSession implementation.
  void Start(
      mojo::PendingRemote<mojom::PeerSessionEventHandler> event_handler,
      const std::string& client_jid,
      mojo::PendingRemote<mojom::DesktopSession> control_remote,
      mojo::PendingReceiver<mojom::DesktopSessionEvents> events_receiver,
      const DesktopEnvironmentOptions& desktop_environment_options) override;
  void DisconnectSession(protocol::ErrorCode error,
                         const std::string& error_details,
                         const SourceLocation& error_location) override;
  void OnSessionServicesClientConnected(
      mojo::PendingReceiver<mojom::ChromotingSessionServices> receiver)
      override;

  void set_on_shutdown_for_testing(base::OnceClosure callback) {
    on_shutdown_for_testing_ = std::move(callback);
  }

 private:
  // IPC::Listener implementation.
  void OnChannelError() override;

  void OnSessionDisconnected();

  void Shutdown(int exit_code);

  void GetDesktopSession(
      mojo::PendingReceiver<mojom::DesktopSession> control_receiver,
      mojo::PendingRemote<mojom::DesktopSessionEvents> events_remote,
      mojom::DesktopSessionOptionsPtr options);

  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  std::unique_ptr<IPC::ChannelProxy> daemon_channel_;

  mojo::AssociatedReceiver<mojom::PeerConnectionProcessControl>
      control_receiver_{this};
  mojo::Receiver<mojom::PeerSession> session_receiver_{this};

  mojo::PendingRemote<mojom::DesktopSession> desktop_session_control_remote_;
  mojo::PendingReceiver<mojom::DesktopSessionEvents>
      desktop_session_events_receiver_;

  std::unique_ptr<IpcDesktopEnvironmentFactory> desktop_environment_factory_;
  mojo::Remote<mojom::PeerSessionEventHandler> event_handler_;
  std::unique_ptr<::remoting::PeerSession> peer_session_;

  base::OnceClosure on_shutdown_for_testing_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<PeerConnectionProcess> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_PEER_CONNECTION_PROCESS_H_
