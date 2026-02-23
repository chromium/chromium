// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/login_session_reporter_main.h"

#include <systemd/sd-login.h>

#include <string>
#include <utility>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "components/named_mojo_ipc_server/named_mojo_ipc_server_client_util.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/invitation.h"
#include "remoting/base/logging.h"
#include "remoting/host/base/host_exit_codes.h"
#include "remoting/host/ipc_constants.h"
#include "remoting/host/mojom/login_session.mojom.h"

namespace remoting {

namespace {

std::string GetSessionId(bool is_user_session) {
  // The `XDG_SESSION_ID` environment variable isn't available when the process
  // is autostarted. This seems to have something to do with the XDG autostart
  // process now being managed by systemd. So we have to use systemd's API to
  // query the session ID.
  char* session_id = nullptr;
  // Get session ID for the current process (PID 0 means self)
  int ret = sd_pid_get_session(0, &session_id);
  if (ret < 0 && is_user_session) {
    // For user sessions, the process is executed by the per-user systemd
    // instance and is not associated with a specific login session. For modern
    // GDM, each user can only one graphical session, so it should be safe to
    // fallback to the user's primary graphical session.
    HOST_LOG << "Failed to get session ID for the current process. Falling back"
             << " to the user's primary graphical session instead.";
    ret = sd_uid_get_display(getuid(), &session_id);
  }
  if (ret < 0) {
    // Handle error (e.g., not running in a systemd session)
    LOG(ERROR) << "Failed to get session ID: " << strerror(-ret);
    return {};
  }
  std::string session_id_string = session_id;
  // Free the memory allocated by the library
  free(session_id);
  return session_id_string;
}

}  // namespace

int LoginSessionReporterMain(int argc, char** argv) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);
  InitHostLogging();

  base::SingleThreadTaskExecutor task_executor(base::MessagePumpType::IO);
  mojo::core::Init();
  mojo::core::ScopedIPCSupport ipc_support(
      task_executor.task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

  auto server_name = GetLoginSessionReporterServerName();
  auto endpoint = named_mojo_ipc_server::ConnectToServer(server_name);
  if (!endpoint.is_valid()) {
    LOG(ERROR) << "Cannot connect to IPC through server name " << server_name
               << ". Endpoint is invalid.";
    return kInitializationFailed;
  }

  auto invitation = mojo::IncomingInvitation::Accept(std::move(endpoint));
  auto message_pipe =
      invitation.ExtractMessagePipe(kLoginSessionReporterMessagePipeId);
  if (!message_pipe.is_valid()) {
    LOG(ERROR) << "Invalid message pipe.";
    return kInitializationFailed;
  }
  mojo::Remote<mojom::LoginSessionObserver> remote(
      mojo::PendingRemote<mojom::LoginSessionObserver>{std::move(message_pipe),
                                                       /*version=*/0});
  base::RunLoop run_loop;
  remote.set_disconnect_handler(run_loop.QuitClosure());
  HOST_LOG << "Successfully connected to the daemon process.";

  auto environment = base::Environment::Create();
  mojom::LoginSessionInfoPtr session_info{std::in_place};
  bool is_user_session =
      environment->GetVar("XDG_SESSION_CLASS").value_or({}) == "user";
  session_info->session_id = GetSessionId(is_user_session);
  session_info->xdg_current_desktop =
      environment->GetVar("XDG_CURRENT_DESKTOP").value_or({});
  session_info->dbus_session_bus_address =
      environment->GetVar("DBUS_SESSION_BUS_ADDRESS").value_or({});
  session_info->display = environment->GetVar("DISPLAY").value_or({});
  session_info->wayland_display =
      environment->GetVar("WAYLAND_DISPLAY").value_or({});
  remote->OnLoginSessionCreated(std::move(session_info));

  HOST_LOG << "Environment variables sent to the daemon process.";
  run_loop.Run();

  HOST_LOG << "Peer disconnected.";

  return kSuccessExitCode;
}

}  // namespace remoting
