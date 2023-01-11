// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/telemetry_log_writer.h"

#include <utility>

#include "base/containers/adapters.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "remoting/base/protobuf_http_client.h"
#include "remoting/base/protobuf_http_request.h"
#include "remoting/base/protobuf_http_request_config.h"
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
    net::DefineNetworkTrafficAnnotation("remoting_telemetry_log_writer",
                                        R"(
        semantics {
          sender: "Chrome Remote Desktop"
          description:
            "Sends telemetry logs for Chrome Remote Desktop."
          trigger:
            "These requests are sent periodically by Chrome Remote Desktop "
            "(CRD) Android and iOS clients when a session is connected, i.e. "
            "CRD app is running and is connected to a host."
          data:
            "Anonymous usage statistics, which includes CRD host version, "
            "connection time, authentication type, connection error, and "
            "round trip latency."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This request cannot be stopped in settings, but will not be sent "
            "if the user does not use Chrome Remote Desktop."
          policy_exception_justification:
            "Not implemented."
        })");

constexpr char kCreateEventPath[] = "/v1/telemetry:createevent";
}  // namespace

const int kMaxSendAttempts = 5;

TelemetryLogWriter::TelemetryLogWriter(
    std::unique_ptr<OAuthTokenGetter> token_getter)
    : token_getter_(std::move(token_getter)), backoff_(&kBackoffPolicy) {
  DETACH_FROM_THREAD(thread_checker_);
  DCHECK(token_getter_);
}

TelemetryLogWriter::~TelemetryLogWriter() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void TelemetryLogWriter::Init(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!http_client_);
  http_client_ = std::make_unique<ProtobufHttpClient>(
      ServiceUrls::GetInstance()->remoting_server_endpoint(),
      token_getter_.get(), url_loader_factory);
}

void TelemetryLogWriter::Log(const ChromotingEvent& entry) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  pending_entries_.push_back(entry);
  SendPendingEntries();
}

void TelemetryLogWriter::SendPendingEntries() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!sending_entries_.empty() || pending_entries_.empty()) {
    return;
  }

  apis::v1::CreateEventRequest request;
  while (!pending_entries_.empty()) {
    ChromotingEvent& entry = pending_entries_.front();
    DCHECK(entry.IsDataValid());
    *request.mutable_payload()->mutable_events()->Add() = entry.CreateProto();
    entry.IncrementSendAttempts();
    sending_entries_.push_back(std::move(entry));
    pending_entries_.pop_front();
  }

  base::TimeDelta time_until_release = backoff_.GetTimeUntilRelease();
  backoff_timer_.Start(
      FROM_HERE, time_until_release,
      base::BindOnce(&TelemetryLogWriter::DoSend, base::Unretained(this),
                     std::move(request)));
}

void TelemetryLogWriter::DoSend(const apis::v1::CreateEventRequest& request) {
  DCHECK(http_client_);
  auto request_config =
      std::make_unique<ProtobufHttpRequestConfig>(kTrafficAnnotation);
  request_config->path = kCreateEventPath;
  request_config->request_message =
      std::make_unique<apis::v1::CreateEventRequest>(request);
  auto http_request =
      std::make_unique<ProtobufHttpRequest>(std::move(request_config));
  http_request->SetResponseCallback(base::BindOnce(
      &TelemetryLogWriter::OnSendLogResult, base::Unretained(this)));
  http_client_->ExecuteRequest(std::move(http_request));
}

void TelemetryLogWriter::OnSendLogResult(
    const ProtobufHttpStatus& status,
    std::unique_ptr<apis::v1::CreateEventResponse> response) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!status.ok()) {
    backoff_.InformOfRequest(false);
    LOG(WARNING) << "Error occur when sending logs to the telemetry server, "
                 << "status: " << status.error_message();

    // Reverse iterating + push_front in order to restore the order of logs.
    for (auto& entry : base::Reversed(sending_entries_)) {
      if (entry.send_attempts() >= kMaxSendAttempts) {
        break;
      }
      pending_entries_.push_front(std::move(entry));
    }
  } else {
    backoff_.InformOfRequest(true);
    VLOG(1) << "Successfully sent " << sending_entries_.size()
            << " log(s) to telemetry server.";
  }
  sending_entries_.clear();
  SendPendingEntries();
}

bool TelemetryLogWriter::IsIdleForTesting() {
  return sending_entries_.empty() && pending_entries_.empty();
}

}  // namespace remoting
