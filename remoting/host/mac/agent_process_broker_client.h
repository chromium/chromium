// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_MAC_AGENT_PROCESS_BROKER_CLIENT_H_
#define REMOTING_HOST_MAC_AGENT_PROCESS_BROKER_CLIENT_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "remoting/host/mojom/agent_process_broker.mojom.h"

namespace remoting {

class AgentProcessBrokerClient final {
 public:
  explicit AgentProcessBrokerClient(base::OnceClosure on_disconnected);
  AgentProcessBrokerClient(const AgentProcessBrokerClient&) = delete;
  AgentProcessBrokerClient& operator=(const AgentProcessBrokerClient&) = delete;
  ~AgentProcessBrokerClient();

  bool ConnectToServer();
  bool ConnectToServer(
      const mojo::NamedPlatformChannel::ServerName& server_name);
  void OnAgentProcessLaunched(mojom::AgentProcess* agent_process);

 private:
  void OnBrokerDisconnected();
  void OnAgentProcessRemoteDisconnected();
  void RunDisconnectedCallback();

  SEQUENCE_CHECKER(sequence_checker_);

  base::OnceClosure on_disconnected_;
  mojo::Remote<mojom::AgentProcessBroker> broker_remote_;
  std::unique_ptr<mojo::Receiver<mojom::AgentProcess>> agent_process_receiver_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_MAC_AGENT_PROCESS_BROKER_CLIENT_H_
