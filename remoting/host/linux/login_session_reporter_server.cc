// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/login_session_reporter_server.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "components/named_mojo_ipc_server/endpoint_options.h"
#include "remoting/host/ipc_constants.h"
#include "remoting/host/mojo_caller_security_checker.h"

namespace remoting {
namespace {

named_mojo_ipc_server::EndpointOptions CreateEndpointOptions() {
  named_mojo_ipc_server::EndpointOptions options;
  options.server_name = GetLoginSessionReporterServerName();
  options.message_pipe_id = kLoginSessionReporterMessagePipeId;
  options.require_same_peer_user = false;
  return options;
}

}  // namespace

LoginSessionReporterServer::LoginSessionReporterServer(
    mojom::LoginSessionObserver* session_reporter)
    : session_reporter_(session_reporter),
      ipc_server_(CreateEndpointOptions(),
                  base::BindRepeating(IsTrustedMojoEndpoint)
                      .Then(base::BindRepeating(
                          [](mojom::LoginSessionObserver* session_reporter,
                             bool is_valid) {
                            return is_valid ? session_reporter : nullptr;
                          },
                          this))) {}

LoginSessionReporterServer::~LoginSessionReporterServer() = default;

void LoginSessionReporterServer::StartServer() {
  ipc_server_.StartServer();
}

void LoginSessionReporterServer::StopServer() {
  ipc_server_.StopServer();
}

void LoginSessionReporterServer::OnLoginSessionCreated(
    mojom::LoginSessionInfoPtr session_info) {
  session_reporter_->OnLoginSessionCreated(std::move(session_info));
  // Terminate the session reporter process.
  ipc_server_.Close(ipc_server_.current_receiver());
}

}  // namespace remoting
