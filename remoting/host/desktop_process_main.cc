// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements the Windows service controlling Me2Me host processes
// running within user sessions.

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "build/build_config.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/system/invitation.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/base/host_exit_codes.h"
#include "remoting/host/base/switches.h"
#include "remoting/host/desktop_process.h"
#include "remoting/host/host_main.h"
#include "remoting/host/me2me_desktop_environment.h"
#include "remoting/host/win/session_desktop_environment.h"

namespace remoting {

int DesktopProcessMain() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("Me2Me");

  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);
  base::RunLoop run_loop;
  scoped_refptr<AutoThreadTaskRunner> ui_task_runner = new AutoThreadTaskRunner(
      main_task_executor.task_runner(), run_loop.QuitClosure());

  // Launch the video capture thread.
  scoped_refptr<AutoThreadTaskRunner> video_capture_task_runner =
      AutoThread::Create("Video capture thread", ui_task_runner);

  // Launch the input thread.
  scoped_refptr<AutoThreadTaskRunner> input_task_runner =
      AutoThread::CreateWithType("Input thread", ui_task_runner,
                                 base::MessagePumpType::IO);

  // Launch the I/O thread.
  scoped_refptr<AutoThreadTaskRunner> io_task_runner =
      AutoThread::CreateWithType("I/O thread", ui_task_runner,
                                 base::MessagePumpType::IO);

  mojo::core::ScopedIPCSupport ipc_support(
      io_task_runner->task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST);
  mojo::PlatformChannelEndpoint endpoint =
      mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
          *command_line);
  if (!endpoint.is_valid()) {
    endpoint = mojo::NamedPlatformChannel::ConnectToServer(*command_line);
  }
  if (!endpoint.is_valid()) {
    return kInvalidCommandLineExitCode;
  }

  auto invitation = mojo::IncomingInvitation::Accept(std::move(endpoint));
  mojo::ScopedMessagePipeHandle message_pipe = invitation.ExtractMessagePipe(
      command_line->GetSwitchValueASCII(kMojoPipeToken));
  DesktopProcess desktop_process(ui_task_runner, input_task_runner,
                                 io_task_runner, std::move(message_pipe));

  // Create a platform-dependent environment factory.
  std::unique_ptr<DesktopEnvironmentFactory> desktop_environment_factory;
#if BUILDFLAG(IS_WIN)
  // base::Unretained() is safe here: |desktop_process| outlives run_loop.Run().
  auto inject_sas_closure = base::BindRepeating(
      &DesktopProcess::InjectSas, base::Unretained(&desktop_process));
  auto lock_workstation_closure = base::BindRepeating(
      &DesktopProcess::LockWorkstation, base::Unretained(&desktop_process));

  desktop_environment_factory =
      std::make_unique<SessionDesktopEnvironmentFactory>(
          ui_task_runner, video_capture_task_runner, input_task_runner,
          ui_task_runner, inject_sas_closure, lock_workstation_closure);
#else   // !BUILDFLAG(IS_WIN)
  desktop_environment_factory.reset(new Me2MeDesktopEnvironmentFactory(
      ui_task_runner, video_capture_task_runner, input_task_runner,
      ui_task_runner));
#endif  // !BUILDFLAG(IS_WIN)

  if (!desktop_process.Start(std::move(desktop_environment_factory))) {
    return kInitializationFailed;
  }

  // Run the UI message loop.
  ui_task_runner = nullptr;
  run_loop.Run();

  return kSuccessExitCode;
}

}  // namespace remoting

#if !BUILDFLAG(IS_WIN)
int main(int argc, char** argv) {
  return remoting::HostMain(argc, argv);
}
#endif  // !BUILDFLAG(IS_WIN)
