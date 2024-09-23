// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ipc_desktop_environment.h"

#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/process/process_handle.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "remoting/host/action_executor.h"
#include "remoting/host/active_display_monitor.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/base/screen_controls.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/desktop_display_info_monitor.h"
#include "remoting/host/desktop_session.h"
#include "remoting/host/desktop_session_proxy.h"
#include "remoting/host/file_transfer/file_operations.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/keyboard_layout_monitor.h"
#include "remoting/host/remote_open_url/url_forwarder_configurator.h"
#include "remoting/protocol/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"

namespace remoting {

IpcDesktopEnvironment::IpcDesktopEnvironment(
    scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control,
    base::WeakPtr<ClientSessionEvents> client_session_events,
    base::WeakPtr<DesktopSessionConnector> desktop_session_connector,
    const DesktopEnvironmentOptions& options)
    : desktop_session_proxy_(
          base::MakeRefCounted<DesktopSessionProxy>(audio_task_runner,
                                                    io_task_runner,
                                                    client_session_control,
                                                    client_session_events,
                                                    desktop_session_connector,
                                                    options)) {
  DCHECK(network_task_runner->BelongsToCurrentThread());
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

DesktopDisplayInfoMonitor* IpcDesktopEnvironment::GetDisplayInfoMonitor() {
  // Not used in the Network process.
  return nullptr;
}

std::unique_ptr<webrtc::MouseCursorMonitor>
IpcDesktopEnvironment::CreateMouseCursorMonitor() {
  return desktop_session_proxy_->CreateMouseCursorMonitor();
}

std::unique_ptr<KeyboardLayoutMonitor>
IpcDesktopEnvironment::CreateKeyboardLayoutMonitor(
    base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback) {
  return desktop_session_proxy_->CreateKeyboardLayoutMonitor(
      std::move(callback));
}

std::unique_ptr<ActiveDisplayMonitor>
IpcDesktopEnvironment::CreateActiveDisplayMonitor(
    ActiveDisplayMonitor::Callback callback) {
  return nullptr;
}

std::unique_ptr<DesktopCapturer> IpcDesktopEnvironment::CreateVideoCapturer(
    webrtc::ScreenId id) {
  return desktop_session_proxy_->CreateVideoCapturer(id);
}

std::unique_ptr<FileOperations> IpcDesktopEnvironment::CreateFileOperations() {
  return desktop_session_proxy_->CreateFileOperations();
}

std::unique_ptr<UrlForwarderConfigurator>
IpcDesktopEnvironment::CreateUrlForwarderConfigurator() {
  return desktop_session_proxy_->CreateUrlForwarderConfigurator();
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

std::unique_ptr<RemoteWebAuthnStateChangeNotifier>
IpcDesktopEnvironment::CreateRemoteWebAuthnStateChangeNotifier() {
  return desktop_session_proxy_->CreateRemoteWebAuthnStateChangeNotifier();
}

IpcDesktopEnvironmentFactory::IpcDesktopEnvironmentFactory(
    scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    mojo::AssociatedRemote<mojom::DesktopSessionManager> remote)
    : audio_task_runner_(audio_task_runner),
      network_task_runner_(network_task_runner),
      io_task_runner_(io_task_runner),
      desktop_session_manager_(std::move(remote)) {}

IpcDesktopEnvironmentFactory::~IpcDesktopEnvironmentFactory() {
  // |desktop_session_manager_| was bound on |network_task_runner_| so it needs
  // to be destroyed there. This is safe since this instance is being destroyed
  // so nothing relies on it at this point.
  network_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](mojo::AssociatedRemote<mojom::DesktopSessionManager> remote) {},
          std::move(desktop_session_manager_)));
}

std::unique_ptr<DesktopEnvironment> IpcDesktopEnvironmentFactory::Create(
    base::WeakPtr<ClientSessionControl> client_session_control,
    base::WeakPtr<ClientSessionEvents> client_session_events,
    const DesktopEnvironmentOptions& options) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  return std::make_unique<IpcDesktopEnvironment>(
      audio_task_runner_, network_task_runner_, io_task_runner_,
      client_session_control, client_session_events,
      connector_factory_.GetWeakPtr(), options);
}

bool IpcDesktopEnvironmentFactory::SupportsAudioCapture() const {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  return AudioCapturer::IsSupported();
}

void IpcDesktopEnvironmentFactory::ConnectTerminal(
    DesktopSessionProxy* desktop_session_proxy,
    const ScreenResolution& resolution,
    bool virtual_terminal) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  int id = next_id_++;
  bool inserted =
      active_connections_.insert(std::make_pair(id, desktop_session_proxy))
          .second;
  CHECK(inserted);

  VLOG(1) << "Network: registered desktop environment " << id;

  desktop_session_manager_->CreateDesktopSession(id, resolution,
                                                 virtual_terminal);
}

void IpcDesktopEnvironmentFactory::DisconnectTerminal(
    DesktopSessionProxy* desktop_session_proxy) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  ActiveConnectionsList::iterator i;
  for (i = active_connections_.begin(); i != active_connections_.end(); ++i) {
    if (i->second == desktop_session_proxy) {
      break;
    }
  }

  if (i != active_connections_.end()) {
    int id = i->first;
    active_connections_.erase(i);

    VLOG(1) << "Network: unregistered desktop environment " << id;
    desktop_session_manager_->CloseDesktopSession(id);
  }
}

void IpcDesktopEnvironmentFactory::SetScreenResolution(
    DesktopSessionProxy* desktop_session_proxy,
    const ScreenResolution& resolution) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  ActiveConnectionsList::iterator i;
  for (i = active_connections_.begin(); i != active_connections_.end(); ++i) {
    if (i->second == desktop_session_proxy) {
      break;
    }
  }

  if (i != active_connections_.end()) {
    desktop_session_manager_->SetScreenResolution(i->first, resolution);
  }
}

bool IpcDesktopEnvironmentFactory::BindConnectionEventsReceiver(
    mojo::ScopedInterfaceEndpointHandle handle) {
  if (desktop_session_connection_events_.is_bound()) {
    return false;
  }

  mojo::PendingAssociatedReceiver<mojom::DesktopSessionConnectionEvents>
      pending_receiver(std::move(handle));
  desktop_session_connection_events_.Bind(std::move(pending_receiver));

  return true;
}

void IpcDesktopEnvironmentFactory::OnDesktopSessionAgentAttached(
    int terminal_id,
    int session_id,
    mojo::ScopedMessagePipeHandle desktop_pipe) {
  if (!network_task_runner_->BelongsToCurrentThread()) {
    network_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &IpcDesktopEnvironmentFactory::OnDesktopSessionAgentAttached,
            base::Unretained(this), terminal_id, session_id,
            std::move(desktop_pipe)));
    return;
  }

  auto i = active_connections_.find(terminal_id);
  if (i != active_connections_.end()) {
    i->second->DetachFromDesktop();
    i->second->AttachToDesktop(std::move(desktop_pipe), session_id);
  }
}

void IpcDesktopEnvironmentFactory::OnTerminalDisconnected(int terminal_id) {
  if (!network_task_runner_->BelongsToCurrentThread()) {
    network_task_runner_->PostTask(
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
    desktop_session_proxy->DisconnectSession(ErrorCode::OK);
  }
}

}  // namespace remoting
