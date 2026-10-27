// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromoting_host_services_client.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/environment.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/named_mojo_ipc_server/endpoint_options.h"
#include "components/named_mojo_ipc_server/named_mojo_ipc_server_client_util.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/system/invitation.h"
#include "remoting/base/constants.h"
#include "remoting/host/base/switches.h"
#include "remoting/host/ipc_constants.h"
#include "remoting/host/mojom/chromoting_host_services.mojom.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace remoting {

namespace {

std::optional<protocol::TerminalControl::AgentInfo::AgentState>
ParseAgentStateString(std::string_view state_str) {
  using AgentInfo = protocol::TerminalControl::AgentInfo;
  static constexpr auto kAgentStateMap =
      base::MakeFixedFlatMap<std::string_view, AgentInfo::AgentState>({
          {"authenticating", AgentInfo::AUTHENTICATING},
          {"error", protocol::TerminalControl_AgentInfo_AgentState_ERROR},
          {"idle", AgentInfo::IDLE},
          {"initializing", AgentInfo::INITIALIZING},
          {"reviewing", AgentInfo::REVIEWING},
          {"tool_use", AgentInfo::TOOL_USE},
          {"working", AgentInfo::WORKING},
      });
  auto it = kAgentStateMap.find(state_str);
  return it != kAgentStateMap.end() ? std::make_optional(it->second)
                                    : std::nullopt;
}

mojo::PendingRemote<mojom::ChromotingHostServices> ConnectToServer(
    const std::vector<mojo::NamedPlatformChannel::ServerName>& server_names) {
  for (const auto& server_name : server_names) {
    mojo::PlatformChannelEndpoint endpoint;
#if BUILDFLAG(IS_WIN)
    mojo::NamedPlatformChannel::Options options;
    options.server_name = server_name;
    options.verify_server_privilege = true;
    endpoint = named_mojo_ipc_server::ConnectToServer(options);
#else
    endpoint = named_mojo_ipc_server::ConnectToServer(server_name);
#endif
    if (!endpoint.is_valid()) {
      VLOG(1) << "Cannot connect to IPC through server name " << server_name
              << ". Endpoint is invalid.";
      continue;
    }
#if BUILDFLAG(IS_WIN)
    DWORD peer_session_id;
    if (!GetNamedPipeServerSessionId(
            endpoint.platform_handle().GetHandle().get(), &peer_session_id)) {
      PLOG(ERROR) << "GetNamedPipeServerSessionId failed";
      continue;
    }
    // '0' (default) corresponds to the session the network process runs in.
    if (peer_session_id != 0) {
      LOG(ERROR)
          << "Cannot establish connection with IPC server running in session: "
          << peer_session_id;
      continue;
    }
#endif
    auto invitation = mojo::IncomingInvitation::Accept(std::move(endpoint));
    auto message_pipe =
        invitation.ExtractMessagePipe(kChromotingHostServicesMessagePipeId);
    return mojo::PendingRemote<mojom::ChromotingHostServices>(
        std::move(message_pipe), /* version= */ 0);
  }
  LOG(WARNING) << "Failed to connect to any ChromotingHostServices IPC server: "
               << base::JoinString(
                      server_names,
                      mojo::NamedPlatformChannel::ServerNameFromUTF8(", "));
  return {};
}

}  // namespace

// static
std::optional<ChromotingHostServicesClient::AgentStatusArgs>
ChromotingHostServicesClient::ParseAgentStatusArgs(
    const base::CommandLine* command_line) {
  if (!command_line || !command_line->HasSwitch(kTerminalIdSwitchName) ||
      (!command_line->HasSwitch(kAgentStatusSwitchName) &&
       !command_line->HasSwitch(kTerminalTitleSwitchName))) {
    return std::nullopt;
  }

  std::string terminal_id_str =
      command_line->GetSwitchValueASCII(kTerminalIdSwitchName);
  int terminal_id;
  if (!base::StringToInt(terminal_id_str, &terminal_id) || terminal_id < 0) {
    LOG(ERROR) << "Invalid --" << kTerminalIdSwitchName << ": "
               << terminal_id_str;
    return std::nullopt;
  }

  std::optional<protocol::TerminalControl::AgentInfo::AgentState> state;
  if (command_line->HasSwitch(kAgentStatusSwitchName)) {
    std::string status_arg =
        command_line->GetSwitchValueASCII(kAgentStatusSwitchName);
    state = ParseAgentStateString(status_arg);
    if (!state.has_value()) {
      LOG(ERROR) << "Unknown agent status: " << status_arg;
      return std::nullopt;
    }
  }

  protocol::TerminalControl::AgentInfo status;
  if (state.has_value()) {
    status.set_agent_state(*state);
  }
  if (command_line->HasSwitch(kTerminalTitleSwitchName)) {
    status.set_conversation_title(
        command_line->GetSwitchValueUTF8(kTerminalTitleSwitchName));
  }
  return AgentStatusArgs{.terminal_id = terminal_id,
                         .status = std::move(status)};
}

ChromotingHostServicesClient::ChromotingHostServicesClient()
    : ChromotingHostServicesClient(
#if BUILDFLAG(IS_LINUX)
          std::vector{GetChromotingHostServicesServerName(),
                      GetLegacyChromotingHostServicesServerName()}
#else
          std::vector{GetChromotingHostServicesServerName()}
#endif
      ) {
}

ChromotingHostServicesClient::ChromotingHostServicesClient(
    const mojo::NamedPlatformChannel::ServerName& server_name)
    : ChromotingHostServicesClient(std::vector{server_name}) {}

ChromotingHostServicesClient::ChromotingHostServicesClient(
    const std::vector<mojo::NamedPlatformChannel::ServerName>& server_names)
    : ChromotingHostServicesClient(
          base::Environment::Create(),
          base::BindRepeating(&ConnectToServer, server_names)) {}

ChromotingHostServicesClient::ChromotingHostServicesClient(
    std::unique_ptr<base::Environment> environment,
    ConnectToServerCallback connect_to_server)
    : environment_(std::move(environment)),
      connect_to_server_(std::move(connect_to_server)) {}

ChromotingHostServicesClient::~ChromotingHostServicesClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

mojom::ChromotingSessionServices*
ChromotingHostServicesClient::GetSessionServices() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!const_cast<ChromotingHostServicesClient*>(this)
           ->EnsureSessionServicesBinding()) {
    return nullptr;
  }
  return session_services_remote_.get();
}

void ChromotingHostServicesClient::set_disconnect_handler(
    base::OnceClosure disconnect_handler) {
  disconnect_handler_ = std::move(disconnect_handler);
}

bool ChromotingHostServicesClient::EnsureConnection() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (remote_.is_bound()) {
    return true;
  }

  auto pending_remote = connect_to_server_.Run();
  if (!pending_remote.is_valid()) {
    LOG(WARNING) << "Invalid message pipe.";
    return false;
  }
  remote_.Bind(std::move(pending_remote));
  remote_.set_disconnect_handler(base::BindOnce(
      &ChromotingHostServicesClient::OnDisconnected, base::Unretained(this)));
  return true;
}

bool ChromotingHostServicesClient::EnsureSessionServicesBinding() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (session_services_remote_.is_bound()) {
    return true;
  }
#if BUILDFLAG(IS_LINUX)
  if (!environment_->HasVar(kChromeRemoteDesktopSessionEnvVar)) {
    LOG(WARNING) << "Current desktop environment is not remotable.";
    return false;
  }
#endif
  if (!EnsureConnection()) {
    return false;
  }
  remote_->BindSessionServices(
      session_services_remote_.BindNewPipeAndPassReceiver());
  session_services_remote_.set_disconnect_handler(
      base::BindOnce(&ChromotingHostServicesClient::OnSessionDisconnected,
                     base::Unretained(this)));
  return true;
}

void ChromotingHostServicesClient::OnDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  remote_.reset();

  if (disconnect_handler_) {
    std::move(disconnect_handler_).Run();
  }
}

void ChromotingHostServicesClient::OnSessionDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  session_services_remote_.reset();

  if (disconnect_handler_) {
    std::move(disconnect_handler_).Run();
  }
}

}  // namespace remoting
