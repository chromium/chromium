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
#include <unistd.h>

#include <memory>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "remoting/host/linux/peer_connection_bpf_policy_linux.h"
#include "remoting/host/linux/peer_connection_landlock_policy_linux.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/services/proc_util.h"
#include "sandbox/linux/services/thread_helpers.h"
#include "sandbox/linux/services/yama.h"
#endif

namespace remoting {

#if BUILDFLAG(IS_LINUX)
namespace {

// Restricts filesystem access of the peer connection process with Landlock.
// `proc_fd` is a file descriptor of /proc. Returns false if the process should
// not continue.
bool ApplyLandlockPolicy(int proc_fd) {
  if (GetLandlockAbiVersion() <= 0) {
    // Without Landlock, the peer connection process is still non-dumpable and
    // sandboxed by Seccomp-BPF. The remaining gap is a short window during the
    // startup of a peer connection process (before it becomes non-dumpable),
    // in which another peer connection process could access its /proc entries.
    // Yama closes that gap if ptrace_scope is at least 1.
    if (sandbox::Yama::GetStatus() & sandbox::Yama::STATUS_ENFORCING) {
      LOG(INFO) << "Landlock is not supported by the kernel.";
    } else {
      LOG(WARNING) << "Landlock is not supported by the kernel and Yama "
                   << "ptrace_scope is 0 (or Yama is disabled). Consider "
                   << "upgrading the kernel, or setting "
                   << "kernel.yama.ptrace_scope=1, to fully isolate peer "
                   << "connection processes from each other.";
    }
    return true;
  }

  // Landlock only restricts the calling thread and the threads it creates
  // afterwards, so it must be applied while the process is single-threaded.
  if (!sandbox::ThreadHelpers::IsSingleThreaded(proc_fd)) {
    LOG(ERROR) << "Cannot apply the Landlock policy: the peer connection "
               << "process is unexpectedly multi-threaded.";
    return false;
  }
  base::FilePath exe_dir;
  if (!base::PathService::Get(base::DIR_EXE, &exe_dir)) {
    LOG(ERROR) << "Failed to get the executable directory.";
    return false;
  }
  if (ApplyPeerConnectionLandlockPolicy({exe_dir}) !=
      PeerConnectionLandlockResult::kApplied) {
    LOG(ERROR) << "Failed to apply the Landlock policy.";
    return false;
  }
  return true;
}

}  // namespace
#endif  // BUILDFLAG(IS_LINUX)

int PeerConnectionProcessMain() {
#if BUILDFLAG(IS_LINUX)
  // SandboxBPF::StartSandbox() opens /proc (and crashes if that fails) unless
  // it is given a file descriptor of /proc. The Landlock policy denies opening
  // /proc, so open it first. The Seccomp-BPF sandbox closes it before
  // installing its filters.
  base::ScopedFD proc_fd = sandbox::ProcUtil::OpenProc();
  if (!ApplyLandlockPolicy(proc_fd.get())) {
    return kInitializationFailed;
  }
#endif  // BUILDFLAG(IS_LINUX)

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
        std::make_unique<PeerConnectionBpfPolicyLinux>(getpid()));
    sandbox.SetProcFd(std::move(proc_fd));
    if (!sandbox.StartSandbox(
            sandbox::SandboxBPF::SeccompLevel::MULTI_THREADED)) {
      LOG(ERROR) << "Failed to engage Seccomp-BPF sandbox in the peer "
                 << "connection process.";
      return kInitializationFailed;
    }
  } else {
    // Running without the sandbox would expose untrusted WebRTC peer traffic
    // to an unconfined process, so fail closed.
    LOG(ERROR) << "Multi-threaded Seccomp-BPF sandbox is not supported "
               << "(requires Linux >= 3.17 and seccomp(2) not being blocked). "
               << "Refusing to start the peer connection process.";
    return kInitializationFailed;
  }
#endif

  // Run the loop.
  task_runner = nullptr;
  run_loop.Run();

  return kSuccessExitCode;
}

}  // namespace remoting
