// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromoting_host_services_server.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "build/buildflag.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/system/isolated_connection.h"
#include "remoting/host/ipc_constants.h"
#include "remoting/host/mojo_caller_security_checker.h"
#include "remoting/host/mojom/chromoting_host_services.mojom.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/strcat_win.h"
#include "base/win/win_util.h"
#endif

namespace remoting {
namespace {

named_mojo_ipc_server::EndpointOptions CreateEndpointOptions(
    const mojo::NamedPlatformChannel::ServerName& server_name) {
  named_mojo_ipc_server::EndpointOptions options;
  options.server_name = server_name;
  options.message_pipe_id = kChromotingHostServicesMessagePipeId;
#if BUILDFLAG(IS_WIN)
  // Create a named pipe owned by the current user which is available to all
  // authenticated users.
  std::wstring user_sid;
  if (!base::win::GetUserSidString(&user_sid)) {
    LOG(ERROR) << "Failed to get user SID string.";
    return {};
  }
  options.security_descriptor =
      base::StrCat({L"O:", user_sid, L"G:", user_sid, L"D:(A;;GA;;;AU)"});
#endif
  return options;
}

}  // namespace

ChromotingHostServicesServer::ChromotingHostServicesServer(
    BindChromotingHostServicesCallback bind_chromoting_host_services)
    : ChromotingHostServicesServer(GetChromotingHostServicesServerName(),
                                   base::BindRepeating(IsTrustedMojoEndpoint),
                                   std::move(bind_chromoting_host_services)) {}

ChromotingHostServicesServer::ChromotingHostServicesServer(
    const mojo::NamedPlatformChannel::ServerName& server_name,
    Validator validator,
    BindChromotingHostServicesCallback bind_chromoting_host_services)
    : message_pipe_server_(
          CreateEndpointOptions(server_name),
          validator.Then(base::BindRepeating([](bool is_valid) {
            return named_mojo_ipc_server::NamedMojoMessagePipeServer::
                ValidationResult{
                    .is_valid = is_valid,
                    .context = nullptr,
                };
          })),
          base::BindRepeating(&ChromotingHostServicesServer::OnMessagePipeReady,
                              base::Unretained(this))),
      bind_chromoting_host_services_(bind_chromoting_host_services) {}

ChromotingHostServicesServer::~ChromotingHostServicesServer() = default;

void ChromotingHostServicesServer::StartServer() {
  message_pipe_server_.StartServer();
}

void ChromotingHostServicesServer::StopServer() {
  message_pipe_server_.StopServer();
}

void ChromotingHostServicesServer::OnMessagePipeReady(
    mojo::ScopedMessagePipeHandle message_pipe,
    std::unique_ptr<named_mojo_ipc_server::ConnectionInfo> connection_info,
    void* context,
    std::unique_ptr<mojo::IsolatedConnection> connection) {
  DCHECK(!context) << "ChromotingHostServicesServer provides no context";
  DCHECK(!connection) << "ChromotingHostServices connections are not isolated";
  bind_chromoting_host_services_.Run(
      mojo::PendingReceiver<mojom::ChromotingHostServices>(
          std::move(message_pipe)),
      connection_info->pid);
}

}  // namespace remoting
