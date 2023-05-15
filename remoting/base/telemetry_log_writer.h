// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_TELEMETRY_LOG_WRITER_H_
#define REMOTING_BASE_TELEMETRY_LOG_WRITER_H_

#include <string>

#include "base/containers/circular_deque.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "net/base/backoff_entry.h"
#include "remoting/base/chromoting_event.h"
#include "remoting/base/chromoting_event_log_writer.h"
#include "remoting/base/oauth_token_getter.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace remoting {

namespace apis {
namespace v1 {

class CreateEventRequest;
class CreateEventResponse;

}  // namespace v1
}  // namespace apis

class ProtobufHttpClient;
class ProtobufHttpStatus;

// TelemetryLogWriter sends log entries (ChromotingEvent) to the telemetry
// server.
// Logs to be sent will be queued and sent when it is available. Logs failed
// to send will be retried for a few times and dropped if they still can't be
// sent.
// The log writer should be used entirely on one thread after it is created,
// unless otherwise noted.
class TelemetryLogWriter : public ChromotingEventLogWriter {
 public:
  explicit TelemetryLogWriter(std::unique_ptr<OAuthTokenGetter> token_getter);

  TelemetryLogWriter(const TelemetryLogWriter&) = delete;
  TelemetryLogWriter& operator=(const TelemetryLogWriter&) = delete;

  ~TelemetryLogWriter() override;

  void Init(scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // Push the log entry to the pending list and send out all the pending logs.
  void Log(const ChromotingEvent& entry) override;

 private:
  void SendPendingEntries();
  void DoSend(const apis::v1::CreateEventRequest& request);
  void OnSendLogResult(const ProtobufHttpStatus& status,
                       std::unique_ptr<apis::v1::CreateEventResponse> response);

  // Returns true if there are no events sending or pending.
  bool IsIdleForTesting();

  THREAD_CHECKER(thread_checker_);

  std::unique_ptr<OAuthTokenGetter> token_getter_;
  std::unique_ptr<ProtobufHttpClient> http_client_;
  net::BackoffEntry backoff_;
  base::OneShotTimer backoff_timer_;

  // Entries to be sent.
  base::circular_deque<ChromotingEvent> pending_entries_;

  // Entries being sent.
  // These will be pushed back to pending_entries if error occurs.
  base::circular_deque<ChromotingEvent> sending_entries_;

  friend class TelemetryLogWriterTest;
};

}  // namespace remoting
#endif  // REMOTING_BASE_TELEMETRY_LOG_WRITER_H_
