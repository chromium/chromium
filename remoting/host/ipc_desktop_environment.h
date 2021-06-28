// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IPC_DESKTOP_ENVIRONMENT_H_
#define REMOTING_HOST_IPC_DESKTOP_ENVIRONMENT_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "ipc/ipc_channel_handle.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/desktop_session_connector.h"
#include "remoting/host/file_transfer/ipc_file_operations.h"

namespace base {
class SingleThreadTaskRunner;
}  // base

namespace IPC {
class Sender;
}  // namespace IPC

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
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      base::WeakPtr<ClientSessionControl> client_session_control,
      base::WeakPtr<DesktopSessionConnector> desktop_session_connector,
      const DesktopEnvironmentOptions& options);
  ~IpcDesktopEnvironment() override;

  // DesktopEnvironment implementation.
  std::unique_ptr<ActionExecutor> CreateActionExecutor() override;
  std::unique_ptr<AudioCapturer> CreateAudioCapturer() override;
  std::unique_ptr<InputInjector> CreateInputInjector() override;
  std::unique_ptr<ScreenControls> CreateScreenControls() override;
  std::unique_ptr<webrtc::DesktopCapturer> CreateVideoCapturer() override;
  std::unique_ptr<webrtc::MouseCursorMonitor> CreateMouseCursorMonitor()
      override;
  std::unique_ptr<KeyboardLayoutMonitor> CreateKeyboardLayoutMonitor(
      base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback)
      override;
  std::unique_ptr<FileOperations> CreateFileOperations() override;
  std::string GetCapabilities() const override;
  void SetCapabilities(const std::string& capabilities) override;
  uint32_t GetDesktopSessionId() const override;
  std::unique_ptr<DesktopAndCursorConditionalComposer>
  CreateComposingVideoCapturer() override;

 private:
  scoped_refptr<DesktopSessionProxy> desktop_session_proxy_;

  DISALLOW_COPY_AND_ASSIGN(IpcDesktopEnvironment);
};

// Used to create IpcDesktopEnvironment objects integrating with the desktop via
// a helper process and talking to that process via IPC.
class IpcDesktopEnvironmentFactory
    : public DesktopEnvironmentFactory,
      public DesktopSessionConnector {
 public:
  // Passes a reference to the IPC channel connected to the daemon process and
  // relevant task runners. |daemon_channel| must outlive this object.
  IpcDesktopEnvironmentFactory(
      scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      IPC::Sender* daemon_channel);
  ~IpcDesktopEnvironmentFactory() override;

  // DesktopEnvironmentFactory implementation.
  std::unique_ptr<DesktopEnvironment> Create(
      base::WeakPtr<ClientSessionControl> client_session_control,
      const DesktopEnvironmentOptions& options) override;
  bool SupportsAudioCapture() const override;

  // DesktopSessionConnector implementation.
  void ConnectTerminal(DesktopSessionProxy* desktop_session_proxy,
                       const ScreenResolution& resolution,
                       bool virtual_terminal) override;
  void DisconnectTerminal(DesktopSessionProxy* desktop_session_proxy) override;
  void SetScreenResolution(DesktopSessionProxy* desktop_session_proxy,
                           const ScreenResolution& resolution) override;
  void OnDesktopSessionAgentAttached(
      int terminal_id,
      int session_id,
      const IPC::ChannelHandle& desktop_pipe) override;
  void OnTerminalDisconnected(int terminal_id) override;

 private:
  // Used to run the audio capturer.
  scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner_;

  // Task runner on which methods of DesktopEnvironmentFactory interface should
  // be called.
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

  // Task runner used for running background I/O.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // IPC channel connected to the daemon process.
  IPC::Sender* daemon_channel_;

  // List of DesktopEnvironment instances we've told the daemon process about.
  typedef std::map<int, DesktopSessionProxy*> ActiveConnectionsList;
  ActiveConnectionsList active_connections_;

  // Next desktop session ID. IDs are allocated sequentially starting from 0.
  // This gives us more than 67 years of unique IDs assuming a new ID is
  // allocated every second.
  int next_id_ = 0;

  // Factory for weak pointers to DesktopSessionConnector interface.
  base::WeakPtrFactory<DesktopSessionConnector> connector_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(IpcDesktopEnvironmentFactory);
};

}  // namespace remoting

#endif  // REMOTING_HOST_IPC_DESKTOP_ENVIRONMENT_H_
