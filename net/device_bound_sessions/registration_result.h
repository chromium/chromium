// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_REGISTRATION_RESULT_H_
#define NET_DEVICE_BOUND_SESSIONS_REGISTRATION_RESULT_H_

#include <memory>
#include <variant>

#include "base/types/expected.h"
#include "net/base/net_export.h"
#include "net/cookies/canonical_cookie.h"
#include "net/device_bound_sessions/session_error.h"

namespace net::device_bound_sessions {

class Session;

// This class represenets the outcome of a registration or refresh
// request. It's a convenience wrapper around a std::variant.
class NET_EXPORT RegistrationResult {
 public:
  // Trivial class used to indicate that no changes should be made to the
  // session.
  class NoSessionConfigChange {};

  RegistrationResult(std::unique_ptr<Session> session,
                     CookieAndLineAccessResultList maybe_stored_cookies);
  RegistrationResult(NoSessionConfigChange no_change,
                     CookieAndLineAccessResultList maybe_stored_cookies);
  explicit RegistrationResult(SessionError session_error);
  explicit RegistrationResult(
      base::expected<std::unique_ptr<Session>, SessionError> session_or_error);

  ~RegistrationResult();

  RegistrationResult(const RegistrationResult&) = delete;
  RegistrationResult& operator=(const RegistrationResult&) = delete;

  RegistrationResult(RegistrationResult&&);
  RegistrationResult& operator=(RegistrationResult&&);

  template <class Visitor>
  decltype(auto) Visit(Visitor&& v) const& {
    return std::visit(std::forward<Visitor>(v), storage_);
  }

  template <class Visitor>
  decltype(auto) Visit(Visitor&& v) && {
    return std::visit(std::forward<Visitor>(v), std::move(storage_));
  }

  // Test-only accessors
  const Session& SessionForTesting() const;
  NoSessionConfigChange NoSessionConfigChangeForTesting() const;
  SessionError SessionErrorForTesting() const;

  CookieAndLineAccessResultList TakeStoredCookies();

 private:
  std::variant<std::unique_ptr<Session>, NoSessionConfigChange, SessionError>
      storage_;
  CookieAndLineAccessResultList maybe_stored_cookies_;
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_REGISTRATION_RESULT_H_
