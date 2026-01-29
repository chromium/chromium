// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/login_session_reporter_main.h"

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

int LoginSessionReporterMain(int argc, char** argv) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);

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
  session_info->xdg_session_id =
      environment->GetVar("XDG_SESSION_ID").value_or({});
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
