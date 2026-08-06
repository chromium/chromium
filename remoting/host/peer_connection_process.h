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
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "remoting/host/mojom/peer_session.mojom.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace IPC {
class ChannelProxy;
}  // namespace IPC

namespace remoting {

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

  void set_on_shutdown_for_testing(base::OnceClosure callback) {
    on_shutdown_for_testing_ = std::move(callback);
  }

 private:
  // IPC::Listener implementation.
  void OnChannelError() override;

  void OnSessionDisconnected();

  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  std::unique_ptr<IPC::ChannelProxy> daemon_channel_;

  mojo::AssociatedReceiver<mojom::PeerConnectionProcessControl>
      control_receiver_{this};
  mojo::Receiver<mojom::PeerSession> session_receiver_{this};

  base::OnceClosure on_shutdown_for_testing_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<PeerConnectionProcess> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_PEER_CONNECTION_PROCESS_H_
