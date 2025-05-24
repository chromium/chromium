// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_error.h"

namespace net::device_bound_sessions {

SessionError::SessionError(SessionError::ErrorType type,
                           net::SchemefulSite site,
                           std::optional<std::string> session_id)
    : type(type), site(std::move(site)), session_id(std::move(session_id)) {}

SessionError::~SessionError() = default;

SessionError::SessionError(SessionError&&) noexcept = default;
SessionError& SessionError::operator=(SessionError&&) noexcept = default;

bool SessionError::IsFatal() const {
  using enum ErrorType;

  switch (type) {
    case kSuccess:
      return false;
    case kKeyError:
    case kSigningError:
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
      return true;

    case kNetError:
    case kTransientHttpError:
      return false;
  }
}

}  // namespace net::device_bound_sessions
