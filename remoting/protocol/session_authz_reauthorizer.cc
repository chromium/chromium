// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/session_authz_reauthorizer.h"

#include <algorithm>
#include <memory>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "net/base/backoff_entry.h"
#include "remoting/base/logging.h"
#include "remoting/proto/session_authz_service.h"
#include "remoting/protocol/errors.h"
#include "remoting/protocol/session.h"

namespace remoting::protocol {

namespace {

constexpr base::TimeDelta kMinReauthTokenLifetime = base::Minutes(5);
constexpr base::TimeDelta kMinReauthRetryDuration = kMinReauthTokenLifetime / 2;
constexpr base::TimeDelta kMaxReauthRetryDuration = base::Minutes(10);

base::TimeDelta ClampSessionReauthTokenLifetime(
    base::TimeDelta session_reauth_token_lifetime) {
  if (session_reauth_token_lifetime < kMinReauthTokenLifetime) {
    LOG(WARNING) << session_reauth_token_lifetime << " is below the minimum "
                 << "session reauth token threshold, reauthorization is likely "
                 << "to fail";
    return kMinReauthTokenLifetime;
  }
  return session_reauth_token_lifetime;
}

base::TimeDelta GetReauthInterval(base::TimeTicks reauth_token_expire_time) {
  auto token_lifetime = reauth_token_expire_time - base::TimeTicks::Now();
  return token_lifetime - std::clamp(token_lifetime / 2,
                                     kMinReauthRetryDuration,
                                     kMaxReauthRetryDuration);
}

}  // namespace

SessionAuthzReauthorizer::SessionAuthzReauthorizer(
    SessionAuthzServiceClient* service_client,
    std::string_view session_id,
    std::string_view session_reauth_token,
    base::TimeDelta session_reauth_token_lifetime,
    OnReauthorizationFailedCallback on_reauthorization_failed)
    : service_client_(service_client),
      session_id_(session_id),
      session_reauth_token_(session_reauth_token),
      on_reauthorization_failed_(std::move(on_reauthorization_failed)) {
  token_expire_time_ =
      base::TimeTicks::Now() +
      ClampSessionReauthTokenLifetime(session_reauth_token_lifetime);
}

SessionAuthzReauthorizer::~SessionAuthzReauthorizer() = default;

void SessionAuthzReauthorizer::Start() {
  HOST_LOG << "SessionAuthz reauthorizer has started.";
  ScheduleNextReauth();
}

void SessionAuthzReauthorizer::ScheduleNextReauth() {
  base::TimeDelta next_reauth_interval = GetReauthInterval(token_expire_time_);
  reauthorize_timer_.Start(FROM_HERE, next_reauth_interval, this,
                           &SessionAuthzReauthorizer::Reauthorize);
  HOST_LOG << "Next reauthorization scheduled in " << next_reauth_interval;
}

void SessionAuthzReauthorizer::Reauthorize() {
  HOST_LOG << "Reauthorizing session...";
  service_client_->ReauthorizeHost(
      session_reauth_token_, session_id_, token_expire_time_,
      base::BindOnce(&SessionAuthzReauthorizer::OnReauthorizeResult,
                     base::Unretained(this)));
}

void SessionAuthzReauthorizer::OnReauthorizeResult(
    const HttpStatus& status,
    std::unique_ptr<internal::ReauthorizeHostResponseStruct> response) {
  if (!status.ok()) {
    Authenticator::RejectionDetails rejection_details(base::StringPrintf(
        "SessionAuthz reauthorization failed with error. Code: %d Message: %s",
        static_cast<int>(status.error_code()), status.error_message()));
    NotifyReauthorizationFailed(status.error_code(), rejection_details);
    return;
  }

  DCHECK(response->session_reauth_token_lifetime.is_positive());
  session_reauth_token_ = response->session_reauth_token;
  token_expire_time_ =
      base::TimeTicks::Now() +
      ClampSessionReauthTokenLifetime(response->session_reauth_token_lifetime);
  VLOG(1) << "SessionAuthz reauthorization succeeded.";
  ScheduleNextReauth();
}

void SessionAuthzReauthorizer::NotifyReauthorizationFailed(
    HttpStatus::Code error_code,
    const Authenticator::RejectionDetails& details) {
  // Make sure the callback causes the reauthorizer to be destroyed (which
  // implies the session is closed). Otherwise, crash the process.
  reauthorize_timer_.Start(
      FROM_HERE, base::Seconds(30), base::BindOnce([]() {
        LOG(FATAL) << "SessionAuthzReauthorizer is still alive after the "
                   << "reauthorization failure has been notified.";
      }));
  std::move(on_reauthorization_failed_).Run(error_code, details);
}

}  // namespace remoting::protocol
