// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mac/agent_process_broker.h"

#include <mach/message.h>
#include <stddef.h>
#include <sys/sysctl.h>

#include <algorithm>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/process/process_handle.h"
#include "base/sequence_checker.h"
#include "base/strings/stringprintf.h"
#include "components/named_mojo_ipc_server/connection_info.h"
#include "components/named_mojo_ipc_server/endpoint_options.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "remoting/base/logging.h"
#include "remoting/host/ipc_constants.h"
#include "remoting/host/mac/agent_process_broker_constants.h"
#include "remoting/host/mojo_caller_security_checker.h"
#include "remoting/host/mojom/agent_process_broker.mojom.h"
#include "remoting/host/mojom/remoting_host.mojom.h"

namespace remoting {

namespace {

bool IsRootProcess(audit_token_t audit_token) {
  return audit_token_to_ruid(audit_token) == 0;
}

}  // namespace

AgentProcessBroker::AgentProcess::AgentProcess(
    size_t reference_id,
    base::ProcessId pid,
    mojo::Remote<mojom::AgentProcess> agent_process_remote,
    mojo::Remote<mojom::RemotingHostControl> remoting_host_control_remote,
    bool is_root,
    bool is_active)
    : reference_id(reference_id),
      pid(pid),
      agent_process_remote(std::move(agent_process_remote)),
      remoting_host_control_remote(std::move(remoting_host_control_remote)),
      is_root(is_root),
      is_active(is_active) {}
AgentProcessBroker::AgentProcess::AgentProcess(AgentProcess&&) = default;
AgentProcessBroker::AgentProcess::~AgentProcess() = default;
AgentProcessBroker::AgentProcess& AgentProcessBroker::AgentProcess::operator=(
    AgentProcess&&) = default;

void AgentProcessBroker::AgentProcess::ResumeProcess() {
  if (is_active) {
    return;
  }
  agent_process_remote->ResumeProcess();
  is_active = true;
  HOST_LOG << GetAgentProcessLogString("resumed");
}

void AgentProcessBroker::AgentProcess::SuspendProcess() {
  if (!is_active) {
    return;
  }
  agent_process_remote->SuspendProcess();
  is_active = false;
  HOST_LOG << GetAgentProcessLogString("suspended");
}

void AgentProcessBroker::AgentProcess::TerminateProcess() {
  agent_process_remote.ResetWithReason(
      kTerminateAgentProcessBrokerReason,
      "Agent process requested to be terminated by the broker.");
  HOST_LOG << GetAgentProcessLogString("terminated");
}

std::string AgentProcessBroker::AgentProcess::GetAgentProcessLogString(
    std::string_view state) const {
  return base::StringPrintf("Agent process %d (PID: %d, %s) %s", reference_id,
                            pid, is_root ? "root" : "user", state.data());
}

AgentProcessBroker::AgentProcessBroker()
    : AgentProcessBroker(GetAgentProcessBrokerServerName(),
                         base::BindRepeating(IsTrustedMojoEndpoint),
                         base::BindRepeating(IsRootProcess)) {}

AgentProcessBroker::AgentProcessBroker(
    const mojo::NamedPlatformChannel::ServerName& server_name,
    Validator validator,
    IsRootProcessGetter is_root_process)
    : server_(named_mojo_ipc_server::EndpointOptions(
                  server_name,
                  kAgentProcessBrokerMessagePipeId),
              validator.Then(base::BindRepeating(
                  [](mojom::AgentProcessBroker* interface, bool is_valid) {
                    return is_valid ? interface : nullptr;
                  },
                  this))),
      is_root_process_(std::move(is_root_process)) {
  chromoting_host_services_server_ =
      std::make_unique<ChromotingHostServicesServer>(
          base::BindRepeating(&AgentProcessBroker::BindChromotingHostServices,
                              base::Unretained(this)));
}

AgentProcessBroker::~AgentProcessBroker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void AgentProcessBroker::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  server_.StartServer();
  chromoting_host_services_server_->StartServer();
  HOST_LOG << "Agent process broker has started.";
}

void AgentProcessBroker::OnAgentProcessLaunched(
    mojo::PendingRemote<mojom::AgentProcess> pending_agent_process) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto& connection_info = server_.current_connection_info();
  bool is_root = is_root_process_.Run(connection_info.audit_token);
  mojo::Remote<mojom::AgentProcess> process_remote{
      std::move(pending_agent_process)};
  process_remote.set_disconnect_handler(
      base::BindOnce(&AgentProcessBroker::OnAgentProcessDisconnected,
                     base::Unretained(this), next_reference_id_));
  mojo::Remote<mojom::RemotingHostControl> remoting_host_control_remote;
  process_remote->BindRemotingHostControl(
      remoting_host_control_remote.BindNewPipeAndPassReceiver());
  auto result = agent_processes_.emplace(
      next_reference_id_,
      AgentProcess{next_reference_id_, connection_info.pid,
                   std::move(process_remote),
                   std::move(remoting_host_control_remote), is_root,
                   /* is_active= */ false});
  DCHECK(result.second);  // Assert success.
  HOST_LOG << result.first->second.GetAgentProcessLogString("launched");
  next_reference_id_++;
  BrokerAgentProcesses();
  if (on_agent_process_launched_) {
    std::move(on_agent_process_launched_).Run();
  }
}

void AgentProcessBroker::BindChromotingHostServices(
    mojo::PendingReceiver<mojom::ChromotingHostServices> receiver,
    base::ProcessId peer_pid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto active_iter = std::ranges::find_if(
      agent_processes_,
      [](const auto& process) { return process.second.is_active; });
  if (active_iter == std::ranges::end(agent_processes_)) {
    LOG(WARNING) << "Binding rejected. No active agent process is found.";
    return;
  }
  AgentProcess& process = active_iter->second;
  process.remoting_host_control_remote->BindChromotingHostServices(
      std::move(receiver), peer_pid);
  HOST_LOG << process.GetAgentProcessLogString(base::StringPrintf(
      "bound ChromotingHostServices for peer PID %d", peer_pid));
}

void AgentProcessBroker::OnAgentProcessDisconnected(size_t reference_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = agent_processes_.find(reference_id);
  if (it != agent_processes_.end()) {
    HOST_LOG << it->second.GetAgentProcessLogString("disconnected");
    agent_processes_.erase(it);
    BrokerAgentProcesses();
  } else {
    LOG(WARNING) << "Agent process ID " << reference_id << " not found.";
  }
}

void AgentProcessBroker::BrokerAgentProcesses() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Goal: There can be up to one root process and one user process running.
  // When both the root process and the user process are running, the root
  // process must be suspended to give way to the user process.
  std::vector<AgentProcess*> root_processes;
  std::vector<AgentProcess*> user_processes;
  for (auto& pair : agent_processes_) {
    if (pair.second.is_root) {
      root_processes.push_back(&pair.second);
    } else {
      user_processes.push_back(&pair.second);
    }
  }
  TrimProcessList(root_processes);
  TrimProcessList(user_processes);
  if (!user_processes.empty()) {
    // Suspend the root process if it's running, then resume the user process,
    // since the latter has higher priority.
    if (!root_processes.empty()) {
      root_processes.front()->SuspendProcess();
    }
    user_processes.front()->ResumeProcess();
    return;
  }
  if (!root_processes.empty()) {
    // There are no user processes, so resume the root process.
    root_processes.front()->ResumeProcess();
  }
}

void AgentProcessBroker::TrimProcessList(
    std::vector<AgentProcess*>& processes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (processes.size() < 2) {
    return;
  }
  // Sort processes. Active processes come first so that they are the last to be
  // removed.
  std::sort(processes.begin(), processes.end(),
            [](const AgentProcess* p1, const AgentProcess* p2) {
              return p1->is_active && !p2->is_active;
            });
  while (processes.size() > 1) {
    AgentProcess* process = processes.back();
    process->TerminateProcess();
    // Note: this will invalidate the storage that `processes.back()` references
    // to.
    agent_processes_.erase(process->reference_id);
    processes.pop_back();
  }
}

}  // namespace remoting
