// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IPC_DESKTOP_ENVIRONMENT_H_
#define REMOTING_HOST_IPC_DESKTOP_ENVIRONMENT_H_

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "remoting/base/errors.h"
#include "remoting/base/source_location.h"
#include "remoting/host/active_display_monitor.h"
#include "remoting/host/base/desktop_environment_options.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/desktop_session_connector.h"
#include "remoting/host/mojom/desktop_session.mojom.h"
#include "remoting/host/mojom/remoting_host.mojom.h"
#include "remoting/protocol/desktop_capturer.h"
#include "remoting/protocol/mouse_cursor_monitor.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

class ClientSessionControl;
class IpcFifoBufferReader;
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
  std::unique_ptr<protocol::MouseCursorMonitor> CreateMouseCursorMonitor()
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
  std::unique_ptr<RemoteWebAuthnStateChangeNotifier>
  CreateRemoteWebAuthnStateChangeNotifier() override;
  std::unique_ptr<AudioInjector> CreateAudioInjector(
      std::unique_ptr<IpcFifoBufferReader> reader) override;

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
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      mojo::AssociatedRemote<mojom::DesktopSessionManager> remote);

  IpcDesktopEnvironmentFactory(const IpcDesktopEnvironmentFactory&) = delete;
  IpcDesktopEnvironmentFactory& operator=(const IpcDesktopEnvironmentFactory&) =
      delete;

  ~IpcDesktopEnvironmentFactory() override;

  // DesktopEnvironmentFactory implementation.
  void Create(base::WeakPtr<ClientSessionControl> client_session_control,
              base::WeakPtr<ClientSessionEvents> client_session_events,
              const DesktopEnvironmentOptions& options,
              CreateCallback callback) override;
  bool SupportsAudioCapture() const override;

  // DesktopSessionConnector implementation.
  void ConnectTerminal(DesktopSessionProxy* desktop_session_proxy,
                       const ScreenResolution& resolution,
                       bool is_curtained) override;
  void DisconnectTerminal(DesktopSessionProxy* desktop_session_proxy) override;
  void SetScreenResolution(DesktopSessionProxy* desktop_session_proxy,
                           const ScreenResolution& resolution) override;
  bool BindConnectionEventsReceiver(
      mojo::ScopedInterfaceEndpointHandle handle) override;
  void SetRequiredUsername(std::string_view username) override;
  void OnDesktopSessionAgentAttached(
      int terminal_id,
      mojo::ScopedMessagePipeHandle desktop_pipe) override;
  void OnTerminalDisconnected(int terminal_id,
                              ErrorCode error_code,
                              const std::string& error_details,
                              const SourceLocation& error_location) override;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  void OnSessionServicesClientConnected(
      int terminal_id,
      mojo::PendingReceiver<mojom::ChromotingSessionServices> receiver)
      override;
#endif

 private:
  friend class IpcDesktopEnvironmentTest;

  class Core;

  struct DesktopConnection {
    DesktopConnection(DesktopSessionProxy* desktop_session_proxy,
                      std::string_view client_id);
    ~DesktopConnection();

    DesktopConnection(DesktopConnection&&);
    DesktopConnection& operator=(DesktopConnection&&);

    // If `persist_desktop_sessions_` is true, this will be nullptr whenever
    // the client has disconnected.
    //
    // DisableDanglingPtrDetection is needed because `DesktopSessionProxy` is
    // owned by `IpcDesktopEnvironment` (destructed on the UI thread), whereas
    // `DesktopConnection` lives in `Core` (destructed on the network thread
    // via `OnTaskRunnerDeleter`). During teardown, `DesktopSessionProxy` may
    // be freed before `~Core()` destroys `connections_`. This is safe because
    // `desktop_session_proxy` is never dereferenced after
    // `IpcDesktopEnvironment` is torn down.
    raw_ptr<DesktopSessionProxy, DisableDanglingPtrDetection>
        desktop_session_proxy;

    // The identifier of the CRD client to ensure the correct desktop session
    // is reused in case the host is configured to accept connections from
    // multiple client users.
    std::string client_id;

    // A pipe that was received before the `desktop_session_proxy` was set.
    mojo::ScopedMessagePipeHandle pending_desktop_pipe;
  };

  void set_persist_desktop_sessions_for_testing(bool persistent);
  size_t active_desktop_sessions_count_for_testing() const;
  const DesktopConnection* GetConnectionForTesting(int terminal_id) const;

  // Task runner on which DesktopEnvironmentFactory methods should be called.
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;

  // Task runner used for running background I/O.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  std::unique_ptr<Core, base::OnTaskRunnerDeleter> core_;

  mojo::AssociatedReceiver<mojom::DesktopSessionConnectionEvents>
      desktop_session_connection_events_{this};

  // Factory for weak pointers to DesktopSessionConnector interface.
  base::WeakPtrFactory<DesktopSessionConnector> connector_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_IPC_DESKTOP_ENVIRONMENT_H_
