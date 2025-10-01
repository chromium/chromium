// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_error.h"

#include "base/notreached.h"

namespace net::device_bound_sessions {

SessionError::SessionError(SessionError::ErrorType type) : type(type) {}

SessionError::~SessionError() = default;

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
      return DeletionReason::kRefreshFatalError;
    case kInvalidConfigJson:
    case kInvalidSessionId:
    case kInvalidCredentials:
    case kInvalidFetcherUrl:
    case kInvalidRefreshUrl:
    case kScopeOriginSameSiteMismatch:
    case kRefreshUrlSameSiteMismatch:
    case kInvalidScopeOrigin:
    case kMismatchedSessionId:
    case kInvalidRefreshInitiators:
    case kInvalidScopeRule:
    case kMissingScope:
    case kNoCredentials:
    case kInvalidScopeIncludeSite:
      return DeletionReason::kInvalidSessionParams;
    case kNetError:
    case kTransientHttpError:
    case kBoundCookieSetForbidden:
      return std::nullopt;
    // Registration-only errors never trigger session deletion.
    case kSubdomainRegistrationWellKnownUnavailable:
    case kSubdomainRegistrationUnauthorized:
    case kSubdomainRegistrationWellKnownMalformed:
    case kFederatedNotAuthorized:
    case kSessionProviderWellKnownUnavailable:
    case kSessionProviderWellKnownMalformed:
    case kRelyingPartyWellKnownUnavailable:
    case kRelyingPartyWellKnownMalformed:
    case kFederatedKeyThumbprintMismatch:
    case kInvalidFederatedSessionUrl:
    case kInvalidFederatedSession:
    case kInvalidFederatedKey:
    case kTooManyRelyingOriginLabels:
      NOTREACHED();
  }
}

bool SessionError::IsServerError() const {
  switch (type) {
    case kSuccess:
    case kKeyError:
    case kSigningError:
    case kNetError:
      return false;
    case kServerRequestedTermination:
    case kInvalidConfigJson:
    case kInvalidSessionId:
    case kInvalidCredentials:
    case kInvalidChallenge:
    case kTooManyChallenges:
    case kInvalidFetcherUrl:
    case kInvalidRefreshUrl:
    case kPersistentHttpError:
    case kScopeOriginSameSiteMismatch:
    case kRefreshUrlSameSiteMismatch:
    case kInvalidScopeOrigin:
    case kTransientHttpError:
    case kMismatchedSessionId:
    case kInvalidRefreshInitiators:
    case kInvalidScopeRule:
    case kMissingScope:
    case kNoCredentials:
    case kInvalidScopeIncludeSite:
    case kBoundCookieSetForbidden:
      return true;
    // Registration-only errors never get reported to the server.
    case kSubdomainRegistrationWellKnownUnavailable:
    case kSubdomainRegistrationUnauthorized:
    case kSubdomainRegistrationWellKnownMalformed:
    case kFederatedNotAuthorized:
    case kSessionProviderWellKnownUnavailable:
    case kSessionProviderWellKnownMalformed:
    case kRelyingPartyWellKnownUnavailable:
    case kRelyingPartyWellKnownMalformed:
    case kFederatedKeyThumbprintMismatch:
    case kInvalidFederatedSessionUrl:
    case kInvalidFederatedSession:
    case kInvalidFederatedKey:
    case kTooManyRelyingOriginLabels:
      NOTREACHED();
  }
}

}  // namespace net::device_bound_sessions
