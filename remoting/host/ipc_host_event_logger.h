// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IPC_HOST_EVENT_LOGGER_H_
#define REMOTING_HOST_IPC_HOST_EVENT_LOGGER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "remoting/host/host_event_logger.h"
#include "remoting/host/host_status_observer.h"

namespace remoting {

class HostStatusMonitor;

class IpcHostEventLogger : public HostEventLogger, public HostStatusObserver {
 public:
  IpcHostEventLogger(scoped_refptr<HostStatusMonitor> monitor,
                     mojo::AssociatedRemote<mojom::HostStatusObserver> remote);

  IpcHostEventLogger(const IpcHostEventLogger&) = delete;
  IpcHostEventLogger& operator=(const IpcHostEventLogger&) = delete;

  ~IpcHostEventLogger() override;

  // HostStatusObserver overrides.
  void OnClientAccessDenied(const std::string& signaling_id) override;
  void OnClientAuthenticated(const std::string& signaling_id) override;
  void OnClientConnected(const std::string& signaling_id) override;
  void OnClientDisconnected(const std::string& signaling_id) override;
  void OnClientRouteChange(const std::string& signaling_id,
                           const std::string& channel_name,
                           const protocol::TransportRoute& route) override;
  void OnHostStarted(const std::string& user_email) override;
  void OnHostShutdown() override;

 private:
  // Used to report host status events to the daemon.
  mojo::AssociatedRemote<mojom::HostStatusObserver> host_status_observer_;

  scoped_refptr<HostStatusMonitor> monitor_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace remoting

#endif  // REMOTING_HOST_IPC_HOST_EVENT_LOGGER_H_
