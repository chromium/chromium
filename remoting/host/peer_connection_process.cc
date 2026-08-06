// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/peer_connection_process.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/process/process.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "ipc/ipc_channel_proxy.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace remoting {

PeerConnectionProcess::PeerConnectionProcess(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : caller_task_runner_(caller_task_runner),
      io_task_runner_(io_task_runner) {}

PeerConnectionProcess::~PeerConnectionProcess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool PeerConnectionProcess::Start(
    mojo::ScopedMessagePipeHandle channel_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!daemon_channel_);

  daemon_channel_ = IPC::ChannelProxy::Create(
      channel_handle.release(), IPC::Channel::MODE_CLIENT, this,
      io_task_runner_, caller_task_runner_);

  return true;
}

void PeerConnectionProcess::OnAssociatedInterfaceRequest(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (interface_name == mojom::PeerConnectionProcessControl::Name_) {
    if (control_receiver_.is_bound()) {
      LOG(FATAL) << "Receiver already bound for associated interface: "
                 << mojom::PeerConnectionProcessControl::Name_;
    }

    mojo::PendingAssociatedReceiver<mojom::PeerConnectionProcessControl>
        pending_receiver(std::move(handle));
    control_receiver_.Bind(std::move(pending_receiver));
    control_receiver_.set_disconnect_handler(base::BindOnce(
        &PeerConnectionProcess::OnChannelError, base::Unretained(this)));
  } else {
    LOG(FATAL) << "Received unexpected associated interface request: "
               << interface_name;
  }
}

void PeerConnectionProcess::BindPeerSession(
    mojo::PendingReceiver<mojom::PeerSession> session_receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  session_receiver_.Bind(std::move(session_receiver));
  session_receiver_.set_disconnect_handler(base::BindOnce(
      &PeerConnectionProcess::OnSessionDisconnected, base::Unretained(this)));
}

void PeerConnectionProcess::OnSessionDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(INFO)
      << "PeerSession dropped by Network process; shutting down PC process.";
  if (on_shutdown_for_testing_) {
    std::move(on_shutdown_for_testing_).Run();
    return;
  }
  base::Process::TerminateCurrentProcessImmediately(0);
}

void PeerConnectionProcess::OnChannelError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(ERROR) << "Daemon channel error in PC process. Terminating.";

  daemon_channel_.reset();
  control_receiver_.reset();
  session_receiver_.reset();

  caller_task_runner_ = nullptr;
  io_task_runner_ = nullptr;
}

}  // namespace remoting
