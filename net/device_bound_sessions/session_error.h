// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_SESSION_ERROR_H_
#define NET_DEVICE_BOUND_SESSIONS_SESSION_ERROR_H_

#include "net/base/schemeful_site.h"

namespace net::device_bound_sessions {

struct NET_EXPORT SessionError {
  enum class ErrorType {
    kSuccess = 0,  // Only used for metrics, a session error will never have
                   // this error type.
    kKeyError = 1,
    kSigningError = 2,
    kEndpointUnreachable = 3,  // Includes both net errors and HTTP errors.
    kServerRequestedTermination = 4,
    // TODO(crbug.com/388557900): This bucket is very broad. Divide it
    // into more actionable error types.
    kInvalidSessionConfig = 6,
    kTooManyChallenges = 7,
  };

  SessionError(ErrorType type,
               net::SchemefulSite site,
               std::optional<std::string> session_id);
  ~SessionError();

  SessionError(const SessionError&) = delete;
  SessionError& operator=(const SessionError&) = delete;

  SessionError(SessionError&&) noexcept;
  SessionError& operator=(SessionError&&) noexcept;

  bool IsFatal() const;

  ErrorType type;
  net::SchemefulSite site;
  std::optional<std::string> session_id;
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_SESSION_ERROR_H_
