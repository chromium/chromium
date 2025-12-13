// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/registration_result.h"

#include "net/device_bound_sessions/session.h"

namespace net::device_bound_sessions {

RegistrationResult::RegistrationResult(
    std::unique_ptr<Session> session,
    CookieAndLineAccessResultList maybe_stored_cookies)
    : storage_(std::move(session)),
      maybe_stored_cookies_(std::move(maybe_stored_cookies)) {}
RegistrationResult::RegistrationResult(
    NoSessionConfigChange no_change,
    CookieAndLineAccessResultList maybe_stored_cookies)
    : storage_(std::move(no_change)),
      maybe_stored_cookies_(std::move(maybe_stored_cookies)) {}
RegistrationResult::RegistrationResult(SessionError error)
    : storage_(std::move(error)) {}
RegistrationResult::RegistrationResult(
    base::expected<std::unique_ptr<Session>, SessionError> session_or_error)
    : storage_(
          session_or_error.has_value()
              ? std::variant<std::unique_ptr<Session>,
                             NoSessionConfigChange,
                             SessionError>(std::move(session_or_error).value())
              : std::move(session_or_error).error()) {}

RegistrationResult::~RegistrationResult() = default;

RegistrationResult::RegistrationResult(RegistrationResult&&) = default;
RegistrationResult& RegistrationResult::operator=(RegistrationResult&&) =
    default;

CookieAndLineAccessResultList RegistrationResult::TakeStoredCookies() {
  return std::move(maybe_stored_cookies_);
}

const Session& RegistrationResult::SessionForTesting() const {
  return *std::get<std::unique_ptr<Session>>(storage_);
}
RegistrationResult::NoSessionConfigChange
RegistrationResult::NoSessionConfigChangeForTesting() const {
  return std::get<NoSessionConfigChange>(storage_);
}
SessionError RegistrationResult::SessionErrorForTesting() const {
  return std::get<SessionError>(storage_);
}

}  // namespace net::device_bound_sessions
