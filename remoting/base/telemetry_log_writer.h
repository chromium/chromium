// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_TELEMETRY_LOG_WRITER_H_
#define REMOTING_BASE_TELEMETRY_LOG_WRITER_H_

#include <string>

#include "base/callback.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "net/base/backoff_entry.h"
#include "remoting/base/chromoting_event.h"
#include "remoting/base/chromoting_event_log_writer.h"
#include "remoting/base/grpc_support/grpc_authenticated_executor.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/base/url_request.h"
#include "remoting/proto/remoting/v1/telemetry_service.grpc.pb.h"

namespace remoting {

// TelemetryLogWriter sends log entries (ChromotingEvent) to the telemetry
// server.
// Logs to be sent will be queued and sent when it is available. Logs failed
// to send will be retried for a few times and dropped if they still can't be
// sent.
// The log writer should be used entirely on one thread after it is created,
// unless otherwise noted.
class TelemetryLogWriter : public ChromotingEventLogWriter {
 public:
  TelemetryLogWriter(std::unique_ptr<OAuthTokenGetter> token_getter);

  ~TelemetryLogWriter() override;

  // Push the log entry to the pending list and send out all the pending logs.
  void Log(const ChromotingEvent& entry) override;

 private:
  void SendPendingEntries();
  void DoSend(apis::v1::CreateEventRequest request);
  void OnSendLogResult(const grpc::Status& status,
                       const apis::v1::CreateEventResponse& response);

  THREAD_CHECKER(thread_checker_);

  std::unique_ptr<OAuthTokenGetter> token_getter_;
  std::unique_ptr<apis::v1::RemotingTelemetryService::Stub> stub_;
  GrpcAuthenticatedExecutor executor_;
  net::BackoffEntry backoff_;
  base::OneShotTimer backoff_timer_;

  // Entries to be sent.
  base::circular_deque<ChromotingEvent> pending_entries_;

  // Entries being sent.
  // These will be pushed back to pending_entries if error occurs.
  base::circular_deque<ChromotingEvent> sending_entries_;

  DISALLOW_COPY_AND_ASSIGN(TelemetryLogWriter);
};

}  // namespace remoting
#endif  // REMOTING_BASE_TELEMETRY_LOG_WRITER_H_
