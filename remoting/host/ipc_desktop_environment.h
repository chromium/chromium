// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IPC_DESKTOP_ENVIRONMENT_H_
#define REMOTING_HOST_IPC_DESKTOP_ENVIRONMENT_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "remoting/host/active_display_monitor.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/desktop_session_connector.h"
#include "remoting/host/file_transfer/ipc_file_operations.h"
#include "remoting/host/mojom/desktop_session.mojom.h"
#include "remoting/host/mojom/remoting_host.mojom.h"
#include "remoting/protocol/desktop_capturer.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

class ClientSessionControl;
class DesktopSessionProxy;
class ScreenResolution;

// A variant of desktop environment integrating with the desktop by means of
// a helper process and talking to that process via IPC.
class IpcDesktopEnvironment : public DesktopEnvironment {
 public:
  // |desktop_session_connector| is used to bind DesktopSessionProxy to
  // a desktop session, to be notified every time the desktop process is
  // restarted.
  IpcDesktopEnvironment(
      scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      base::WeakPtr<ClientSessionControl> client_session_control,
      base::WeakPtr<ClientSessionEvents> client_session_events,
      base::WeakPtr<DesktopSessionConnector> desktop_session_connector,
      const DesktopEnvironmentOptions& options);

  IpcDesktopEnvironment(const IpcDesktopEnvironment&) = delete;
  IpcDesktopEnvironment& operator=(const IpcDesktopEnvironment&) = delete;

  ~IpcDesktopEnvironment() override;

  // DesktopEnvironment implementation.
  std::unique_ptr<ActionExecutor> CreateActionExecutor() override;
  std::unique_ptr<AudioCapturer> CreateAudioCapturer() override;
  std::unique_ptr<InputInjector> CreateInputInjector() override;
  std::unique_ptr<ScreenControls> CreateScreenControls() override;
  std::unique_ptr<DesktopCapturer> CreateVideoCapturer(
      webrtc::ScreenId id) override;
  DesktopDisplayInfoMonitor* GetDisplayInfoMonitor() override;
  std::unique_ptr<webrtc::MouseCursorMonitor> CreateMouseCursorMonitor()
      override;
  std::unique_ptr<KeyboardLayoutMonitor> CreateKeyboardLayoutMonitor(
      base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback)
      override;
  std::unique_ptr<ActiveDisplayMonitor> CreateActiveDisplayMonitor(
      ActiveDisplayMonitor::Callback callback) override;
  std::unique_ptr<FileOperations> CreateFileOperations() override;
  std::unique_ptr<UrlForwarderConfigurator> CreateUrlForwarderConfigurator()
      override;
  std::string GetCapabilities() const override;
  void SetCapabilities(const std::string& capabilities) override;
  uint32_t GetDesktopSessionId() const override;
  std::unique_ptr<RemoteWebAuthnStateChangeNotifier>
  CreateRemoteWebAuthnStateChangeNotifier() override;

 private:
  scoped_refptr<DesktopSessionProxy> desktop_session_proxy_;
};

// Used to create IpcDesktopEnvironment objects integrating with the desktop via
// a helper process and talking to that process via IPC.
class IpcDesktopEnvironmentFactory : public DesktopEnvironmentFactory,
                                     public DesktopSessionConnector {
 public:
  // Passes a reference to the IPC channel connected to the daemon process and
  // relevant task runners. |remote| must be released on |network_task_runner|.
  IpcDesktopEnvironmentFactory(
      scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      mojo::AssociatedRemote<mojom::DesktopSessionManager> remote);

  IpcDesktopEnvironmentFactory(const IpcDesktopEnvironmentFactory&) = delete;
  IpcDesktopEnvironmentFactory& operator=(const IpcDesktopEnvironmentFactory&) =
      delete;

  ~IpcDesktopEnvironmentFactory() override;

  // DesktopEnvironmentFactory implementation.
  std::unique_ptr<DesktopEnvironment> Create(
      base::WeakPtr<ClientSessionControl> client_session_control,
      base::WeakPtr<ClientSessionEvents> client_session_events,
      const DesktopEnvironmentOptions& options) override;
  bool SupportsAudioCapture() const override;

  // DesktopSessionConnector implementation.
  void ConnectTerminal(DesktopSessionProxy* desktop_session_proxy,
                       const ScreenResolution& resolution,
                       bool virtual_terminal) override;
  void DisconnectTerminal(DesktopSessionProxy* desktop_session_proxy) override;
  void SetScreenResolution(DesktopSessionProxy* desktop_session_proxy,
                           const ScreenResolution& resolution) override;
  bool BindConnectionEventsReceiver(
      mojo::ScopedInterfaceEndpointHandle handle) override;
  void OnDesktopSessionAgentAttached(
      int terminal_id,
      int session_id,
      mojo::ScopedMessagePipeHandle desktop_pipe) override;
  void OnTerminalDisconnected(int terminal_id) override;

 private:
  // Used to run the audio capturer.
  scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner_;

  // Task runner on which DesktopEnvironmentFactory methods should be called.
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;

  // Task runner used for running background I/O.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // List of DesktopEnvironment instances we've told the daemon process about.
  typedef std::map<int, raw_ptr<DesktopSessionProxy, CtnExperimental>>
      ActiveConnectionsList;
  ActiveConnectionsList active_connections_;

  // Next desktop session ID. IDs are allocated sequentially starting from 0.
  // This gives us more than 67 years of unique IDs assuming a new ID is
  // allocated every second.
  int next_id_ = 0;

  mojo::AssociatedRemote<mojom::DesktopSessionManager> desktop_session_manager_;

  mojo::AssociatedReceiver<mojom::DesktopSessionConnectionEvents>
      desktop_session_connection_events_{this};

  // Factory for weak pointers to DesktopSessionConnector interface.
  base::WeakPtrFactory<DesktopSessionConnector> connector_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_IPC_DESKTOP_ENVIRONMENT_H_
