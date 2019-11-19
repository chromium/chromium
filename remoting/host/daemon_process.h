// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DAEMON_PROCESS_H_
#define REMOTING_HOST_DAEMON_PROCESS_H_

#include <stdint.h>

#include <list>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/process/process.h"
#include "base/time/time.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_handle.h"
#include "remoting/host/config_watcher.h"
#include "remoting/host/current_process_stats_agent.h"
#include "remoting/host/host_status_monitor.h"
#include "remoting/host/worker_process_ipc_delegate.h"
#include "remoting/protocol/process_stats_stub.h"

struct SerializedTransportRoute;

namespace base {
class Location;
}  // namespace base

namespace remoting {

class AutoThreadTaskRunner;
class DesktopSession;
class HostEventLogger;
class HostStatusObserver;
class ProcessStatsSender;
class ScreenResolution;

// This class implements core of the daemon process. It manages the networking
// process running at lower privileges and maintains the list of desktop
// sessions.
class DaemonProcess
    : public ConfigWatcher::Delegate,
      public WorkerProcessIpcDelegate,
      public protocol::ProcessStatsStub {
 public:
  typedef std::list<DesktopSession*> DesktopSessionList;

  ~DaemonProcess() override;

  // Creates a platform-specific implementation of the daemon process object
  // passing relevant task runners. Public methods of this class must be called
  // on the |caller_task_runner| thread. |io_task_runner| is used to handle IPC
  // and background I/O tasks.
  static std::unique_ptr<DaemonProcess> Create(
      scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
      scoped_refptr<AutoThreadTaskRunner> io_task_runner,
      const base::Closure& stopped_callback);

  // ConfigWatcher::Delegate
  void OnConfigUpdated(const std::string& serialized_config) override;
  void OnConfigWatcherError() override;

  scoped_refptr<HostStatusMonitor> status_monitor() { return status_monitor_; }

  // WorkerProcessIpcDelegate implementation.
  void OnChannelConnected(int32_t peer_pid) override;
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnPermanentError(int exit_code) override;
  void OnWorkerProcessStopped() override;

  // Sends an IPC message to the network process. The message will be dropped
  // unless the network process is connected over the IPC channel.
  virtual void SendToNetwork(IPC::Message* message) = 0;

  // Called when a desktop integration process attaches to |terminal_id|.
  // |session_id| is the id of the desktop session being attached.
  // |desktop_pipe| specifies the client end of the desktop pipe. Returns true
  // on success, false otherwise.
  virtual bool OnDesktopSessionAgentAttached(
      int terminal_id,
      int session_id,
      const IPC::ChannelHandle& desktop_pipe) = 0;

  // Closes the desktop session identified by |terminal_id|.
  void CloseDesktopSession(int terminal_id);

 protected:
  DaemonProcess(scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
                scoped_refptr<AutoThreadTaskRunner> io_task_runner,
                const base::Closure& stopped_callback);

  // Creates a desktop session and assigns a unique ID to it.
  void CreateDesktopSession(int terminal_id,
                            const ScreenResolution& resolution,
                            bool virtual_terminal);

  // Changes the screen resolution of the desktop session identified by
  // |terminal_id|.
  void SetScreenResolution(int terminal_id, const ScreenResolution& resolution);

  // Requests the network process to crash.
  void CrashNetworkProcess(const base::Location& location);

  // Reads the host configuration and launches the network process.
  void Initialize();

  // Invokes |stopped_callback_| to ask the owner to delete |this|.
  void Stop();

  // Returns true if |terminal_id| is in the range of allocated IDs. I.e. it is
  // less or equal to the highest ID we have seen so far.
  bool WasTerminalIdAllocated(int terminal_id);

  // Handlers for the host status notifications received from the network
  // process.
  void OnAccessDenied(const std::string& jid);
  void OnClientAuthenticated(const std::string& jid);
  void OnClientConnected(const std::string& jid);
  void OnClientDisconnected(const std::string& jid);
  void OnClientRouteChange(const std::string& jid,
                           const std::string& channel_name,
                           const SerializedTransportRoute& route);
  void OnHostStarted(const std::string& xmpp_login);
  void OnHostShutdown();

  // Creates a platform-specific desktop session and assigns a unique ID to it.
  // An implementation should validate |params| as they are received via IPC.
  virtual std::unique_ptr<DesktopSession> DoCreateDesktopSession(
      int terminal_id,
      const ScreenResolution& resolution,
      bool virtual_terminal) = 0;

  // Requests the network process to crash.
  virtual void DoCrashNetworkProcess(const base::Location& location) = 0;

  // Launches the network process and establishes an IPC channel with it.
  virtual void LaunchNetworkProcess() = 0;

  scoped_refptr<AutoThreadTaskRunner> caller_task_runner() {
    return caller_task_runner_;
  }

  scoped_refptr<AutoThreadTaskRunner> io_task_runner() {
    return io_task_runner_;
  }

  // Let the test code analyze the list of desktop sessions.
  friend class DaemonProcessTest;
  const DesktopSessionList& desktop_sessions() const {
    return desktop_sessions_;
  }

 private:
  // Deletes all desktop sessions.
  void DeleteAllDesktopSessions();

  // Starts to report process statistic data to network process. If |interval|
  // is less then or equal to 0, a default non-zero value will be used.
  void StartProcessStatsReport(base::TimeDelta interval);

  // Stops sending process statistic data to network process.
  void StopProcessStatsReport();

  // ProcessStatsStub implementation.
  void OnProcessStats(
      const protocol::AggregatedProcessResourceUsage& usage) override;

  // Gets the location of the config file.
  base::FilePath GetConfigPath();

  // Updates the config file, replacing the current OAuth refresh token with the
  // one provided.
  void UpdateConfigRefreshToken(const std::string& token);
  static void UpdateConfigRefreshTokenOnIoThread(
      const base::FilePath& config_file,
      const std::string& token);

  // Task runner on which public methods of this class must be called.
  scoped_refptr<AutoThreadTaskRunner> caller_task_runner_;

  // Handles IPC and background I/O tasks.
  scoped_refptr<AutoThreadTaskRunner> io_task_runner_;

  std::unique_ptr<ConfigWatcher> config_watcher_;

  // The configuration file contents.
  std::string serialized_config_;

  // The list of active desktop sessions.
  DesktopSessionList desktop_sessions_;

  // The highest desktop session ID that has been seen so far.
  int next_terminal_id_;

  // Keeps track of observers receiving host status notifications.
  base::ObserverList<HostStatusObserver>::Unchecked status_observers_;

  // Invoked to ask the owner to delete |this|.
  base::Closure stopped_callback_;

  // Writes host status updates to the system event log.
  std::unique_ptr<HostEventLogger> host_event_logger_;

  scoped_refptr<HostStatusMonitor> status_monitor_;

  // Reports process statistic data to network process.
  std::unique_ptr<ProcessStatsSender> stats_sender_;

  // The number of StartProcessStatsReport requests received.
  // Daemon and Network processes manages multiple desktop sessions. Some of
  // them may request for process statistic reports. So the resource usage of
  // daemon process and network process will be merged to each desktop session.
  //
  // As long as at least process statistic reports is enabled for one desktop
  // session, daemon process should continually send the reports.
  int process_stats_request_count_ = 0;

  CurrentProcessStatsAgent current_process_stats_;

  DISALLOW_COPY_AND_ASSIGN(DaemonProcess);
};

}  // namespace remoting

#endif  // REMOTING_HOST_DAEMON_PROCESS_H_
