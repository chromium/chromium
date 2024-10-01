// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_MAC_AGENT_PROCESS_BROKER_H_
#define REMOTING_HOST_MAC_AGENT_PROCESS_BROKER_H_

#include <mach/message.h>

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/process/process_handle.h"
#include "base/sequence_checker.h"
#include "components/named_mojo_ipc_server/named_mojo_ipc_server.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "remoting/host/mojom/agent_process_broker.mojom.h"

namespace remoting {

class AgentProcessBroker final : public mojom::AgentProcessBroker {
 public:
  AgentProcessBroker();
  AgentProcessBroker(const AgentProcessBroker&) = delete;
  AgentProcessBroker& operator=(const AgentProcessBroker&) = delete;
  ~AgentProcessBroker() override;

  void Start();

  void OnAgentProcessLaunched(
      mojo::PendingRemote<mojom::AgentProcess> pending_agent_process) override;

 private:
  friend class AgentProcessBrokerTest;

  struct AgentProcess {
    AgentProcess(size_t reference_id,
                 base::ProcessId pid,
                 mojo::Remote<mojom::AgentProcess> remote,
                 bool is_root,
                 bool is_active);
    AgentProcess(AgentProcess&&);
    ~AgentProcess();

    AgentProcess& operator=(AgentProcess&&);

    void ResumeProcess();
    void SuspendProcess();

    std::string GetAgentProcessLogString(std::string_view state) const;

    size_t reference_id;  // For reverse lookup in `agent_processes_`.
    base::ProcessId pid;  // For logging only. Not for book keeping.
    mojo::Remote<mojom::AgentProcess> remote;
    bool is_root;
    bool is_active;
  };

  using Validator = base::RepeatingCallback<bool(
      const named_mojo_ipc_server::ConnectionInfo&)>;

  // Interface to allow tests to fake the root-ness/non-root-ness of a process,
  // since tests can't launch process as root.
  using IsRootProcessGetter = base::RepeatingCallback<bool(audit_token_t)>;

  AgentProcessBroker(const mojo::NamedPlatformChannel::ServerName& server_name,
                     Validator validator,
                     IsRootProcessGetter is_root_process);

  void OnAgentProcessDisconnected(size_t reference_id);
  void BrokerAgentProcesses();

  // Closes all processes except one. If there are active processes in
  // |processes|, then the one not being closed is guaranteed to be active.
  // Other than that, the process being left over is arbitrary.
  void TrimProcessList(std::vector<AgentProcess*>& processes);

  void set_on_agent_process_launched_for_testing(
      base::OnceClosure on_agent_process_launched) {
    on_agent_process_launched_ = std::move(on_agent_process_launched);
  }

  SEQUENCE_CHECKER(sequence_checker_);

  named_mojo_ipc_server::NamedMojoIpcServer<mojom::AgentProcessBroker> server_;
  IsRootProcessGetter is_root_process_;
  base::flat_map<size_t /* reference_id */, AgentProcess> agent_processes_;
  // We use our own reference ID for book keeping. While unlikely, the OS is
  // free to immediately reuse the PID after a process has exited. This might
  // cause race conditions since the disconnect callback might be called after
  // the new process is spawned.
  size_t next_reference_id_ = 0u;
  base::OnceClosure on_agent_process_launched_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_MAC_AGENT_PROCESS_BROKER_H_
