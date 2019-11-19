// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_REMOTING_LOG_TO_SERVER_H_
#define REMOTING_SIGNALING_REMOTING_LOG_TO_SERVER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "net/base/backoff_entry.h"
#include "remoting/signaling/log_to_server.h"

namespace grpc {
class Status;
}  // namespace grpc

namespace remoting {

namespace apis {
namespace v1 {
class CreateLogEntryRequest;
class CreateLogEntryResponse;
}  // namespace v1
}  // namespace apis

class OAuthTokenGetter;

// RemotingLogToServer sends log entries to to the remoting telemetry server.
class RemotingLogToServer : public LogToServer {
 public:
  RemotingLogToServer(ServerLogEntry::Mode mode,
                      std::unique_ptr<OAuthTokenGetter> token_getter);
  ~RemotingLogToServer() override;

  // LogToServer interface.
  void Log(const ServerLogEntry& entry) override;
  ServerLogEntry::Mode mode() const override;

 private:
  static constexpr int kMaxSendLogAttempts = 5;

  using CreateLogEntryResponseCallback =
      base::OnceCallback<void(const grpc::Status&,
                              const apis::v1::CreateLogEntryResponse&)>;
  using CreateLogEntryCallback =
      base::RepeatingCallback<void(const apis::v1::CreateLogEntryRequest&,
                                   CreateLogEntryResponseCallback callback)>;

  friend class RemotingLogToServerTest;

  void SendLogRequest(const apis::v1::CreateLogEntryRequest& request,
                      int attempts_left);
  void SendLogRequestWithBackoff(const apis::v1::CreateLogEntryRequest& request,
                                 int attempts_left);
  void OnSendLogRequestResult(const apis::v1::CreateLogEntryRequest& request,
                              int attempts_left,
                              const grpc::Status& status,
                              const apis::v1::CreateLogEntryResponse& response);

  ServerLogEntry::Mode mode_;
  std::unique_ptr<OAuthTokenGetter> token_getter_;
  net::BackoffEntry backoff_;
  base::OneShotTimer backoff_timer_;

  // Callback used to send the log entry to the server. Replaceable for
  // unittest.
  CreateLogEntryCallback create_log_entry_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(RemotingLogToServer);
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_REMOTING_LOG_TO_SERVER_H_
