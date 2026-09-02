// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/login_session_server.h"

#include <systemd/sd-login.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/free_deleter.h"
#include "base/posix/safe_strerror.h"
#include "components/named_mojo_ipc_server/connection_info.h"
#include "components/named_mojo_ipc_server/endpoint_options.h"
#include "remoting/base/logging.h"
#include "remoting/host/ipc_constants.h"
#include "remoting/host/mojo_caller_security_checker.h"

namespace remoting {
namespace {

named_mojo_ipc_server::EndpointOptions CreateEndpointOptions() {
  named_mojo_ipc_server::EndpointOptions options;
  options.server_name = GetLoginSessionServerName();
  options.message_pipe_id = kLoginSessionServerMessagePipeId;
  options.require_same_peer_user = false;
  return options;
}

bool IsGraphicalSession(const char* session_id) {
  char* raw_type = nullptr;
  int ret = sd_session_get_type(session_id, &raw_type);
  if (ret < 0) {
    LOG(ERROR) << "Failed to get session type for session " << session_id
               << ": " << base::safe_strerror(-ret);
    return false;
  }
  if (!raw_type) {
    LOG(ERROR) << "Failed to get session type for session " << session_id
               << ": null session type returned";
    return false;
  }
  std::unique_ptr<char, base::FreeDeleter> type(raw_type);
  const std::string_view session_type = type.get();
  return session_type == "x11" || session_type == "wayland";
}

std::string GetGraphicalSessionIdForPid(base::ProcessId pid) {
  char* raw_session_id = nullptr;
  int ret = sd_pid_get_session(pid, &raw_session_id);
  if (ret < 0) {
    // If the process is not directly in a session scope (e.g. running under
    // the systemd user manager), sd_pid_get_session() returns -ENODATA.
    // For modern GDM, each user can only have one graphical session, so it
    // falls back to the user's primary graphical display session.
    HOST_LOG << "Failed to get session ID for PID " << pid << ": "
             << base::safe_strerror(-ret)
             << ". Falling back to the user's display session.";
    uid_t uid;
    ret = sd_pid_get_owner_uid(pid, &uid);
    if (ret >= 0) {
      ret = sd_uid_get_display(uid, &raw_session_id);
    }
  }
  if (ret < 0) {
    LOG(ERROR) << "Failed to get session ID for PID " << pid
               << ", error: " << base::safe_strerror(-ret);
    return {};
  }
  if (!raw_session_id) {
    LOG(ERROR) << "Failed to get session ID for PID " << pid
               << ": null session ID returned";
    return {};
  }
  std::unique_ptr<char, base::FreeDeleter> session_id(raw_session_id);

  if (!IsGraphicalSession(session_id.get())) {
    HOST_LOG << "Session " << session_id.get() << " is not graphical.";
    return {};
  }
  return session_id.get();
}

}  // namespace

LoginSessionServer::LoginSessionServer(Delegate* delegate)
    : delegate_(delegate),
      ipc_server_(
          CreateEndpointOptions(),
          base::BindRepeating(IsTrustedMojoEndpoint)
              .Then(base::BindRepeating(
                  [](mojom::LoginSessionService* service, bool is_valid) {
                    return is_valid ? service : nullptr;
                  },
                  this))) {}

LoginSessionServer::~LoginSessionServer() = default;

void LoginSessionServer::StartServer() {
  ipc_server_.StartServer();
}

void LoginSessionServer::StopServer() {
  ipc_server_.StopServer();
}

void LoginSessionServer::IsRunningInCrdSession(
    IsRunningInCrdSessionCallback callback) {
  std::string session_id =
      GetGraphicalSessionIdForPid(ipc_server_.current_connection_info().pid);
  if (session_id.empty()) {
    std::move(callback).Run(false);
    return;
  }
  delegate_->IsRunningInCrdSession(session_id, std::move(callback));
}

}  // namespace remoting
