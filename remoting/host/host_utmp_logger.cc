// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_utmp_logger.h"

#include <pty.h>
#include <unistd.h>
#include <utempter.h>

#include "remoting/base/logging.h"
#include "remoting/host/host_status_monitor.h"

namespace remoting {

namespace {

// Name to pass to utempter_add_record as ut_host.
constexpr char kApplicationName[] = "chromoting";

}  // namespace

HostUTMPLogger::HostUTMPLogger(scoped_refptr<HostStatusMonitor> monitor)
    : monitor_(monitor) {
  monitor_->AddStatusObserver(this);
}

HostUTMPLogger::~HostUTMPLogger() {
  monitor_->RemoveStatusObserver(this);
}

void HostUTMPLogger::OnClientConnected(const std::string& signaling_id) {
  int pty, replica_pty;
  if (openpty(&pty, &replica_pty, nullptr, nullptr, nullptr)) {
    PLOG(ERROR) << "Failed to open pty for utmp logging";
    return;
  }
  close(replica_pty);
  pty_.emplace(signaling_id, pty);
  utempter_add_record(pty, kApplicationName);
}

void HostUTMPLogger::OnClientDisconnected(const std::string& signaling_id) {
  auto pty_iter = pty_.find(signaling_id);
  if (pty_iter != pty_.end()) {
    utempter_remove_record(pty_iter->second);
    close(pty_iter->second);
  }
}

}  // namespace remoting
