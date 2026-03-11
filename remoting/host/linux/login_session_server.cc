// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/login_session_server.h"

#include <systemd/sd-login.h>

#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/posix/safe_strerror.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
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

bool IsRunningInGraphicalSession(base::ProcessId pid) {
  std::string environ_content;
  if (!base::ReadFileToString(
          base::FilePath(base::StringPrintf("/proc/%d/environ", pid)),
          &environ_content)) {
    return false;
  }

  std::vector<std::string> env_vars =
      base::SplitString(environ_content, std::string(1, '\0'),
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const std::string& env_var : env_vars) {
    if (env_var == "XDG_SESSION_TYPE=x11" ||
        env_var == "XDG_SESSION_TYPE=wayland") {
      return true;
    }
  }
  return false;
}

std::string GetSessionIdForPid(base::ProcessId pid) {
  char* session_id = nullptr;
  int ret = sd_pid_get_session(pid, &session_id);
  if (ret < 0 && IsRunningInGraphicalSession(pid)) {
    // For user sessions, the process is executed by the per-user systemd
    // instance and is not associated with a specific login session, meaning
    // sd_pid_get_session() will always return -ENODATA. See:
    // https://manpages.debian.org/stretch/libsystemd-dev/sd_pid_get_session.3.en.html
    // For modern GDM, each user can only have one graphical session, so it
    // should be safe to fallback to the user's primary graphical session.
    HOST_LOG << "Failed to get session ID for PID " << pid << ": "
             << base::safe_strerror(-ret)
             << ". Falling back to the user's display session.";
    uid_t uid;
    if (sd_pid_get_owner_uid(pid, &uid) == 0) {
      ret = sd_uid_get_display(uid, &session_id);
    }
  }
  if (ret < 0) {
    LOG(ERROR) << "Failed to get session ID for PID " << pid
               << ", error: " << base::safe_strerror(-ret);
    return {};
  }
  std::string session_id_string = session_id;
  free(session_id);
  return session_id_string;
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
      GetSessionIdForPid(ipc_server_.current_connection_info().pid);
  if (session_id.empty()) {
    std::move(callback).Run(false);
    return;
  }
  std::move(callback).Run(delegate_->IsRunningInCrdSession(session_id));
}

}  // namespace remoting
