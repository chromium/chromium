// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_CLIENT_TELEMETRY_LOGGER_H_
#define REMOTING_CLIENT_CLIENT_TELEMETRY_LOGGER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "remoting/base/chromoting_event.h"
#include "remoting/base/chromoting_event_log_writer.h"
#include "remoting/base/url_request.h"
#include "remoting/protocol/connection_to_host.h"
#include "remoting/protocol/performance_tracker.h"
#include "remoting/protocol/transport.h"

namespace remoting {

// ClientTelemetryLogger sends client log entries to the telemetry server.
// The logger should be used entirely on one single thread after it is created.
// TODO(yuweih): Implement new features that session_logger.js provides.
class ClientTelemetryLogger {
 public:
  // |log_writer| must outlive ClientTelemetryLogger.
  ClientTelemetryLogger(ChromotingEventLogWriter* log_writer,
                        ChromotingEvent::Mode mode,
                        ChromotingEvent::SessionEntryPoint entry_point);
  ~ClientTelemetryLogger();

  void SetAuthMethod(ChromotingEvent::AuthMethod auth_method);

  // Sets the host info to be posted along with other log data. By default
  // no host info will be logged.
  void SetHostInfo(const std::string& host_version,
                   ChromotingEvent::Os host_os,
                   const std::string& host_os_version);

  void SetSignalStrategyType(ChromotingEvent::SignalStrategyType type);

  void SetTransportRoute(const protocol::TransportRoute& route);

  void LogSessionStateChange(ChromotingEvent::SessionState state,
                             ChromotingEvent::ConnectionError error);

  void LogStatistics(const protocol::PerformanceTracker& perf_tracker);

  const std::string& session_id() const { return session_id_; }

  void SetSessionIdGenerationTimeForTest(base::TimeTicks gen_time);

  const ChromotingEvent& current_session_state_event() const {
    return current_session_state_event_;
  }

  static ChromotingEvent::SessionState TranslateState(
      protocol::ConnectionToHost::State current_state,
      protocol::ConnectionToHost::State previous_state);

  static ChromotingEvent::ConnectionError TranslateError(
      protocol::ErrorCode state);

  static ChromotingEvent::ConnectionType TranslateConnectionType(
      protocol::TransportRoute::RouteType type);

 private:
  struct HostInfo;

  void FillEventContext(ChromotingEvent* event) const;

  // Generates a new random session ID.
  void GenerateSessionId();

  void PrintLogStatistics(const protocol::PerformanceTracker& perf_tracker);

  // If not session ID has been set, simply generates a new one without sending
  // any logs, otherwise expire the session ID if the maximum duration has been
  // exceeded, and sends SessionIdOld and SessionIdNew events describing the
  // change of id.
  void RefreshSessionIdIfOutdated();

  ChromotingEvent MakeStatsEvent(
      const protocol::PerformanceTracker& perf_tracker);
  ChromotingEvent MakeSessionStateChangeEvent(
      ChromotingEvent::SessionState state,
      ChromotingEvent::ConnectionError error);
  ChromotingEvent MakeSessionIdOldEvent();
  ChromotingEvent MakeSessionIdNewEvent();

  // A randomly generated session ID to be attached to log messages. This
  // is regenerated at the start of a new session.
  std::string session_id_;

  base::TimeTicks session_start_time_;

  base::TimeTicks session_id_generation_time_;

  ChromotingEvent current_session_state_event_;

  ChromotingEvent::AuthMethod auth_method_ =
      ChromotingEvent::AuthMethod::NOT_SET;

  ChromotingEvent::Mode mode_;

  ChromotingEvent::SessionEntryPoint entry_point_;

  ChromotingEvent::SignalStrategyType signal_strategy_type_ =
      ChromotingEvent::SignalStrategyType::NOT_SET;

  std::unique_ptr<HostInfo> host_info_;
  std::unique_ptr<protocol::TransportRoute> transport_route_;

  // The log writer that actually sends log to the server.
  ChromotingEventLogWriter* log_writer_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(ClientTelemetryLogger);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_CLIENT_TELEMETRY_LOGGER_H_
