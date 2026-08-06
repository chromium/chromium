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
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "remoting/base/session_policies.h"
#include "remoting/host/ipc_desktop_environment.h"
#include "remoting/host/peer_session_impl.h"
#include "remoting/protocol/ice_config_fetcher.h"

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

void PeerConnectionProcess::Start(
    mojo::PendingRemote<mojom::PeerSessionEventHandler> event_handler,
    const std::string& client_jid,
    mojo::PendingRemote<mojom::DesktopSession> control_remote,
    mojo::PendingReceiver<mojom::DesktopSessionEvents> events_receiver,
    const DesktopEnvironmentOptions& desktop_environment_options) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!peer_session_);

  desktop_session_control_remote_ = std::move(control_remote);
  desktop_session_events_receiver_ = std::move(events_receiver);

  desktop_environment_factory_ = std::make_unique<IpcDesktopEnvironmentFactory>(
      caller_task_runner_, io_task_runner_,
      base::BindRepeating(&PeerConnectionProcess::GetDesktopSession,
                          base::Unretained(this)));

  // TODO(crbug.com/502281489): Hook up IceConfigFetcher with the Network
  // process over Mojo.
  PeerSessionImplFactory peer_session_factory(
      desktop_environment_factory_.get(),
      base::BindRepeating([]() -> std::unique_ptr<protocol::IceConfigFetcher> {
        return nullptr;
      }));

  peer_session_ = peer_session_factory.Create();
  if (!peer_session_) {
    LOG(ERROR) << "Failed to create PeerSession.";
    Shutdown(1);
    return;
  }

  event_handler_.Bind(std::move(event_handler));
  peer_session_->Start(event_handler_.get(), client_jid,
                       desktop_environment_options, /*extensions=*/{},
                       SessionPolicies(), SessionOptions());
}

void PeerConnectionProcess::DisconnectSession(
    protocol::ErrorCode error,
    const std::string& error_details,
    const SourceLocation& error_location) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (peer_session_) {
    peer_session_->DisconnectSession(error, error_details, error_location);
  }
}

void PeerConnectionProcess::OnSessionServicesClientConnected(
    mojo::PendingReceiver<mojom::ChromotingSessionServices> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (peer_session_) {
    peer_session_->OnSessionServicesClientConnected(std::move(receiver));
  }
}

void PeerConnectionProcess::OnChannelError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(ERROR) << "Daemon channel error in PC process. Terminating.";
  Shutdown(1);
}

void PeerConnectionProcess::OnSessionDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(INFO)
      << "PeerSession dropped by Network process; shutting down PC process.";
  Shutdown(0);
}

void PeerConnectionProcess::Shutdown(int exit_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (on_shutdown_for_testing_) {
    std::move(on_shutdown_for_testing_).Run();
    return;
  }
  base::Process::TerminateCurrentProcessImmediately(exit_code);
}

void PeerConnectionProcess::GetDesktopSession(
    mojo::PendingReceiver<mojom::DesktopSession> control_receiver,
    mojo::PendingRemote<mojom::DesktopSessionEvents> events_remote,
    mojom::DesktopSessionOptionsPtr options) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // `options` is safely ignored here because the DesktopSession handles were
  // pre-created in the network process and the options have already been
  // evaluated when requesting the session from the daemon process.
  if (desktop_session_control_remote_.is_valid()) {
    mojo::FusePipes(std::move(control_receiver),
                    std::move(desktop_session_control_remote_));
    mojo::FusePipes(std::move(desktop_session_events_receiver_),
                    std::move(events_remote));
  } else {
    LOG(WARNING) << "GetDesktopSession called when desktop session handles are "
                 << "already consumed or invalid.";
  }
}

}  // namespace remoting
