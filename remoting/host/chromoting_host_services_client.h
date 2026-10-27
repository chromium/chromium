// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMOTING_HOST_SERVICES_CLIENT_H_
#define REMOTING_HOST_CHROMOTING_HOST_SERVICES_CLIENT_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "remoting/host/chromoting_host_services_provider.h"
#include "remoting/host/mojom/chromoting_host_services.mojom.h"
#include "remoting/proto/terminal.pb.h"

namespace base {
class CommandLine;
class Environment;
}  // namespace base

namespace remoting {

// Maintains connection to a ChromotingHostServices server, and provides the
// ChromotingHostServices interface. Note that each process should have only one
// ChromotingHostServicesClient instance. Making multiple connections to the
// ChromotingHostServices server is not supported.
class ChromotingHostServicesClient final
    : public ChromotingHostServicesProvider {
 public:
  // The arguments to a SetAgentStatus() call, as parsed from the command line.
  struct AgentStatusArgs {
    int32_t terminal_id;
    protocol::TerminalControl::AgentInfo status;
  };

  // Parses `--agent-status`, `--terminal-title`, and `--terminal-id` switches.
  // Requires `--terminal-id` and at least one of `--agent-status` or
  // `--terminal-title`. Returns `std::nullopt` if the arguments are invalid.
  static std::optional<AgentStatusArgs> ParseAgentStatusArgs(
      const base::CommandLine* command_line);

  ChromotingHostServicesClient();
  explicit ChromotingHostServicesClient(
      const mojo::NamedPlatformChannel::ServerName& server_name);
  explicit ChromotingHostServicesClient(
      const std::vector<mojo::NamedPlatformChannel::ServerName>& server_names);
  ChromotingHostServicesClient(const ChromotingHostServicesClient&) = delete;
  ChromotingHostServicesClient& operator=(const ChromotingHostServicesClient&) =
      delete;
  ~ChromotingHostServicesClient() override;

  // Gets the ChromotingSessionServices. Always null-check before using it, as
  // nullptr will be returned if the connection could not be established.
  // Note that when the session is not remoted, you will still get a callable
  // interface, but all outgoing IPCs will be silently dropped, and any pending
  // receivers/remotes/message pipes sent will be closed.
  mojom::ChromotingSessionServices* GetSessionServices() const override;

  void set_disconnect_handler(base::OnceClosure disconnect_handler) override;

 private:
  friend class ChromotingHostServicesClientTest;

  using ConnectToServerCallback = base::RepeatingCallback<
      mojo::PendingRemote<mojom::ChromotingHostServices>()>;

  ChromotingHostServicesClient(std::unique_ptr<base::Environment> environment,
                               ConnectToServerCallback connect_to_server);

  // Attempts to connect to the IPC server if the connection has not been
  // established. Returns a boolean indicating whether there is a valid IPC
  // connection to the chromoting host.
  bool EnsureConnection();

  bool EnsureSessionServicesBinding();

  void OnDisconnected();
  void OnSessionDisconnected();

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<base::Environment> environment_;
  ConnectToServerCallback connect_to_server_;
  mojo::Remote<mojom::ChromotingHostServices> remote_
      GUARDED_BY_CONTEXT(sequence_checker_);
  mojo::Remote<mojom::ChromotingSessionServices> session_services_remote_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::OnceClosure disconnect_handler_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMOTING_HOST_SERVICES_CLIENT_H_
