// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_status_logger.h"

#include "base/bind.h"
#include "remoting/base/constants.h"
#include "remoting/host/host_status_monitor.h"
#include "remoting/host/server_log_entry_host.h"
#include "remoting/protocol/transport.h"
#include "remoting/signaling/server_log_entry.h"

namespace remoting {

HostStatusLogger::HostStatusLogger(scoped_refptr<HostStatusMonitor> monitor,
                                   LogToServer* log_to_server)
    : log_to_server_(log_to_server), monitor_(monitor) {
  DCHECK(log_to_server_);
  DCHECK(monitor_);
  monitor_->AddStatusObserver(this);
}

HostStatusLogger::~HostStatusLogger() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  monitor_->RemoveStatusObserver(this);
}

void HostStatusLogger::LogSessionStateChange(const std::string& jid,
                                             bool connected) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<ServerLogEntry> entry(
      MakeLogEntryForSessionStateChange(connected));
  AddHostFieldsToLogEntry(entry.get());
  entry->AddModeField(log_to_server_->mode());

  if (connected && connection_route_type_.count(jid) > 0)
    AddConnectionTypeToLogEntry(entry.get(), connection_route_type_[jid]);

  log_to_server_->Log(*entry.get());
}

void HostStatusLogger::OnClientConnected(const std::string& jid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LogSessionStateChange(jid, true);
}

void HostStatusLogger::OnClientDisconnected(const std::string& jid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LogSessionStateChange(jid, false);
  connection_route_type_.erase(jid);
}

void HostStatusLogger::OnClientRouteChange(
    const std::string& jid,
    const std::string& channel_name,
    const protocol::TransportRoute& route) {
  // Store connection type for the video channel. It is logged later
  // when client authentication is finished.
  if (channel_name == kVideoChannelName) {
    connection_route_type_[jid] = route.type;
  }
}

}  // namespace remoting
