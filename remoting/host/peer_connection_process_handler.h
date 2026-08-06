// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_PEER_CONNECTION_PROCESS_HANDLER_H_
#define REMOTING_HOST_PEER_CONNECTION_PROCESS_HANDLER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "remoting/host/mojom/peer_session.mojom.h"
#include "remoting/host/worker_process_ipc_delegate.h"
#include "remoting/host/worker_process_launcher.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

// Manages the lifecycle and Mojo bootstrapping of a dedicated Peer Connection
// process for a single desktop session.
class PeerConnectionProcessHandler : public WorkerProcessIpcDelegate {
 public:
  using StoppedCallback =
      base::OnceCallback<void(base::WeakPtr<PeerConnectionProcessHandler>)>;

  PeerConnectionProcessHandler(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      std::unique_ptr<WorkerProcessLauncher::Delegate> launcher_delegate,
      StoppedCallback stopped_callback);

  PeerConnectionProcessHandler(const PeerConnectionProcessHandler&) = delete;
  PeerConnectionProcessHandler& operator=(const PeerConnectionProcessHandler&) =
      delete;

  ~PeerConnectionProcessHandler() override;

  // Binds the `PeerSession` receiver in the PC process.
  void BindPeerSession(mojo::PendingReceiver<mojom::PeerSession> receiver);

  // Crashes the Peer Connection process.
  void Crash(const base::Location& location);

  // WorkerProcessIpcDelegate implementation.
  void OnChannelConnected(int32_t peer_pid) override;
  void OnPermanentError(int exit_code) override;
  void OnWorkerProcessStopped() override;
  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override;

 private:
  void CloseSelf();

  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;
  std::unique_ptr<WorkerProcessLauncher> launcher_;
  StoppedCallback stopped_callback_;

  mojo::AssociatedRemote<mojom::PeerConnectionProcessControl>
      pc_process_control_;
  mojo::PendingReceiver<mojom::PeerSession> pending_session_receiver_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PeerConnectionProcessHandler> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_PEER_CONNECTION_PROCESS_HANDLER_H_
