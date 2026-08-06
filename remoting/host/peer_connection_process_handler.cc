// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/peer_connection_process_handler.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"

namespace remoting {

PeerConnectionProcessHandler::PeerConnectionProcessHandler(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    std::unique_ptr<WorkerProcessLauncher::Delegate> launcher_delegate,
    StoppedCallback stopped_callback)
    : caller_task_runner_(caller_task_runner),
      stopped_callback_(std::move(stopped_callback)) {
  launcher_ = std::make_unique<WorkerProcessLauncher>(
      std::move(launcher_delegate), this);
}

PeerConnectionProcessHandler::~PeerConnectionProcessHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PeerConnectionProcessHandler::BindPeerSession(
    mojo::PendingReceiver<mojom::PeerSession> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (pc_process_control_.is_bound()) {
    pc_process_control_->BindPeerSession(std::move(receiver));
  } else {
    CHECK(!pending_session_receiver_) << "Only one peer session receiver can "
                                         "be buffered prior to connection.";
    pending_session_receiver_ = std::move(receiver);
  }
}

void PeerConnectionProcessHandler::Crash(const base::Location& location) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (launcher_) {
    launcher_->Crash(location);
  }
}

void PeerConnectionProcessHandler::OnChannelConnected(int32_t peer_pid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  pc_process_control_.reset();
  launcher_->GetRemoteAssociatedInterface(
      pc_process_control_.BindNewEndpointAndPassReceiver());

  if (pending_session_receiver_) {
    pc_process_control_->BindPeerSession(std::move(pending_session_receiver_));
  }
}

void PeerConnectionProcessHandler::OnPermanentError(int exit_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(ERROR) << "Peer Connection process permanent error, exit code: "
             << exit_code;
  OnWorkerProcessStopped();
}

void PeerConnectionProcessHandler::OnWorkerProcessStopped() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // We do not attempt to restart the Peer Connection process because WebRTC
  // connection state is lost on crash (requiring a client reconnect), and the
  // desktop pipe cannot be recovered. Thus, we tear down the handler and the
  // session.
  caller_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PeerConnectionProcessHandler::CloseSelf,
                                weak_factory_.GetWeakPtr()));
}

void PeerConnectionProcessHandler::OnAssociatedInterfaceRequest(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(ERROR) << "Unexpected associated interface request: " << interface_name;
}

void PeerConnectionProcessHandler::CloseSelf() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (stopped_callback_) {
    caller_task_runner_->PostTask(FROM_HERE,
                                  base::BindOnce(std::move(stopped_callback_),
                                                 weak_factory_.GetWeakPtr()));
  }
}

}  // namespace remoting
