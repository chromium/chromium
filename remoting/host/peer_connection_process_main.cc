// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/scoped_refptr.h"
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
#include "mojo/public/cpp/system/message_pipe.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/base/host_exit_codes.h"
#include "remoting/host/base/switches.h"
#include "remoting/host/peer_connection_process.h"
#include "remoting/host/security_key/security_key_auth_handler.h"

#if BUILDFLAG(IS_POSIX)
#include "base/files/file_descriptor_watcher_posix.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include <memory>

#include "base/logging.h"
#include "remoting/host/linux/peer_connection_bpf_policy_linux.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#endif

namespace remoting {

int PeerConnectionProcessMain() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  SecurityKeyAuthHandler::set_use_mojo_handler(true);

  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("PeerConnection");

  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::IO);
  base::RunLoop run_loop;
  scoped_refptr<AutoThreadTaskRunner> task_runner =
      base::MakeRefCounted<AutoThreadTaskRunner>(
          main_task_executor.task_runner(), run_loop.QuitClosure());

  // Launch the IPC I/O thread.
  scoped_refptr<AutoThreadTaskRunner> io_task_runner =
      AutoThread::CreateWithType("I/O thread", task_runner,
                                 base::MessagePumpType::IO);

#if BUILDFLAG(IS_POSIX)
  base::FileDescriptorWatcher fd_watcher(main_task_executor.task_runner());
#endif

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

  PeerConnectionProcess peer_connection_process(task_runner, io_task_runner);

  if (!peer_connection_process.Start(std::move(message_pipe))) {
    return kInitializationFailed;
  }

#if BUILDFLAG(IS_LINUX)
  // Engage the multi-threaded Seccomp-BPF sandbox after establishing the
  // initial Mojo IPC connection with the parent process, but before starting
  // the main RunLoop to process untrusted WebRTC peer traffic and remote
  // inputs.
  //
  // Note: Future threads created on the fly (e.g. by ThreadPool or WebRTC)
  // are explicitly allowed by the policy and automatically inherit the filter
  // via SECCOMP_FILTER_FLAG_TSYNC.
  if (sandbox::SandboxBPF::SupportsSeccompSandbox(
          sandbox::SandboxBPF::SeccompLevel::MULTI_THREADED)) {
    sandbox::SandboxBPF sandbox(
        std::make_unique<PeerConnectionBpfPolicyLinux>());
    if (!sandbox.StartSandbox(
            sandbox::SandboxBPF::SeccompLevel::MULTI_THREADED)) {
      LOG(ERROR) << "Failed to engage Seccomp-BPF sandbox in the peer "
                 << "connection process.";
      return kInitializationFailed;
    }
  } else {
    LOG(WARNING)
        << "Seccomp-BPF multi-threaded sandbox not supported on this kernel.";
  }
#endif

  // Run the loop.
  task_runner = nullptr;
  run_loop.Run();

  return kSuccessExitCode;
}

}  // namespace remoting
