// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements the Windows service controlling Me2Me host processes
// running within user sessions.

#include "remoting/host/desktop_process.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/notreached.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "ipc/ipc_channel_proxy.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/crash_process.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/desktop_session_agent.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/win/windows_version.h"
#endif  // BUILDFLAG(IS_WIN)

namespace remoting {

DesktopProcess::DesktopProcess(
    scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
    scoped_refptr<AutoThreadTaskRunner> input_task_runner,
    scoped_refptr<AutoThreadTaskRunner> io_task_runner,
    mojo::ScopedMessagePipeHandle daemon_channel_handle)
    : caller_task_runner_(caller_task_runner),
      input_task_runner_(input_task_runner),
      io_task_runner_(io_task_runner),
      daemon_channel_handle_(std::move(daemon_channel_handle)) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(base::CurrentUIThread::IsSet());
}

DesktopProcess::~DesktopProcess() {
  DCHECK(!daemon_channel_);
  DCHECK(!desktop_agent_.get());
}

DesktopEnvironmentFactory& DesktopProcess::desktop_environment_factory() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return *desktop_environment_factory_;
}

void DesktopProcess::OnNetworkProcessDisconnected() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  if (desktop_agent_) {
    desktop_agent_->Stop();
    desktop_agent_ = nullptr;
  }
  if (on_network_process_disconnected_callback_) {
    std::move(on_network_process_disconnected_callback_).Run();
  }
}

void DesktopProcess::CrashNetworkProcess(const base::Location& location) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  LOG(ERROR) << "Asking the daemon process to crash the network process. "
             << "Request originated from: " << location.ToString();
  desktop_session_request_handler_->CrashNetworkProcess();
}

void DesktopProcess::InjectSas() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  desktop_session_request_handler_->InjectSecureAttentionSequence();
}

void DesktopProcess::LockWorkstation() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
#if BUILDFLAG(IS_WIN)
  if (base::win::OSInfo::GetInstance()->version_type() ==
      base::win::VersionType::SUITE_HOME) {
    return;
  }

  if (!::LockWorkStation()) {
    PLOG(ERROR) << "LockWorkStation() failed";
  }
#else
  NOTREACHED();
#endif  // BUILDFLAG(IS_WIN)
}

void DesktopProcess::OnChannelConnected(int32_t peer_pid) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  VLOG(1) << "IPC: desktop <- daemon (" << peer_pid << ")";
}

void DesktopProcess::OnChannelError() {
  // Shutdown the desktop process.
  daemon_channel_.reset();
  if (desktop_agent_) {
    desktop_agent_->Stop();
    desktop_agent_ = nullptr;
  }
  desktop_session_request_handler_.reset();
  worker_process_control_.reset();

  caller_task_runner_ = nullptr;
  input_task_runner_ = nullptr;
  io_task_runner_ = nullptr;
  audio_task_runner_ = nullptr;
  desktop_environment_factory_.reset();
}

void DesktopProcess::OnAssociatedInterfaceRequest(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  if (interface_name == mojom::WorkerProcessControl::Name_) {
    if (worker_process_control_.is_bound()) {
      LOG(ERROR) << "Receiver already bound for associated interface: "
                 << mojom::WorkerProcessControl::Name_;
      CrashProcess(__func__, __FILE__, __LINE__);
    }

    mojo::PendingAssociatedReceiver<mojom::WorkerProcessControl>
        pending_receiver(std::move(handle));
    worker_process_control_.Bind(std::move(pending_receiver));
  } else if (interface_name == mojom::DesktopProcessControl::Name_) {
    if (desktop_process_control_.is_bound()) {
      LOG(ERROR) << "Receiver already bound for associated interface: "
                 << mojom::DesktopProcessControl::Name_;
      CrashProcess(__func__, __FILE__, __LINE__);
    }

    mojo::PendingAssociatedReceiver<mojom::DesktopProcessControl>
        pending_receiver(std::move(handle));
    desktop_process_control_.Bind(std::move(pending_receiver));
  } else {
    LOG(ERROR) << "Received unexpected associated interface request: "
               << interface_name;
    CrashProcess(__func__, __FILE__, __LINE__);
  }
}

bool DesktopProcess::Start(
    std::unique_ptr<DesktopEnvironmentFactory> desktop_environment_factory) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(!desktop_environment_factory_);
  DCHECK(desktop_environment_factory);

  desktop_environment_factory_ = std::move(desktop_environment_factory);

  // Launch the audio capturing thread.
#if BUILDFLAG(IS_WIN)
  // On Windows the AudioCapturer requires COM, so we run a single-threaded
  // apartment, which requires a UI thread.
  audio_task_runner_ = AutoThread::CreateWithLoopAndComInitTypes(
      "ChromotingAudioThread", caller_task_runner_, base::MessagePumpType::UI,
      AutoThread::COM_INIT_STA);
#else   // !BUILDFLAG(IS_WIN)
  audio_task_runner_ = AutoThread::CreateWithType(
      "ChromotingAudioThread", caller_task_runner_, base::MessagePumpType::IO);
#endif  // !BUILDFLAG(IS_WIN)

  // Create a desktop agent.
  mojo::ScopedMessagePipeHandle desktop_pipe = CreateDesktopAgent();

  // Connect to the daemon.
  daemon_channel_ = IPC::ChannelProxy::Create(
      daemon_channel_handle_.release(), IPC::Channel::MODE_CLIENT, this,
      io_task_runner_, base::SingleThreadTaskRunner::GetCurrentDefault());

  daemon_channel_->GetRemoteAssociatedInterface(
      &desktop_session_request_handler_);

  // Pass |desktop_pipe| to the daemon.
  desktop_session_request_handler_->ConnectDesktopChannel(
      std::move(desktop_pipe));

  return true;
}

void DesktopProcess::SetOnNetworkProcessDisconnectedCallbackForTesting(
    base::OnceClosure callback) {
  on_network_process_disconnected_callback_ = std::move(callback);
}

void DesktopProcess::SetOnDesktopAgentCreatedCallbackForTesting(
    base::OnceClosure callback) {
  on_desktop_agent_created_callback_ = std::move(callback);
}

void DesktopProcess::CrashProcess(const std::string& function_name,
                                  const std::string& file_name,
                                  int line_number) {
  // The daemon requested us to crash the process.
  ::remoting::CrashProcess(function_name, file_name, line_number);
}

void DesktopProcess::ReconnectNetworkChannel() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  if (desktop_agent_) {
    LOG(ERROR) << "Cannot reconnect the network channel when the "
               << "DesktopSessionAgent is still active.";
    CrashProcess(__func__, __FILE__, __LINE__);
    return;
  }
  desktop_session_request_handler_->ConnectDesktopChannel(CreateDesktopAgent());
}

mojo::ScopedMessagePipeHandle DesktopProcess::CreateDesktopAgent() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  desktop_agent_ = base::MakeRefCounted<DesktopSessionAgent>(
      audio_task_runner_, caller_task_runner_, input_task_runner_,
      io_task_runner_);

  if (on_desktop_agent_created_callback_) {
    std::move(on_desktop_agent_created_callback_).Run();
  }

  // Initialize the agent and create an IPC channel to talk to it.
  return desktop_agent_->Initialize(weak_factory_.GetWeakPtr());
}

}  // namespace remoting
