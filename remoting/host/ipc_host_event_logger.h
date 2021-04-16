// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IPC_HOST_EVENT_LOGGER_H_
#define REMOTING_HOST_IPC_HOST_EVENT_LOGGER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "remoting/host/host_event_logger.h"
#include "remoting/host/host_status_observer.h"

namespace IPC {
class Sender;
}  // namespace IPC

namespace remoting {

class HostStatusMonitor;

class IpcHostEventLogger : public HostEventLogger, public HostStatusObserver {
 public:
  // Initializes the logger. |daemon_channel| must outlive this object.
  IpcHostEventLogger(scoped_refptr<HostStatusMonitor> monitor,
                     IPC::Sender* daemon_channel);
  ~IpcHostEventLogger() override;

  // HostStatusObserver interface.
  void OnAccessDenied(const std::string& jid) override;
  void OnClientAuthenticated(const std::string& jid) override;
  void OnClientConnected(const std::string& jid) override;
  void OnClientDisconnected(const std::string& jid) override;
  void OnClientRouteChange(const std::string& jid,
                           const std::string& channel_name,
                           const protocol::TransportRoute& route) override;
  void OnStart(const std::string& xmpp_login) override;
  void OnShutdown() override;

 private:
  // Used to report host status events to the daemon.
  IPC::Sender* daemon_channel_;

  scoped_refptr<HostStatusMonitor> monitor_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(IpcHostEventLogger);
};

}

#endif  // REMOTING_HOST_IPC_HOST_EVENT_LOGGER_H_
