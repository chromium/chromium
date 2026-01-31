// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_error.h"

#include "base/notreached.h"

namespace net::device_bound_sessions {

SessionError::SessionError(SessionError::ErrorType type) : type(type) {}

SessionError::~SessionError() = default;

SessionError::SessionError(const SessionError&) = default;
SessionError& SessionError::operator=(const SessionError&) = default;
SessionError::SessionError(SessionError&&) noexcept = default;
SessionError& SessionError::operator=(SessionError&&) noexcept = default;

std::optional<DeletionReason> SessionError::GetDeletionReason() const {
  switch (type) {
    case kSuccess:
      return std::nullopt;
    case kServerRequestedTermination:
      return DeletionReason::kServerRequested;
    case kKeyError:
    case kSigningError:
    case kPersistentHttpError:
    case kInvalidChallenge:
    case kTooManyChallenges:
    case kSessionDeletedDuringRefresh:
      return DeletionReason::kRefreshFatalError;
    case kInvalidConfigJson:
    case kInvalidSessionId:
    case kInvalidCredentialsConfig:
    case kInvalidCredentialsType:
    case kInvalidCredentialsEmptyName:
    case kInvalidCredentialsCookie:
    case kInvalidCredentialsCookieCreationTime:
    case kInvalidCredentialsCookieName:
    case kInvalidCredentialsCookieParsing:
    case kInvalidCredentialsCookieUnpermittedAttribute:
    case kInvalidCredentialsCookieInvalidDomain:
    case kInvalidCredentialsCookiePrefix:
    case kInvalidFetcherUrl:
    case kInvalidRefreshUrl:
    case kScopeOriginSameSiteMismatch:
    case kRefreshUrlSameSiteMismatch:
    case kInvalidScopeOrigin:
    case kScopeOriginContainsPath:
    case kMismatchedSessionId:
    case kRefreshInitiatorNotString:
    case kRefreshInitiatorInvalidHostPattern:
    case kInvalidScopeRulePath:
    case kInvalidScopeRuleHostPattern:
    case kScopeRuleOriginScopedHostPatternMismatch:
    case kScopeRuleSiteScopedHostPatternMismatch:
    case kInvalidScopeSpecification:
    case kMissingScopeSpecificationType:
    case kEmptyScopeSpecificationDomain:
    case kEmptyScopeSpecificationPath:
    case kInvalidScopeSpecificationType:
    case kMissingScope:
    case kNoCredentials:
    case kInvalidScopeIncludeSite:
    case kMissingScopeIncludeSite:
      return DeletionReason::kInvalidSessionParams;
    case kNetError:
    case kProxyError:
    case kTransientHttpError:
    case kBoundCookieSetForbidden:
    case kSigningQuotaExceeded:
      return std::nullopt;
    // Registration-only errors never trigger session deletion.
    case kSubdomainRegistrationWellKnownUnavailable:
    case kSubdomainRegistrationUnauthorized:
    case kSubdomainRegistrationWellKnownMalformed:
    case kFederatedNotAuthorizedByProvider:
    case kFederatedNotAuthorizedByRelyingParty:
    case kSessionProviderWellKnownUnavailable:
    case kSessionProviderWellKnownMalformed:
    case kSessionProviderWellKnownHasProviderOrigin:
    case kRelyingPartyWellKnownUnavailable:
    case kRelyingPartyWellKnownMalformed:
    case kRelyingPartyWellKnownHasRelyingOrigins:
    case kFederatedKeyThumbprintMismatch:
    case kInvalidFederatedSessionUrl:
    case kInvalidFederatedSessionProviderSessionMissing:
    case kInvalidFederatedSessionWrongProviderOrigin:
    case kInvalidFederatedKey:
    case kTooManyRelyingOriginLabels:
    case kEmptySessionConfig:
    case kRegistrationAttemptedChallenge:
    case kInvalidFederatedSessionProviderFailedToRestoreKey:
    case kFailedToUnwrapKey:
      NOTREACHED();
  }
}

bool SessionError::IsServerError() const {
  switch (type) {
    case kSuccess:
    case kKeyError:
    case kSigningError:
    case kNetError:
    case kProxyError:
    case kSigningQuotaExceeded:
    case kSessionDeletedDuringRefresh:
      return false;
    case kServerRequestedTermination:
    case kInvalidConfigJson:
    case kInvalidSessionId:
    case kInvalidCredentialsConfig:
    case kInvalidCredentialsType:
    case kInvalidCredentialsEmptyName:
    case kInvalidCredentialsCookie:
    case kInvalidCredentialsCookieCreationTime:
    case kInvalidCredentialsCookieName:
    case kInvalidCredentialsCookieParsing:
    case kInvalidCredentialsCookieUnpermittedAttribute:
    case kInvalidCredentialsCookieInvalidDomain:
    case kInvalidCredentialsCookiePrefix:
    case kInvalidChallenge:
    case kTooManyChallenges:
    case kInvalidFetcherUrl:
    case kInvalidRefreshUrl:
    case kPersistentHttpError:
    case kScopeOriginSameSiteMismatch:
    case kRefreshUrlSameSiteMismatch:
    case kInvalidScopeOrigin:
    case kScopeOriginContainsPath:
    case kTransientHttpError:
    case kMismatchedSessionId:
    case kRefreshInitiatorNotString:
    case kRefreshInitiatorInvalidHostPattern:
    case kInvalidScopeRulePath:
    case kInvalidScopeRuleHostPattern:
    case kScopeRuleOriginScopedHostPatternMismatch:
    case kScopeRuleSiteScopedHostPatternMismatch:
    case kInvalidScopeSpecification:
    case kMissingScopeSpecificationType:
    case kEmptyScopeSpecificationDomain:
    case kEmptyScopeSpecificationPath:
    case kInvalidScopeSpecificationType:
    case kMissingScope:
    case kNoCredentials:
    case kInvalidScopeIncludeSite:
    case kMissingScopeIncludeSite:
    case kBoundCookieSetForbidden:
      return true;
    // Registration-only errors never get reported to the server.
    case kSubdomainRegistrationWellKnownUnavailable:
    case kSubdomainRegistrationUnauthorized:
    case kSubdomainRegistrationWellKnownMalformed:
    case kFederatedNotAuthorizedByProvider:
    case kFederatedNotAuthorizedByRelyingParty:
    case kSessionProviderWellKnownUnavailable:
    case kSessionProviderWellKnownMalformed:
    case kSessionProviderWellKnownHasProviderOrigin:
    case kRelyingPartyWellKnownUnavailable:
    case kRelyingPartyWellKnownMalformed:
    case kRelyingPartyWellKnownHasRelyingOrigins:
    case kFederatedKeyThumbprintMismatch:
    case kInvalidFederatedSessionUrl:
    case kInvalidFederatedSessionProviderSessionMissing:
    case kInvalidFederatedSessionWrongProviderOrigin:
    case kInvalidFederatedKey:
    case kTooManyRelyingOriginLabels:
    case kEmptySessionConfig:
    case kRegistrationAttemptedChallenge:
    case kInvalidFederatedSessionProviderFailedToRestoreKey:
    case kFailedToUnwrapKey:
      NOTREACHED();
  }
}

}  // namespace net::device_bound_sessions
