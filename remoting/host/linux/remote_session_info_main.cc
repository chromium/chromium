// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/remote_session_info_main.h"

#include <iostream>
#include <utility>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_executor.h"
#include "components/named_mojo_ipc_server/named_mojo_ipc_server_client_util.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/invitation.h"
#include "remoting/base/logging.h"
#include "remoting/host/base/host_exit_codes.h"
#include "remoting/host/base/switches.h"
#include "remoting/host/ipc_constants.h"
#include "remoting/host/mojom/login_session.mojom.h"

namespace remoting {

namespace {

constexpr char kHelpSwitch[] = "help";
constexpr char kIsCrdSessionSwitch[] = "is-crd-session";

constexpr int kInCrdSessionExitCode = 0;
constexpr int kNotInCrdSessionExitCode = 1;

void Usage(const char* program_name) {
  std::cout
      << "Usage: " << program_name << " [options]" << std::endl
      << std::endl
      << "Options:" << std::endl
      << "    --" << kIsCrdSessionSwitch << std::endl
      << "        Exits with 0 if the current session is managed by Chrome"
      << std::endl
      << "        Remote Desktop and exits with 1 otherwise. Note that the"
      << std::endl
      << "        program will still exit with 0 if the session is managed"
      << std::endl
      << "        by is managed by CRD but the client isn't connected."
      << std::endl;
}

}  // namespace

int RemoteSessionInfoMain(int argc, char** argv) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);
  InitHostLogging();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->GetSwitches().empty() ||
      command_line->HasSwitch(kHelpSwitch)) {
    Usage(argv[0]);
    return kSuccessExitCode;
  }
  // We currently only support --is-crd-session.
  if (!command_line->HasSwitch(kIsCrdSessionSwitch)) {
    std::cerr << "Unrecognized command: "
              << base::JoinString(command_line->argv(), " ") << std::endl;
    std::cerr << "Try '" << argv[0] << " --" << kHelpSwitch
              << "' for more information." << std::endl;
    return kSuccessExitCode;
  }

  base::SingleThreadTaskExecutor task_executor(base::MessagePumpType::IO);
  mojo::core::Init();
  mojo::core::ScopedIPCSupport ipc_support(
      task_executor.task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

  auto server_name = GetLoginSessionServerName();
  auto endpoint = named_mojo_ipc_server::ConnectToServer(server_name);
  if (!endpoint.is_valid()) {
    VLOG(1) << "Cannot connect to IPC through server name " << server_name
            << ". Endpoint is invalid.";
    // The multi-process host is not running. Check the
    // `CHROME_REMOTE_DESKTOP_SESSION` variable instead, which is set by the
    // single-process host.
    auto environment = base::Environment::Create();
    return environment->GetVar("CHROME_REMOTE_DESKTOP_SESSION")
                   .value_or("")
                   .empty()
               ? kNotInCrdSessionExitCode
               : kInCrdSessionExitCode;
  }

  auto invitation = mojo::IncomingInvitation::Accept(std::move(endpoint));
  auto message_pipe =
      invitation.ExtractMessagePipe(kLoginSessionServerMessagePipeId);
  if (!message_pipe.is_valid()) {
    LOG(ERROR) << "Invalid message pipe.";
    return kInitializationFailed;
  }

  mojo::Remote<mojom::LoginSessionService> remote(
      mojo::PendingRemote<mojom::LoginSessionService>{std::move(message_pipe),
                                                      /*version=*/0});

  base::RunLoop run_loop;
  int exit_code = kNotInCrdSessionExitCode;
  remote->IsRunningInCrdSession(base::BindOnce(
      [](base::OnceClosure quit_closure, int* exit_code_out,
         bool is_crd_session) {
        *exit_code_out =
            is_crd_session ? kInCrdSessionExitCode : kNotInCrdSessionExitCode;
        std::move(quit_closure).Run();
      },
      run_loop.QuitClosure(), &exit_code));

  remote.set_disconnect_handler(run_loop.QuitClosure());
  run_loop.Run();

  return exit_code;
}

}  // namespace remoting
