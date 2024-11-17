// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mac/agent_process_broker_client.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "components/named_mojo_ipc_server/named_mojo_ipc_server_client_util.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "remoting/host/ipc_constants.h"
#include "remoting/host/mojom/agent_process_broker.mojom.h"

namespace remoting {

AgentProcessBrokerClient::AgentProcessBrokerClient(
    base::OnceClosure on_disconnected)
    : on_disconnected_(std::move(on_disconnected)) {}

AgentProcessBrokerClient::~AgentProcessBrokerClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool AgentProcessBrokerClient::ConnectToServer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ConnectToServer(GetAgentProcessBrokerServerName());
}

bool AgentProcessBrokerClient::ConnectToServer(
    const mojo::NamedPlatformChannel::ServerName& server_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto endpoint = named_mojo_ipc_server::ConnectToServer(server_name);
  if (!endpoint.is_valid()) {
    LOG(WARNING) << "Cannot connect to IPC through server name " << server_name
                 << ". Endpoint is invalid.";
    // This may happen if the broker process is somehow launched after the host
    // process. The host process will exit and the host service process will
    // relaunch the host process.
    return false;
  }
  auto invitation = mojo::IncomingInvitation::Accept(std::move(endpoint));
  auto message_pipe =
      invitation.ExtractMessagePipe(kAgentProcessBrokerMessagePipeId);
  mojo::PendingRemote<mojom::AgentProcessBroker> pending_remote(
      std::move(message_pipe), /* version= */ 0);
  if (!pending_remote.is_valid()) {
    LOG(WARNING) << "Invalid message pipe.";
    return false;
  }
  broker_remote_.Bind(std::move(pending_remote));
  broker_remote_.set_disconnect_handler(base::BindOnce(
      &AgentProcessBrokerClient::OnBrokerDisconnected, base::Unretained(this)));
  return true;
}

void AgentProcessBrokerClient::OnAgentProcessLaunched(
    mojom::AgentProcess* agent_process) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(broker_remote_.is_bound());
  DCHECK(!agent_process_receiver_);
  agent_process_receiver_ =
      std::make_unique<mojo::Receiver<mojom::AgentProcess>>(agent_process);
  broker_remote_->OnAgentProcessLaunched(
      agent_process_receiver_->BindNewPipeAndPassRemote());
  agent_process_receiver_->set_disconnect_handler(base::BindOnce(
      &AgentProcessBrokerClient::OnAgentProcessRemoteDisconnected,
      base::Unretained(this)));
}

void AgentProcessBrokerClient::OnBrokerDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(ERROR) << "Broker process has disconnected.";
  // This may happen if the broker process has crashed. The host process will
  // exit and the host service process will relaunch it.
  RunDisconnectedCallback();
}

void AgentProcessBrokerClient::OnAgentProcessRemoteDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(WARNING) << "Agent process remote has disconnected.";
  RunDisconnectedCallback();
}

void AgentProcessBrokerClient::RunDisconnectedCallback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (on_disconnected_) {
    std::move(on_disconnected_).Run();
  }
}

}  // namespace remoting
