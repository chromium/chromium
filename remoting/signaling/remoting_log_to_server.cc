// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/remoting_log_to_server.h"

#include <sstream>

#include "base/bind.h"
#include "remoting/base/grpc_support/grpc_async_unary_request.h"
#include "remoting/base/grpc_support/grpc_authenticated_executor.h"
#include "remoting/base/grpc_support/grpc_channel.h"
#include "remoting/base/service_urls.h"
#include "remoting/proto/remoting/v1/telemetry_service.grpc.pb.h"
#include "third_party/grpc/src/include/grpcpp/support/status.h"

namespace remoting {

namespace {

const net::BackoffEntry::Policy kBackoffPolicy = {
    // Number of initial errors (in sequence) to ignore before applying
    // exponential back-off rules.
    0,

    // Initial delay for exponential back-off in ms.
    1000,

    // Factor by which the waiting time will be multiplied.
    2,

    // Fuzzing percentage. ex: 10% will spread requests randomly
    // between 90%-100% of the calculated time.
    0.5,

    // Maximum amount of time we are willing to delay our request in ms.
    60000,

    // Time to keep an entry from being discarded even when it
    // has no significant state, -1 to never discard.
    -1,

    // Starts with initial delay.
    false,
};

using CreateLogEntryResponseCallback =
    base::OnceCallback<void(const grpc::Status&,
                            const apis::v1::CreateLogEntryResponse&)>;

class TelemetryClient {
 public:
  explicit TelemetryClient(OAuthTokenGetter* token_getter);
  ~TelemetryClient();

  void CreateLogEntry(const apis::v1::CreateLogEntryRequest& request,
                      CreateLogEntryResponseCallback callback);

 private:
  using TelemetryService = apis::v1::RemotingTelemetryService;
  GrpcAuthenticatedExecutor executor_;
  std::unique_ptr<TelemetryService::Stub> stub_;
  DISALLOW_COPY_AND_ASSIGN(TelemetryClient);
};

TelemetryClient::TelemetryClient(OAuthTokenGetter* token_getter)
    : executor_(token_getter) {
  stub_ = TelemetryService::NewStub(CreateSslChannelForEndpoint(
      ServiceUrls::GetInstance()->remoting_server_endpoint()));
}

TelemetryClient::~TelemetryClient() = default;

void TelemetryClient::CreateLogEntry(
    const apis::v1::CreateLogEntryRequest& request,
    CreateLogEntryResponseCallback callback) {
  executor_.ExecuteRpc(CreateGrpcAsyncUnaryRequest(
      base::BindOnce(&TelemetryService::Stub::AsyncCreateLogEntry,
                     base::Unretained(stub_.get())),
      request, std::move(callback)));
}

}  // namespace

RemotingLogToServer::RemotingLogToServer(
    ServerLogEntry::Mode mode,
    std::unique_ptr<OAuthTokenGetter> token_getter)
    : mode_(mode),
      token_getter_(std::move(token_getter)),
      backoff_(&kBackoffPolicy),
      create_log_entry_(base::BindRepeating(
          &TelemetryClient::CreateLogEntry,
          std::make_unique<TelemetryClient>(token_getter_.get()))) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

RemotingLogToServer::~RemotingLogToServer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void RemotingLogToServer::Log(const ServerLogEntry& entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  apis::v1::CreateLogEntryRequest request;
  *request.mutable_payload()->mutable_entry() = entry.ToGenericLogEntry();
  SendLogRequestWithBackoff(request, kMaxSendLogAttempts);
}

void RemotingLogToServer::SendLogRequest(
    const apis::v1::CreateLogEntryRequest& request,
    int attempts_left) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (VLOG_IS_ON(1)) {
    std::ostringstream log_stream;
    log_stream << "Sending log entry with " << attempts_left
               << " attempts left: \n";
    for (const auto& field : request.payload().entry().field()) {
      log_stream << field.key() << ": " << field.value() << "\n";
    }
    log_stream << "=========================================================";
    VLOG(1) << log_stream.str();
  }
  create_log_entry_.Run(
      request,
      base::BindOnce(&RemotingLogToServer::OnSendLogRequestResult,
                     base::Unretained(this), request, attempts_left - 1));
}

void RemotingLogToServer::SendLogRequestWithBackoff(
    const apis::v1::CreateLogEntryRequest& request,
    int attempts_left) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto time_until_release = backoff_.GetTimeUntilRelease();
  VLOG(1) << "Scheduling request in " << time_until_release
          << ", attempts left: " << attempts_left;
  backoff_timer_.Start(
      FROM_HERE, time_until_release,
      base::BindOnce(&RemotingLogToServer::SendLogRequest,
                     base::Unretained(this), request, attempts_left));
}

void RemotingLogToServer::OnSendLogRequestResult(
    const apis::v1::CreateLogEntryRequest& request,
    int attempts_left,
    const grpc::Status& status,
    const apis::v1::CreateLogEntryResponse& response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (status.ok()) {
    VLOG(1) << "One log has been successfully sent.";
    backoff_.InformOfRequest(true);
    return;
  }
  LOG(WARNING) << "Failed to send one log."
               << " Error: " << status.error_code()
               << " Message: " << status.error_message();
  backoff_.InformOfRequest(false);
  if (attempts_left <= 0) {
    LOG(WARNING) << "Exceeded maximum retry attempts. Dropping it...";
    return;
  }
  SendLogRequestWithBackoff(request, attempts_left);
}

ServerLogEntry::Mode RemotingLogToServer::mode() const {
  return mode_;
}

}  // namespace remoting
