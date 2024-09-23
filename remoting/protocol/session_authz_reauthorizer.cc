// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/session_authz_reauthorizer.h"

#include <memory>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "net/base/backoff_entry.h"
#include "remoting/base/logging.h"
#include "remoting/proto/session_authz_service.h"
#include "remoting/protocol/errors.h"
#include "remoting/protocol/session.h"

namespace remoting::protocol {
namespace {

constexpr net::BackoffEntry::Policy kBackoffPolicy = {
    .num_errors_to_ignore = 0,
    .initial_delay_ms = base::Seconds(5).InMilliseconds(),
    .multiply_factor = 2,
    .jitter_factor = 0.5,
    .maximum_backoff_ms = base::Minutes(1).InMilliseconds(),
    .entry_lifetime_ms = -1,  // never discard.
    // InformOfRequest() is called before the retry task is scheduled, so the
    // initial delay is technically used.
    .always_use_initial_delay = false,
};

bool IsRetriableError(ProtobufHttpStatus::Code code) {
  DCHECK_NE(code, ProtobufHttpStatus::Code::OK);
  switch (code) {
    case ProtobufHttpStatus::Code::ABORTED:
    case ProtobufHttpStatus::Code::UNAVAILABLE:
    case ProtobufHttpStatus::Code::NETWORK_ERROR:
    case ProtobufHttpStatus::Code::UNKNOWN:
      return true;
    default:
      return false;
  }
}

}  // namespace

SessionAuthzReauthorizer::SessionAuthzReauthorizer(
    SessionAuthzServiceClient* service_client,
    std::string_view session_id,
    std::string_view session_reauth_token,
    base::TimeDelta session_reauth_token_lifetime,
    base::OnceClosure on_reauthorization_failed)
    : service_client_(service_client),
      session_id_(session_id),
      session_reauth_token_(session_reauth_token),
      token_expire_time_(base::TimeTicks::Now() +
                         session_reauth_token_lifetime),
      on_reauthorization_failed_(std::move(on_reauthorization_failed)) {}

SessionAuthzReauthorizer::~SessionAuthzReauthorizer() = default;

void SessionAuthzReauthorizer::Start() {
  HOST_LOG << "SessionAuthz reauthorizer has started.";
  ScheduleNextReauth();
}

const net::BackoffEntry* SessionAuthzReauthorizer::GetBackoffEntryForTest()
    const {
  return backoff_entry_.get();
}

void SessionAuthzReauthorizer::ScheduleNextReauth() {
  base::TimeDelta next_reauth_interval =
      backoff_entry_ ? backoff_entry_->GetTimeUntilRelease()
                     : (token_expire_time_ - base::TimeTicks::Now()) / 2;
  reauthorize_timer_.Start(FROM_HERE, next_reauth_interval, this,
                           &SessionAuthzReauthorizer::Reauthorize);
  HOST_LOG << "Next reauthorization scheduled in " << next_reauth_interval;
}

void SessionAuthzReauthorizer::Reauthorize() {
  HOST_LOG << "Reauthorizing session...";
  internal::ReauthorizeHostRequestStruct request;
  request.session_id = session_id_;
  request.session_reauth_token = session_reauth_token_;
  service_client_->ReauthorizeHost(
      request, base::BindOnce(&SessionAuthzReauthorizer::OnReauthorizeResult,
                              base::Unretained(this)));
}

void SessionAuthzReauthorizer::OnReauthorizeResult(
    const ProtobufHttpStatus& status,
    std::unique_ptr<internal::ReauthorizeHostResponseStruct> response) {
  if (!status.ok()) {
    LOG(ERROR) << "SessionAuthz reauthorization failed with error. Code: "
               << static_cast<int>(status.error_code())
               << " Message: " << status.error_message();
    if (!IsRetriableError(status.error_code())) {
      LOG(ERROR) << "Error is non-retriable. Closing the session.";
      NotifyReauthorizationFailed();
      return;
    }
    if (!backoff_entry_) {
      backoff_entry_ = std::make_unique<net::BackoffEntry>(&kBackoffPolicy);
    }
    backoff_entry_->InformOfRequest(false);
    // Add some leeway to account for network latencies.
    if (backoff_entry_->GetReleaseTime() >
        (token_expire_time_ - base::Seconds(5))) {
      LOG(ERROR) << "No more retries remaining. Closing the session.";
      NotifyReauthorizationFailed();
      return;
    }
    ScheduleNextReauth();
    return;
  }

  DCHECK(response->session_reauth_token_lifetime.is_positive());
  backoff_entry_.reset();
  session_reauth_token_ = response->session_reauth_token;
  token_expire_time_ =
      base::TimeTicks::Now() + response->session_reauth_token_lifetime;
  VLOG(1) << "SessionAuthz reauthorization succeeded.";
  ScheduleNextReauth();
}

void SessionAuthzReauthorizer::NotifyReauthorizationFailed() {
  // Make sure the callback causes the reauthorizer to be destroyed (which
  // implies the session is closed). Otherwise, crash the process.
  reauthorize_timer_.Start(
      FROM_HERE, base::Seconds(30), base::BindOnce([]() {
        LOG(FATAL) << "SessionAuthzReauthorizer is still alive after the "
                   << "reauthorization failure has been notified.";
      }));
  std::move(on_reauthorization_failed_).Run();
}

}  // namespace remoting::protocol
