// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/telemetry_log_writer.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "remoting/base/grpc_support/grpc_async_unary_request.h"
#include "remoting/base/grpc_support/grpc_channel.h"
#include "remoting/base/service_urls.h"

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

}

const int kMaxSendAttempts = 5;

TelemetryLogWriter::TelemetryLogWriter(
    std::unique_ptr<OAuthTokenGetter> token_getter)
    : token_getter_(std::move(token_getter)),
      stub_(apis::v1::RemotingTelemetryService::NewStub(
          CreateSslChannelForEndpoint(
              ServiceUrls::GetInstance()->remoting_server_endpoint()))),
      executor_(token_getter_.get()),
      backoff_(&kBackoffPolicy) {
  DETACH_FROM_THREAD(thread_checker_);
  DCHECK(token_getter_);
}

TelemetryLogWriter::~TelemetryLogWriter() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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

void TelemetryLogWriter::DoSend(apis::v1::CreateEventRequest request) {
  executor_.ExecuteRpc(CreateGrpcAsyncUnaryRequest(
      base::BindOnce(
          &apis::v1::RemotingTelemetryService::Stub::AsyncCreateEvent,
          base::Unretained(stub_.get())),
      request,
      base::BindOnce(&TelemetryLogWriter::OnSendLogResult,
                     base::Unretained(this))));
}

void TelemetryLogWriter::OnSendLogResult(
    const grpc::Status& status,
    const apis::v1::CreateEventResponse& response) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!status.ok()) {
    backoff_.InformOfRequest(false);
    LOG(WARNING) << "Error occur when sending logs to the telemetry server, "
                 << "status: " << status.error_message();

    // Reverse iterating + push_front in order to restore the order of logs.
    for (auto i = sending_entries_.rbegin(); i < sending_entries_.rend(); i++) {
      if (i->send_attempts() >= kMaxSendAttempts) {
        break;
      }
      pending_entries_.push_front(std::move(*i));
    }
  } else {
    backoff_.InformOfRequest(true);
    VLOG(1) << "Successfully sent " << sending_entries_.size()
            << " log(s) to telemetry server.";
  }
  sending_entries_.clear();
  SendPendingEntries();
}

}  // namespace remoting
