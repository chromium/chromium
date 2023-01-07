// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_UTMP_LOGGER_H_
#define REMOTING_HOST_HOST_UTMP_LOGGER_H_

#include "base/containers/flat_map.h"
#include "remoting/host/host_status_observer.h"

namespace remoting {

class HostStatusMonitor;
// HostUTMPLogger records client connect/disconnect to a local log file.
class HostUTMPLogger : public HostStatusObserver {
 public:
  explicit HostUTMPLogger(scoped_refptr<HostStatusMonitor> monitor);

  HostUTMPLogger(const HostUTMPLogger&) = delete;
  HostUTMPLogger& operator=(const HostUTMPLogger&) = delete;

  ~HostUTMPLogger() override;

  // HostStatusObserver interface.
  void OnClientConnected(const std::string& signaling_id) override;
  void OnClientDisconnected(const std::string& signaling_id) override;

 private:
  // A map from client signaling ID to pseudoterminal file descriptor.
  base::flat_map<std::string, int> pty_;

  scoped_refptr<HostStatusMonitor> monitor_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_UTMP_LOGGER_H_
