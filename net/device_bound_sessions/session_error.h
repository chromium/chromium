// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_SESSION_ERROR_H_
#define NET_DEVICE_BOUND_SESSIONS_SESSION_ERROR_H_

#include "net/base/schemeful_site.h"

namespace net::device_bound_sessions {

struct NET_EXPORT SessionError {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(DeviceBoundSessionError)
  enum class ErrorType {
    kSuccess = 0,  // Only used for metrics, a session error will never have
                   // this error type.
    kKeyError = 1,
    kSigningError = 2,
    kNetError = 3,
    // Deprecated: kHttpError = 4,
    kServerRequestedTermination = 5,
    kInvalidConfigJson = 6,
    kInvalidSessionId = 7,
    kInvalidCredentials = 8,
    kInvalidChallenge = 9,
    kTooManyChallenges = 10,
    kInvalidFetcherUrl = 11,
    kInvalidRefreshUrl = 12,
    kTransientHttpError = 13,
    kPersistentHttpError = 14,
    kScopeOriginSameSiteMismatch = 15,
    kRefreshUrlSameSiteMismatch = 16,
    kInvalidScopeOrigin = 17,
    kMismatchedSessionId = 18,
    kInvalidRefreshInitiators = 19,
    kInvalidScopeRule = 20,
    kMissingScope = 21,
    kNoCredentials = 22,
    kInvalidScopeIncludeSite = 23,
    kMaxValue = kInvalidScopeIncludeSite
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/net/enums.xml:DeviceBoundSessionError)

  explicit SessionError(ErrorType type);
  ~SessionError();

  SessionError(const SessionError&) = delete;
  SessionError& operator=(const SessionError&) = delete;

  SessionError(SessionError&&) noexcept;
  SessionError& operator=(SessionError&&) noexcept;

  bool IsFatal() const;

  // Whether the error is due to server-side behavior.
  bool IsServerError() const;

  ErrorType type;
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_SESSION_ERROR_H_
