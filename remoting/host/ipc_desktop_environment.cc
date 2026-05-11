// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ipc_desktop_environment.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "remoting/base/errors.h"
#include "remoting/base/logging.h"
#include "remoting/host/action_executor.h"
#include "remoting/host/active_display_monitor.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/base/desktop_environment_options.h"
#include "remoting/host/base/screen_controls.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/desktop_display_info_monitor.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/desktop_session.h"
#include "remoting/host/desktop_session_proxy.h"
#include "remoting/host/file_transfer/file_operations.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/ipc_audio_injector.h"
#include "remoting/host/ipc_keyboard_layout_monitor.h"
#include "remoting/host/keyboard_layout_monitor.h"
#include "remoting/host/mojom/desktop_session.mojom.h"
#include "remoting/host/mojom/remoting_host.mojom.h"
#include "remoting/host/remote_open_url/url_forwarder_configurator.h"
#include "remoting/protocol/mouse_cursor_monitor.h"
#include "remoting/signaling/signaling_id_util.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"

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

std::unique_ptr<protocol::MouseCursorMonitor>
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

std::unique_ptr<RemoteWebAuthnStateChangeNotifier>
IpcDesktopEnvironment::CreateRemoteWebAuthnStateChangeNotifier() {
  return desktop_session_proxy_->CreateRemoteWebAuthnStateChangeNotifier();
}

std::unique_ptr<AudioInjector> IpcDesktopEnvironment::CreateAudioInjector() {
  return std::make_unique<IpcAudioInjector>(desktop_session_proxy_);
}

IpcDesktopEnvironmentFactory::DesktopConnection::DesktopConnection(
    DesktopSessionProxy* desktop_session_proxy,
    std::string_view client_id)
    : desktop_session_proxy(desktop_session_proxy), client_id(client_id) {}

IpcDesktopEnvironmentFactory::DesktopConnection::~DesktopConnection() = default;

IpcDesktopEnvironmentFactory::DesktopConnection::DesktopConnection(
    DesktopConnection&&) = default;

IpcDesktopEnvironmentFactory::DesktopConnection&
IpcDesktopEnvironmentFactory::DesktopConnection::operator=(
    DesktopConnection&&) = default;

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

void IpcDesktopEnvironmentFactory::Create(
    base::WeakPtr<ClientSessionControl> client_session_control,
    base::WeakPtr<ClientSessionEvents> client_session_events,
    const DesktopEnvironmentOptions& options,
    CreateCallback callback) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                std::make_unique<IpcDesktopEnvironment>(
                                    audio_task_runner_, network_task_runner_,
                                    io_task_runner_, client_session_control,
                                    client_session_events,
                                    connector_factory_.GetWeakPtr(), options)));
}

bool IpcDesktopEnvironmentFactory::SupportsAudioCapture() const {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  return AudioCapturer::IsSupported();
}

void IpcDesktopEnvironmentFactory::ConnectTerminal(
    DesktopSessionProxy* desktop_session_proxy,
    const ScreenResolution& resolution,
    bool is_curtained) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  DCHECK(desktop_session_proxy);

  std::string_view client_jid = desktop_session_proxy->client_jid();
  if (client_jid.empty()) {
    LOG(ERROR) << "Cannot connect terminal. Client JID is empty.";
    return;
  }
  std::string client_id;
  SplitSignalingIdResource(client_jid, &client_id, /*resource=*/nullptr);

  mojom::DesktopSessionOptionsPtr options = mojom::DesktopSessionOptions::New();
  options->screen_resolution = resolution;
  options->is_curtained = is_curtained;
  options->required_username = required_username_;
  options->client_id = client_id;

  if (persist_desktop_sessions_) {
    auto it =
        std::ranges::find_if(connections_, [&client_id](const auto& pair) {
          return pair.second.client_id == client_id &&
                 // Find an unused session.
                 !pair.second.desktop_session_proxy;
        });
    if (it != connections_.end()) {
      int id = it->first;
      VLOG(1) << "Network: reconnecting desktop session " << id;
      it->second.desktop_session_proxy = desktop_session_proxy;
      if (it->second.pending_desktop_pipe.is_valid()) {
        VLOG(1) << "Network: using buffered desktop pipe for session " << id;
        desktop_session_proxy->AttachToDesktop(
            std::move(it->second.pending_desktop_pipe));
      } else {
        desktop_session_manager_->ReconnectDesktopSession(id,
                                                          std::move(options));
      }
      return;
    }
  }

  int id = next_id_++;
  bool inserted =
      connections_
          .insert(std::make_pair(
              id, DesktopConnection{desktop_session_proxy, client_id}))
          .second;
  CHECK(inserted);

  VLOG(1) << "Network: registered desktop session " << id;

  desktop_session_manager_->CreateDesktopSession(id, std::move(options));
}

void IpcDesktopEnvironmentFactory::DisconnectTerminal(
    DesktopSessionProxy* desktop_session_proxy) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  auto it = FindConnection(desktop_session_proxy);
  if (it == connections_.end()) {
    return;
  }

  if (persist_desktop_sessions_) {
    it->second.desktop_session_proxy = nullptr;
    return;
  }

  int id = it->first;
  connections_.erase(it);

  VLOG(1) << "Network: unregistered desktop session " << id;
  desktop_session_manager_->CloseDesktopSession(id);
}

void IpcDesktopEnvironmentFactory::SetScreenResolution(
    DesktopSessionProxy* desktop_session_proxy,
    const ScreenResolution& resolution) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  auto it = FindConnection(desktop_session_proxy);
  if (it != connections_.end()) {
    desktop_session_manager_->SetScreenResolution(it->first, resolution);
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

void IpcDesktopEnvironmentFactory::SetRequiredUsername(
    std::string_view username) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  if (required_username_ == username) {
    return;
  }

  // TODO: yuweih - see if we should just terminate sessions with a mismatched
  // username.
  CHECK(connections_.empty())
      << "Cannot change required username when there are active connections.";

  required_username_ = std::string(username);
}

void IpcDesktopEnvironmentFactory::OnDesktopSessionAgentAttached(
    int terminal_id,
    mojo::ScopedMessagePipeHandle desktop_pipe) {
  if (!network_task_runner_->BelongsToCurrentThread()) {
    network_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &IpcDesktopEnvironmentFactory::OnDesktopSessionAgentAttached,
            base::Unretained(this), terminal_id, std::move(desktop_pipe)));
    return;
  }

  VLOG(1) << "IpcDesktopEnvironmentFactory::OnDesktopSessionAgentAttached() "
          << "terminal_id=" << terminal_id;

  auto it = connections_.find(terminal_id);
  if (it != connections_.end()) {
    DesktopSessionProxy* proxy = it->second.desktop_session_proxy;
    if (!proxy) {
      VLOG(1) << "Network: buffering desktop pipe for session " << terminal_id;
      it->second.pending_desktop_pipe = std::move(desktop_pipe);
      return;
    }
    proxy->DetachFromDesktop();
    proxy->AttachToDesktop(std::move(desktop_pipe));
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

  auto it = connections_.find(terminal_id);
  if (it != connections_.end()) {
    DesktopSessionProxy* desktop_session_proxy =
        it->second.desktop_session_proxy;
    connections_.erase(it);

    if (desktop_session_proxy) {
      // Disconnect the client session.
      desktop_session_proxy->DisconnectSession(
          ErrorCode::OK, "Terminal disconnected.", FROM_HERE);
    }
  }
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
void IpcDesktopEnvironmentFactory::OnSessionServicesClientConnected(
    int terminal_id,
    mojo::PendingReceiver<mojom::ChromotingSessionServices> receiver) {
  if (!network_task_runner_->BelongsToCurrentThread()) {
    network_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &IpcDesktopEnvironmentFactory::OnSessionServicesClientConnected,
            base::Unretained(this), terminal_id, std::move(receiver)));
    return;
  }

  auto it = connections_.find(terminal_id);
  if (it != connections_.end()) {
    DesktopSessionProxy* proxy = it->second.desktop_session_proxy;
    if (proxy) {
      proxy->OnSessionServicesClientConnected(std::move(receiver));
    } else {
      LOG(WARNING) << "ChromotingSessionServices bind request rejected: "
                   << "Terminal is not connected to any client.";
    }
  } else {
    LOG(WARNING) << "ChromotingSessionServices bind request rejected: "
                 << "Invalid terminal ID " << terminal_id;
  }
}
#endif

IpcDesktopEnvironmentFactory::ConnectionsList::iterator
IpcDesktopEnvironmentFactory::FindConnection(const DesktopSessionProxy* proxy) {
  return std::ranges::find_if(connections_, [proxy](const auto& pair) {
    return pair.second.desktop_session_proxy == proxy;
  });
}

}  // namespace remoting
