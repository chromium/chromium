// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ipc_desktop_environment.h"

#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/process/process_handle.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_sender.h"
#include "remoting/host/action_executor.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/desktop_session.h"
#include "remoting/host/desktop_session_proxy.h"
#include "remoting/host/file_transfer/file_operations.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/screen_controls.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"

namespace remoting {

IpcDesktopEnvironment::IpcDesktopEnvironment(
    scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control,
    base::WeakPtr<DesktopSessionConnector> desktop_session_connector,
    const DesktopEnvironmentOptions& options)
    : desktop_session_proxy_(
          base::MakeRefCounted<DesktopSessionProxy>(audio_task_runner,
                                                    caller_task_runner,
                                                    io_task_runner,
                                                    client_session_control,
                                                    desktop_session_connector,
                                                    options)) {
  DCHECK(caller_task_runner->BelongsToCurrentThread());
}

IpcDesktopEnvironment::~IpcDesktopEnvironment() = default;

std::unique_ptr<ActionExecutor> IpcDesktopEnvironment::CreateActionExecutor() {
  return desktop_session_proxy_->CreateActionExecutor();
}

std::unique_ptr<AudioCapturer> IpcDesktopEnvironment::CreateAudioCapturer() {
  return desktop_session_proxy_->CreateAudioCapturer();
}

std::unique_ptr<InputInjector> IpcDesktopEnvironment::CreateInputInjector() {
  return desktop_session_proxy_->CreateInputInjector();
}

std::unique_ptr<ScreenControls> IpcDesktopEnvironment::CreateScreenControls() {
  return desktop_session_proxy_->CreateScreenControls();
}

std::unique_ptr<webrtc::MouseCursorMonitor>
IpcDesktopEnvironment::CreateMouseCursorMonitor() {
  return desktop_session_proxy_->CreateMouseCursorMonitor();
}

std::unique_ptr<webrtc::DesktopCapturer>
IpcDesktopEnvironment::CreateVideoCapturer() {
  return desktop_session_proxy_->CreateVideoCapturer();
}

std::unique_ptr<FileOperations> IpcDesktopEnvironment::CreateFileOperations() {
  return desktop_session_proxy_->CreateFileOperations();
}

std::string IpcDesktopEnvironment::GetCapabilities() const {
  return desktop_session_proxy_->GetCapabilities();
}

void IpcDesktopEnvironment::SetCapabilities(const std::string& capabilities) {
  return desktop_session_proxy_->SetCapabilities(capabilities);
}

uint32_t IpcDesktopEnvironment::GetDesktopSessionId() const {
  return desktop_session_proxy_->desktop_session_id();
}

IpcDesktopEnvironmentFactory::IpcDesktopEnvironmentFactory(
    scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    IPC::Sender* daemon_channel)
    : audio_task_runner_(audio_task_runner),
      caller_task_runner_(caller_task_runner),
      io_task_runner_(io_task_runner),
      daemon_channel_(daemon_channel) {}

IpcDesktopEnvironmentFactory::~IpcDesktopEnvironmentFactory() = default;

std::unique_ptr<DesktopEnvironment> IpcDesktopEnvironmentFactory::Create(
    base::WeakPtr<ClientSessionControl> client_session_control,
    const DesktopEnvironmentOptions& options) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return std::make_unique<IpcDesktopEnvironment>(
      audio_task_runner_, caller_task_runner_, io_task_runner_,
      client_session_control, connector_factory_.GetWeakPtr(), options);
}

bool IpcDesktopEnvironmentFactory::SupportsAudioCapture() const {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return AudioCapturer::IsSupported();
}

void IpcDesktopEnvironmentFactory::ConnectTerminal(
    DesktopSessionProxy* desktop_session_proxy,
    const ScreenResolution& resolution,
    bool virtual_terminal) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  int id = next_id_++;
  bool inserted = active_connections_.insert(
      std::make_pair(id, desktop_session_proxy)).second;
  CHECK(inserted);

  VLOG(1) << "Network: registered desktop environment " << id;

  daemon_channel_->Send(new ChromotingNetworkHostMsg_ConnectTerminal(
      id, resolution, virtual_terminal));
}

void IpcDesktopEnvironmentFactory::DisconnectTerminal(
    DesktopSessionProxy* desktop_session_proxy) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  ActiveConnectionsList::iterator i;
  for (i = active_connections_.begin(); i != active_connections_.end(); ++i) {
    if (i->second == desktop_session_proxy)
      break;
  }

  if (i != active_connections_.end()) {
    int id = i->first;
    active_connections_.erase(i);

    VLOG(1) << "Network: unregistered desktop environment " << id;
    daemon_channel_->Send(new ChromotingNetworkHostMsg_DisconnectTerminal(id));
  }
}

void IpcDesktopEnvironmentFactory::SetScreenResolution(
    DesktopSessionProxy* desktop_session_proxy,
    const ScreenResolution& resolution) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  ActiveConnectionsList::iterator i;
  for (i = active_connections_.begin(); i != active_connections_.end(); ++i) {
    if (i->second == desktop_session_proxy)
      break;
  }

  if (i != active_connections_.end()) {
    daemon_channel_->Send(new ChromotingNetworkDaemonMsg_SetScreenResolution(
        i->first, resolution));
  }
}

void IpcDesktopEnvironmentFactory::OnDesktopSessionAgentAttached(
    int terminal_id,
    int session_id,
    const IPC::ChannelHandle& desktop_pipe) {
  if (!caller_task_runner_->BelongsToCurrentThread()) {
    caller_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &IpcDesktopEnvironmentFactory::OnDesktopSessionAgentAttached,
            base::Unretained(this), terminal_id, session_id, desktop_pipe));
    return;
  }

  auto i = active_connections_.find(terminal_id);
  if (i != active_connections_.end()) {
    i->second->DetachFromDesktop();
    i->second->AttachToDesktop(desktop_pipe, session_id);
  } else {
    mojo::ScopedMessagePipeHandle closer(desktop_pipe.mojo_handle);
  }
}

void IpcDesktopEnvironmentFactory::OnTerminalDisconnected(int terminal_id) {
  if (!caller_task_runner_->BelongsToCurrentThread()) {
    caller_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&IpcDesktopEnvironmentFactory::OnTerminalDisconnected,
                       base::Unretained(this), terminal_id));
    return;
  }

  auto i = active_connections_.find(terminal_id);
  if (i != active_connections_.end()) {
    DesktopSessionProxy* desktop_session_proxy = i->second;
    active_connections_.erase(i);

    // Disconnect the client session.
    desktop_session_proxy->DisconnectSession(protocol::OK);
  }
}

}  // namespace remoting
