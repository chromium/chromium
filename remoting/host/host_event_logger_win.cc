// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_event_logger.h"

#include <windows.h>

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/ip_endpoint.h"
#include "remoting/host/host_status_monitor.h"
#include "remoting/host/host_status_observer.h"
#include "remoting/host/win/remoting_host_messages.h"
#include "remoting/host/win/windows_event_logger.h"
#include "remoting/protocol/transport.h"

namespace remoting {

namespace {

class HostEventLoggerWin : public HostEventLogger, public HostStatusObserver {
 public:
  HostEventLoggerWin(scoped_refptr<HostStatusMonitor> monitor,
                     const std::string& application_name);

  HostEventLoggerWin(const HostEventLoggerWin&) = delete;
  HostEventLoggerWin& operator=(const HostEventLoggerWin&) = delete;

  ~HostEventLoggerWin() override;

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
  void LogString(WORD type, DWORD event_id, const std::string& string);
  void Log(WORD type, DWORD event_id, const std::vector<std::string>& strings);

  scoped_refptr<HostStatusMonitor> monitor_;

  WindowsEventLogger event_logger_;
};

}  // namespace

HostEventLoggerWin::HostEventLoggerWin(scoped_refptr<HostStatusMonitor> monitor,
                                       const std::string& application_name)
    : monitor_(monitor), event_logger_(application_name) {
  if (event_logger_.IsRegistered()) {
    monitor_->AddStatusObserver(this);
  } else {
    PLOG(ERROR) << "Failed to register the event source: " << application_name;
  }
}

HostEventLoggerWin::~HostEventLoggerWin() {
  if (event_logger_.IsRegistered()) {
    monitor_->RemoveStatusObserver(this);
  }
}

void HostEventLoggerWin::OnClientAuthenticated(
    const std::string& signaling_id) {
  LogString(EVENTLOG_INFORMATION_TYPE, MSG_HOST_CLIENT_CONNECTED, signaling_id);
}

void HostEventLoggerWin::OnClientDisconnected(const std::string& signaling_id) {
  LogString(EVENTLOG_INFORMATION_TYPE, MSG_HOST_CLIENT_DISCONNECTED,
            signaling_id);
}

void HostEventLoggerWin::OnClientAccessDenied(const std::string& signaling_id) {
  LogString(EVENTLOG_ERROR_TYPE, MSG_HOST_CLIENT_ACCESS_DENIED, signaling_id);
}

void HostEventLoggerWin::OnClientRouteChange(
    const std::string& signaling_id,
    const std::string& channel_name,
    const protocol::TransportRoute& route) {
  std::vector<std::string> strings(5);
  strings[0] = signaling_id;
  strings[1] = route.remote_address.address().IsValid()
                   ? route.remote_address.ToString()
                   : "unknown";
  strings[2] = route.local_address.address().IsValid()
                   ? route.local_address.ToString()
                   : "unknown";
  strings[3] = channel_name;
  strings[4] = protocol::TransportRoute::GetTypeString(route.type);
  Log(EVENTLOG_INFORMATION_TYPE, MSG_HOST_CLIENT_ROUTING_CHANGED, strings);
}

void HostEventLoggerWin::OnHostShutdown() {
  // TODO(rmsousa): Fix host shutdown to actually call this, and add a log line.
}

void HostEventLoggerWin::OnHostStarted(const std::string& user_email) {
  LogString(EVENTLOG_INFORMATION_TYPE, MSG_HOST_STARTED, user_email);
}

void HostEventLoggerWin::Log(WORD type,
                             DWORD event_id,
                             const std::vector<std::string>& strings) {
  if (!event_logger_.Log(type, event_id, strings)) {
    PLOG(ERROR) << "Failed to write an event to the event log";
  }
}

void HostEventLoggerWin::LogString(WORD type,
                                   DWORD event_id,
                                   const std::string& string) {
  std::vector<std::string> strings;
  strings.push_back(string);
  Log(type, event_id, strings);
}

// static
std::unique_ptr<HostEventLogger> HostEventLogger::Create(
    scoped_refptr<HostStatusMonitor> monitor,
    const std::string& application_name) {
  return std::make_unique<HostEventLoggerWin>(monitor, application_name);
}

}  // namespace remoting
