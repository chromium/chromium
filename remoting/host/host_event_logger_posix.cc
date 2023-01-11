// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_event_logger.h"

// Included order is important, since the #define for LOG_USER in syslog.h
// conflicts with the constants in base/logging.h, and this source file should
// use the version in syslog.h.
// clang-format off
#include "base/logging.h"
#include <syslog.h>
// clang-format on

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "net/base/ip_endpoint.h"
#include "remoting/host/host_status_monitor.h"
#include "remoting/host/host_status_observer.h"
#include "remoting/protocol/transport.h"

namespace remoting {

namespace {

class HostEventLoggerPosix : public HostEventLogger, public HostStatusObserver {
 public:
  HostEventLoggerPosix(scoped_refptr<HostStatusMonitor> monitor,
                       const std::string& application_name);

  HostEventLoggerPosix(const HostEventLoggerPosix&) = delete;
  HostEventLoggerPosix& operator=(const HostEventLoggerPosix&) = delete;

  ~HostEventLoggerPosix() override;

  // HostStatusObserver implementation.  These methods will be called from the
  // network thread.
  void OnClientAuthenticated(const std::string& signaling_id) override;
  void OnClientDisconnected(const std::string& signaling_id) override;
  void OnClientAccessDenied(const std::string& signaling_id) override;
  void OnClientRouteChange(const std::string& signaling_id,
                           const std::string& channel_name,
                           const protocol::TransportRoute& route) override;
  void OnHostStarted(const std::string& user_email) override;
  void OnHostShutdown() override;

 private:
  void Log(const std::string& message);

  scoped_refptr<HostStatusMonitor> monitor_;
  std::string application_name_;
};

}  // namespace

HostEventLoggerPosix::HostEventLoggerPosix(
    scoped_refptr<HostStatusMonitor> monitor,
    const std::string& application_name)
    : monitor_(monitor), application_name_(application_name) {
  openlog(application_name_.c_str(), 0, LOG_USER);
  monitor_->AddStatusObserver(this);
}

HostEventLoggerPosix::~HostEventLoggerPosix() {
  monitor_->RemoveStatusObserver(this);
  closelog();
}

void HostEventLoggerPosix::OnClientAuthenticated(
    const std::string& signaling_id) {
  Log("Client connected: " + signaling_id);
}

void HostEventLoggerPosix::OnClientDisconnected(
    const std::string& signaling_id) {
  Log("Client disconnected: " + signaling_id);
}

void HostEventLoggerPosix::OnClientAccessDenied(
    const std::string& signaling_id) {
  Log("Access denied for client: " + signaling_id);
}

void HostEventLoggerPosix::OnClientRouteChange(
    const std::string& signaling_id,
    const std::string& channel_name,
    const protocol::TransportRoute& route) {
  Log(base::StringPrintf(
      "Channel IP for client: %s ip='%s' host_ip='%s' channel='%s' "
      "connection='%s'",
      signaling_id.c_str(),
      route.remote_address.address().IsValid()
          ? route.remote_address.ToString().c_str()
          : "unknown",
      route.local_address.address().IsValid()
          ? route.local_address.ToString().c_str()
          : "unknown",
      channel_name.c_str(),
      protocol::TransportRoute::GetTypeString(route.type).c_str()));
}

void HostEventLoggerPosix::OnHostShutdown() {
  // TODO(rmsousa): Fix host shutdown to actually call this, and add a log line.
}

void HostEventLoggerPosix::OnHostStarted(const std::string& user_email) {
  Log("Host started for user: " + user_email);
}

void HostEventLoggerPosix::Log(const std::string& message) {
  syslog(LOG_USER | LOG_NOTICE, "%s", message.c_str());
}

// static
std::unique_ptr<HostEventLogger> HostEventLogger::Create(
    scoped_refptr<HostStatusMonitor> monitor,
    const std::string& application_name) {
  return base::WrapUnique(new HostEventLoggerPosix(monitor, application_name));
}

}  // namespace remoting
