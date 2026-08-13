// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_PEER_CONNECTION_PROCESS_H_
#define REMOTING_HOST_PEER_CONNECTION_PROCESS_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

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
#include "remoting/host/mojom/desktop_session.mojom.h"
#include "remoting/host/mojom/peer_session.mojom.h"
#include "remoting/host/peer_session.h"
#include "remoting/protocol/errors.h"
#include "remoting/signaling/jingle_data_structures.h"

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
                              public mojom::WorkerProcessControl,
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

  // mojom::WorkerProcessControl implementation.
  void CrashProcess(const std::string& function_name,
                    const std::string& file_name,
                    int line_number) override;

  // mojom::PeerConnectionProcessControl implementation.
  void BindPeerSession(
      mojo::PendingReceiver<mojom::PeerSession> session_receiver) override;

  // mojom::PeerSession implementation.
  void Start(const std::string& client_jid,
             mojo::PendingRemote<mojom::PeerSessionEventHandler> event_handler,
             mojo::PendingRemote<mojom::DesktopSession> desktop_control,
             mojo::PendingReceiver<mojom::DesktopSessionEvents>
                 desktop_events_receiver,
             mojo::PendingRemote<mojom::IceConfigFetcher> ice_config_fetcher,
             mojo::PendingRemote<mojom::PairingRequester> pairing_requester,
             const DesktopEnvironmentOptions& desktop_environment_options,
             const SessionPolicies& session_policies,
             const SessionOptions& session_options) override;
  void StartTransport(const std::string& auth_key,
                      mojo::PendingRemote<mojom::TransportEventHandler>
                          transport_event_handler) override;
  void ProcessTransportInfo(const JingleTransportInfo& transport_info) override;
  void DisconnectSession(protocol::ErrorCode error,
                         const std::string& error_details,
                         const SourceLocation& error_location) override;

  void set_on_shutdown_for_testing(base::OnceClosure callback) {
    on_shutdown_for_testing_ = std::move(callback);
  }

 private:
  // IPC::Listener implementation.
  void OnChannelError() override;

  void OnSendTransportInfo(std::unique_ptr<JingleTransportInfo> transport_info);
  void OnSessionDisconnected();

  void Shutdown(int exit_code);

  void RequestPairing(
      const std::string& client_name,
      PeerSessionFactory::RequestPairingResponseCallback response_cb);

  void GetDesktopSession(
      mojo::PendingReceiver<mojom::DesktopSession> control_receiver,
      mojo::PendingRemote<mojom::DesktopSessionEvents> events_remote,
      mojom::DesktopSessionOptionsPtr options);

  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  std::unique_ptr<IPC::ChannelProxy> daemon_channel_;

  mojo::AssociatedReceiver<mojom::WorkerProcessControl> worker_process_control_{
      this};
  mojo::AssociatedReceiver<mojom::PeerConnectionProcessControl>
      control_receiver_{this};
  mojo::Receiver<mojom::PeerSession> session_receiver_{this};

  mojo::PendingRemote<mojom::DesktopSession> desktop_session_control_remote_;
  mojo::PendingReceiver<mojom::DesktopSessionEvents>
      desktop_session_events_receiver_;

  std::unique_ptr<IpcDesktopEnvironmentFactory> desktop_environment_factory_;
  mojo::Remote<mojom::PeerSessionEventHandler> event_handler_;
  mojo::Remote<mojom::TransportEventHandler> transport_event_handler_;
  mojo::Remote<mojom::PairingRequester> pairing_requester_remote_;
  std::unique_ptr<::remoting::PeerSession> peer_session_;

  // Transport messages may arrive from the NetworkProcess via Mojo before
  // PeerConnectionProcess::Start() finishes initializing the PeerSession (e.g.
  // while waiting for IPC channel setup or session creation). These members
  // buffer incoming messages until Start() completes, at which point they are
  // flushed to the underlying PeerSession and transport.
  struct PendingStartTransport {
    std::string auth_key;
    mojo::PendingRemote<mojom::TransportEventHandler> transport_event_handler;
  };
  std::optional<PendingStartTransport> pending_start_transport_;
  std::vector<JingleTransportInfo> pending_transport_infos_;

  base::OnceClosure on_shutdown_for_testing_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<PeerConnectionProcess> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_PEER_CONNECTION_PROCESS_H_
