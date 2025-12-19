// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_SESSION_ERROR_H_
#define NET_DEVICE_BOUND_SESSIONS_SESSION_ERROR_H_

#include "net/base/schemeful_site.h"
#include "net/device_bound_sessions/deletion_reason.h"

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
    // Deprecated: kNetError = 3,
    // Deprecated: kHttpError = 4,
    kServerRequestedTermination = 5,
    // Deprecated: kInvalidConfigJson = 6,
    kInvalidSessionId = 7,
    // Deprecated: kInvalidCredentials = 8,
    kInvalidChallenge = 9,
    kTooManyChallenges = 10,
    kInvalidFetcherUrl = 11,
    kInvalidRefreshUrl = 12,
    kTransientHttpError = 13,
    // Deprecated: kPersistentHttpError = 14,
    kScopeOriginSameSiteMismatch = 15,
    kRefreshUrlSameSiteMismatch = 16,
    // Deprecated: kInvalidScopeOrigin = 17,
    kMismatchedSessionId = 18,
    // Deprecated: kInvalidRefreshInitiators = 19,
    // Deprecated: kInvalidScopeRule = 20,
    kMissingScope = 21,
    kNoCredentials = 22,
    // Deprecated: kInvalidScopeIncludeSite = 23,
    kSubdomainRegistrationWellKnownUnavailable = 24,
    kSubdomainRegistrationUnauthorized = 25,
    kSubdomainRegistrationWellKnownMalformed = 26,
    // Deprecated: kFederatedNotAuthorized = 27,
    kSessionProviderWellKnownUnavailable = 28,
    // Deprecated: kSessionProviderWellKnownMalformed = 29,
    kRelyingPartyWellKnownUnavailable = 30,
    // Deprecated: kRelyingPartyWellKnownMalformed = 31,
    kFederatedKeyThumbprintMismatch = 32,
    kInvalidFederatedSessionUrl = 33,
    // Deprecated: kInvalidFederatedSession = 34,
    kInvalidFederatedKey = 35,
    kTooManyRelyingOriginLabels = 36,
    kBoundCookieSetForbidden = 37,
    kNetError = 38,
    kProxyError = 39,
    // Deprecated: kInvalidConfigJson = 40,
    kEmptySessionConfig = 41,
    kInvalidCredentialsConfig = 42,
    kInvalidCredentialsType = 43,
    kInvalidCredentialsEmptyName = 44,
    kInvalidCredentialsCookie = 45,
    kPersistentHttpError = 46,
    kRegistrationAttemptedChallenge = 47,
    kInvalidScopeOrigin = 48,
    kScopeOriginContainsPath = 49,
    kRefreshInitiatorNotString = 50,
    kRefreshInitiatorInvalidHostPattern = 51,
    kInvalidScopeSpecification = 52,
    kMissingScopeSpecificationType = 53,
    kEmptyScopeSpecificationDomain = 54,
    kEmptyScopeSpecificationPath = 55,
    kInvalidScopeSpecificationType = 56,
    kInvalidScopeIncludeSite = 57,
    kMissingScopeIncludeSite = 58,
    kFederatedNotAuthorizedByProvider = 59,
    kFederatedNotAuthorizedByRelyingParty = 60,
    kSessionProviderWellKnownMalformed = 61,
    kSessionProviderWellKnownHasProviderOrigin = 62,
    kRelyingPartyWellKnownMalformed = 63,
    kRelyingPartyWellKnownHasRelyingOrigins = 64,
    kInvalidFederatedSessionProviderSessionMissing = 65,
    kInvalidFederatedSessionWrongProviderOrigin = 66,
    kInvalidCredentialsCookieCreationTime = 67,
    kInvalidCredentialsCookieName = 68,
    kInvalidCredentialsCookieParsing = 69,
    kInvalidCredentialsCookieUnpermittedAttribute = 70,
    kInvalidCredentialsCookieInvalidDomain = 71,
    kInvalidCredentialsCookiePrefix = 72,
    kInvalidScopeRulePath = 73,
    kInvalidScopeRuleHostPattern = 74,
    kScopeRuleOriginScopedHostPatternMismatch = 75,
    kScopeRuleSiteScopedHostPatternMismatch = 76,
    kSigningQuotaExceeded = 77,
    kInvalidConfigJson = 78,
    kInvalidFederatedSessionProviderFailedToRestoreKey = 79,
    kFailedToUnwrapKey = 80,
    kSessionDeletedDuringRefresh = 81,
    kMaxValue = kSessionDeletedDuringRefresh,
  };
  // LINT.ThenChange(//tools/metrics/histograms/enums.xml:DeviceBoundSessionError,//services/network/public/mojom/device_bound_sessions.mojom:DeviceBoundSessionError)

  using enum ErrorType;

  explicit SessionError(ErrorType type);
  ~SessionError();

  SessionError(const SessionError&);
  SessionError& operator=(const SessionError&);

  SessionError(SessionError&&) noexcept;
  SessionError& operator=(SessionError&&) noexcept;

  // If the error is non-fatal, returns `std::nullopt`. Otherwise
  // returns the reason for deleting the session.
  std::optional<DeletionReason> GetDeletionReason() const;

  // Whether the error is due to server-side behavior.
  bool IsServerError() const;

  ErrorType type;

  bool operator==(const SessionError& other) const {
    return type == other.type;
  }
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_SESSION_ERROR_H_
