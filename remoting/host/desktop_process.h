// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_PROCESS_H_
#define REMOTING_HOST_DESKTOP_PROCESS_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "ipc/ipc_listener.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "remoting/host/desktop_session_agent.h"
#include "remoting/host/mojom/desktop_session.mojom.h"

namespace base {
class Location;
}

namespace IPC {
class ChannelProxy;
}  // namespace IPC

namespace remoting {

class AutoThreadTaskRunner;
class DesktopEnvironmentFactory;
class DesktopSessionAgent;

class DesktopProcess : public DesktopSessionAgent::Delegate,
                       public IPC::Listener,
                       public mojom::WorkerProcessControl {
 public:
  DesktopProcess(scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
                 scoped_refptr<AutoThreadTaskRunner> input_task_runner,
                 scoped_refptr<AutoThreadTaskRunner> io_task_runner,
                 mojo::ScopedMessagePipeHandle daemon_channel_handle);

  DesktopProcess(const DesktopProcess&) = delete;
  DesktopProcess& operator=(const DesktopProcess&) = delete;

  ~DesktopProcess() override;

  // DesktopSessionAgent::Delegate implementation.
  DesktopEnvironmentFactory& desktop_environment_factory() override;
  void OnNetworkProcessDisconnected() override;
  void CrashNetworkProcess(const base::Location& location) override;

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnChannelConnected(int32_t peer_pid) override;
  void OnChannelError() override;
  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override;

  // mojom::WorkerProcessControl implementation.
  void CrashProcess(const std::string& function_name,
                    const std::string& file_name,
                    int line_number) override;

  // Injects Secure Attention Sequence.
  void InjectSas();

  // Locks the workstation for the current session.
  void LockWorkstation();

  // Creates the desktop agent and required threads and IPC channels. Returns
  // true on success.
  bool Start(
      std::unique_ptr<DesktopEnvironmentFactory> desktop_environment_factory);

 private:
  // Task runner on which public methods of this class should be called.
  scoped_refptr<AutoThreadTaskRunner> caller_task_runner_;

  // Used to run input-related tasks.
  scoped_refptr<AutoThreadTaskRunner> input_task_runner_;

  // Used for IPC communication with Daemon process.
  scoped_refptr<AutoThreadTaskRunner> io_task_runner_;

  // Factory used to create integration components for use by |desktop_agent_|.
  std::unique_ptr<DesktopEnvironmentFactory> desktop_environment_factory_;

  // Handle for the IPC channel connecting the desktop process with the daemon
  // process.
  mojo::ScopedMessagePipeHandle daemon_channel_handle_;

  // IPC channel connecting the desktop process with the daemon process.
  std::unique_ptr<IPC::ChannelProxy> daemon_channel_;

  // Provides screen/audio capturing and input injection services for
  // the network process.
  scoped_refptr<DesktopSessionAgent> desktop_agent_;

  mojo::AssociatedRemote<mojom::DesktopSessionRequestHandler>
      desktop_session_request_handler_;

  mojo::AssociatedReceiver<mojom::WorkerProcessControl> worker_process_control_{
      this};

  base::WeakPtrFactory<DesktopProcess> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_PROCESS_H_
