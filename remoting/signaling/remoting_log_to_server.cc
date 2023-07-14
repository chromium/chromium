// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/remoting_log_to_server.h"

#include <sstream>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "remoting/base/protobuf_http_client.h"
#include "remoting/base/protobuf_http_request.h"
#include "remoting/base/protobuf_http_request_config.h"
#include "remoting/base/protobuf_http_status.h"
#include "remoting/base/service_urls.h"
#include "remoting/proto/remoting/v1/telemetry_messages.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

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

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("remoting_log_to_server",
                                        R"(
        semantics {
          sender: "Chrome Remote Desktop"
          description:
            "Sends telemetry logs for Chrome Remote Desktop."
          trigger:
            "These requests are sent periodically when a session is connected, "
            "i.e. CRD host is running and is connected to a client."
          user_data {
            type: OTHER
          }
          data:
            "Anonymous usage statistics, which includes CRD host version, OS "
            "name, OS version, and CPU architecture (e.g. x86_64)."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts { email: "garykac@chromium.org" }
            contacts { email: "jamiewalch@chromium.org" }
            contacts { email: "joedow@chromium.org" }
            contacts { email: "lambroslambrou@chromium.org" }
            contacts { email: "rkjnsn@chromium.org" }
            contacts { email: "yuweih@chromium.org" }
          }
          last_reviewed: "2023-07-07"
        }
        policy {
          cookies_allowed: NO
          setting:
            "This request cannot be stopped in settings, but will not be sent "
            "if the user does not use Chrome Remote Desktop."
          chrome_policy {
            RemoteAccessHostAllowRemoteSupportConnections {
              RemoteAccessHostAllowRemoteSupportConnections: false
            }
            RemoteAccessHostAllowEnterpriseRemoteSupportConnections {
              RemoteAccessHostAllowEnterpriseRemoteSupportConnections: false
            }
          }
        })");

constexpr char kCreateLogEntryPath[] = "/v1/telemetry:createlogentry";

using CreateLogEntryResponseCallback =
    base::OnceCallback<void(const ProtobufHttpStatus&,
                            std::unique_ptr<apis::v1::CreateLogEntryResponse>)>;

class TelemetryClient {
 public:
  TelemetryClient(
      OAuthTokenGetter* token_getter,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  TelemetryClient(const TelemetryClient&) = delete;
  TelemetryClient& operator=(const TelemetryClient&) = delete;

  ~TelemetryClient();

  void CreateLogEntry(const apis::v1::CreateLogEntryRequest& request,
                      CreateLogEntryResponseCallback callback);

 private:
  ProtobufHttpClient http_client_;
};

TelemetryClient::TelemetryClient(
    OAuthTokenGetter* token_getter,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : http_client_(ServiceUrls::GetInstance()->remoting_server_endpoint(),
                   token_getter,
                   url_loader_factory) {}

TelemetryClient::~TelemetryClient() = default;

void TelemetryClient::CreateLogEntry(
    const apis::v1::CreateLogEntryRequest& request,
    CreateLogEntryResponseCallback callback) {
  auto request_config =
      std::make_unique<ProtobufHttpRequestConfig>(kTrafficAnnotation);
  request_config->path = kCreateLogEntryPath;
  request_config->request_message =
      std::make_unique<apis::v1::CreateLogEntryRequest>(request);
  auto http_request =
      std::make_unique<ProtobufHttpRequest>(std::move(request_config));
  http_request->SetResponseCallback(std::move(callback));
  http_client_.ExecuteRequest(std::move(http_request));
}

}  // namespace

RemotingLogToServer::RemotingLogToServer(
    ServerLogEntry::Mode mode,
    std::unique_ptr<OAuthTokenGetter> token_getter,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : mode_(mode),
      token_getter_(std::move(token_getter)),
      backoff_(&kBackoffPolicy),
      create_log_entry_(base::BindRepeating(
          &TelemetryClient::CreateLogEntry,
          std::make_unique<TelemetryClient>(token_getter_.get(),
                                            url_loader_factory))) {}

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
    const ProtobufHttpStatus& status,
    std::unique_ptr<apis::v1::CreateLogEntryResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (status.ok()) {
    VLOG(1) << "One log has been successfully sent.";
    backoff_.InformOfRequest(true);
    return;
  }
  LOG(WARNING) << "Failed to send one log."
               << " Error: " << static_cast<int>(status.error_code())
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
