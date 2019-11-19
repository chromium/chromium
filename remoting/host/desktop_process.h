// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_PROCESS_H_
#define REMOTING_HOST_DESKTOP_PROCESS_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "ipc/ipc_listener.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "remoting/host/desktop_session_agent.h"

namespace IPC {
class ChannelProxy;
}  // namespace IPC

namespace remoting {

class AutoThreadTaskRunner;
class DesktopEnvironmentFactory;
class DesktopSessionAgent;

class DesktopProcess : public DesktopSessionAgent::Delegate,
                       public IPC::Listener {
 public:
  DesktopProcess(scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
                 scoped_refptr<AutoThreadTaskRunner> input_task_runner,
                 scoped_refptr<AutoThreadTaskRunner> io_task_runner,
                 mojo::ScopedMessagePipeHandle daemon_channel_handle);
  ~DesktopProcess() override;

  // DesktopSessionAgent::Delegate implementation.
  DesktopEnvironmentFactory& desktop_environment_factory() override;
  void OnNetworkProcessDisconnected() override;

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnChannelConnected(int32_t peer_pid) override;
  void OnChannelError() override;

  // Injects Secure Attention Sequence.
  void InjectSas();

  // Locks the workstation for the current session.
  void LockWorkstation();

  // Creates the desktop agent and required threads and IPC channels. Returns
  // true on success.
  bool Start(
      std::unique_ptr<DesktopEnvironmentFactory> desktop_environment_factory);

 private:
  // Crashes the process in response to a daemon's request. The daemon passes
  // the location of the code that detected the fatal error resulted in this
  // request. See the declaration of ChromotingDaemonMsg_Crash message.
  void OnCrash(const std::string& function_name,
               const std::string& file_name,
               const int& line_number);

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

  base::WeakPtrFactory<DesktopProcess> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DesktopProcess);
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_PROCESS_H_
