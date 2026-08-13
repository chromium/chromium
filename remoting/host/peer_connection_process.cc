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
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "remoting/base/session_policies.h"
#include "remoting/host/crash_process.h"
#include "remoting/host/ipc_desktop_environment.h"
#include "remoting/host/peer_session_impl.h"
#include "remoting/protocol/ice_config_fetcher.h"
#include "remoting/protocol/transport.h"
#include "remoting/signaling/jingle_data_structures.h"

namespace remoting {

namespace {

class MojoIceConfigFetcher final : public protocol::IceConfigFetcher {
 public:
  explicit MojoIceConfigFetcher(
      mojo::SharedRemote<mojom::IceConfigFetcher> remote)
      : remote_(std::move(remote)) {}

  ~MojoIceConfigFetcher() override = default;

  void GetIceConfig(OnIceConfigCallback callback) override {
    if (!remote_) {
      std::move(callback).Run(std::nullopt);
      return;
    }
    remote_->GetIceConfig(base::BindOnce(
        [](OnIceConfigCallback callback,
           std::optional<protocol::IceConfig> ice_config) {
          std::move(callback).Run(std::move(ice_config));
        },
        std::move(callback)));
  }

 private:
  mojo::SharedRemote<mojom::IceConfigFetcher> remote_;
};

}  // namespace

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
      std::move(channel_handle), IPC::Channel::MODE_CLIENT, this,
      io_task_runner_, caller_task_runner_);

  return true;
}

void PeerConnectionProcess::CrashProcess(const std::string& function_name,
                                         const std::string& file_name,
                                         int line_number) {
  remoting::CrashProcess(function_name, file_name, line_number);
}

void PeerConnectionProcess::OnAssociatedInterfaceRequest(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (interface_name == mojom::WorkerProcessControl::Name_) {
    if (worker_process_control_.is_bound()) {
      LOG(FATAL) << "Receiver already bound for associated interface: "
                 << mojom::WorkerProcessControl::Name_;
    }

    mojo::PendingAssociatedReceiver<mojom::WorkerProcessControl>
        pending_receiver(std::move(handle));
    worker_process_control_.Bind(std::move(pending_receiver));
  } else if (interface_name == mojom::PeerConnectionProcessControl::Name_) {
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
    const std::string& client_jid,
    mojo::PendingRemote<mojom::PeerSessionEventHandler> event_handler,
    mojo::PendingRemote<mojom::DesktopSession> desktop_control,
    mojo::PendingReceiver<mojom::DesktopSessionEvents> desktop_events_receiver,
    mojo::PendingRemote<mojom::IceConfigFetcher> ice_config_fetcher,
    mojo::PendingRemote<mojom::PairingRequester> pairing_requester,
    const DesktopEnvironmentOptions& desktop_environment_options,
    const SessionPolicies& session_policies,
    const SessionOptions& session_options) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!peer_session_);

  desktop_session_control_remote_ = std::move(desktop_control);
  desktop_session_events_receiver_ = std::move(desktop_events_receiver);

  desktop_environment_factory_ = std::make_unique<IpcDesktopEnvironmentFactory>(
      caller_task_runner_, io_task_runner_,
      base::BindRepeating(&PeerConnectionProcess::GetDesktopSession,
                          base::Unretained(this)));

  PeerSessionImplFactory::GetIceConfigFetcherCallback get_ice_config_fetcher_cb;
  if (ice_config_fetcher) {
    // Uses SharedRemote because `get_ice_config_fetcher_cb` is a repeating
    // callback that creates an IceConfigFetcher for each connection, but only a
    // single IceConfigFetcher pending remote is received from the Network
    // process over IPC. SharedRemote allows multiple IceConfigFetcher instances
    // to share the same underlying Mojo channel safely.
    mojo::SharedRemote<mojom::IceConfigFetcher> shared_remote(
        std::move(ice_config_fetcher), caller_task_runner_);
    get_ice_config_fetcher_cb = base::BindRepeating(
        [](mojo::SharedRemote<mojom::IceConfigFetcher> shared_remote)
            -> std::unique_ptr<protocol::IceConfigFetcher> {
          return std::make_unique<MojoIceConfigFetcher>(
              std::move(shared_remote));
        },
        std::move(shared_remote));
  } else {
    get_ice_config_fetcher_cb = base::BindRepeating(
        []() -> std::unique_ptr<protocol::IceConfigFetcher> {
          return nullptr;
        });
  }

  PeerSessionImplFactory::RequestPairingCallback request_pairing_cb;
  if (pairing_requester) {
    pairing_requester_remote_.Bind(std::move(pairing_requester));
    request_pairing_cb = base::BindRepeating(
        &PeerConnectionProcess::RequestPairing, base::Unretained(this));
  }

  PeerSessionImplFactory peer_session_factory(
      desktop_environment_factory_.get(), get_ice_config_fetcher_cb,
      std::move(request_pairing_cb));

  peer_session_ = peer_session_factory.Create();
  if (!peer_session_) {
    LOG(ERROR) << "Failed to create PeerSession.";
    Shutdown(1);
    return;
  }

  event_handler_.Bind(std::move(event_handler));
  peer_session_->Start(event_handler_.get(), client_jid,
                       desktop_environment_options, /*extensions=*/{},
                       session_policies, session_options);

  if (pending_start_transport_) {
    auto pending = std::move(*pending_start_transport_);
    pending_start_transport_.reset();
    StartTransport(pending.auth_key,
                   std::move(pending.transport_event_handler));
  }

  auto pending_transport_infos = std::move(pending_transport_infos_);
  for (const auto& transport_info : pending_transport_infos) {
    ProcessTransportInfo(transport_info);
  }
}

void PeerConnectionProcess::StartTransport(
    const std::string& auth_key,
    mojo::PendingRemote<mojom::TransportEventHandler> transport_event_handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!peer_session_ || !peer_session_->transport()) {
    pending_start_transport_ =
        PendingStartTransport{auth_key, std::move(transport_event_handler)};
    return;
  }
  transport_event_handler_.reset();
  if (transport_event_handler) {
    transport_event_handler_.Bind(std::move(transport_event_handler));
  }
  peer_session_->transport()->Start(
      auth_key, base::BindRepeating(&PeerConnectionProcess::OnSendTransportInfo,
                                    weak_factory_.GetWeakPtr()));
}

void PeerConnectionProcess::ProcessTransportInfo(
    const JingleTransportInfo& transport_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!peer_session_ || !peer_session_->transport()) {
    pending_transport_infos_.push_back(transport_info);
    return;
  }
  peer_session_->transport()->ProcessTransportInfo(transport_info);
}

void PeerConnectionProcess::OnSendTransportInfo(
    std::unique_ptr<JingleTransportInfo> transport_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!transport_info) {
    return;
  }
  if (transport_event_handler_.is_bound()) {
    transport_event_handler_->SendTransportInfo(std::move(*transport_info));
  }
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

void PeerConnectionProcess::OnChannelError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(ERROR) << "Daemon channel error in PC process. Terminating.";
  Shutdown(1);
}

void PeerConnectionProcess::OnSessionDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(INFO)
      << "PeerSession dropped by Network process; shutting down PC process.";
  pairing_requester_remote_.reset();
  transport_event_handler_.reset();
  pending_start_transport_.reset();
  pending_transport_infos_.clear();
  Shutdown(0);
}

void PeerConnectionProcess::RequestPairing(
    const std::string& client_name,
    PeerSessionFactory::RequestPairingResponseCallback response_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pairing_requester_remote_.is_bound()) {
    std::move(response_cb).Run(std::nullopt);
    return;
  }
  pairing_requester_remote_->RequestPairing(client_name,
                                            std::move(response_cb));
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
