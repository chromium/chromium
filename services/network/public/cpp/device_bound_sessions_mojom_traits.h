// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_DEVICE_BOUND_SESSIONS_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_DEVICE_BOUND_SESSIONS_MOJOM_TRAITS_H_

#include "base/time/time.h"
#include "net/device_bound_sessions/cookie_craving_display.h"
#include "net/device_bound_sessions/deletion_reason.h"
#include "net/device_bound_sessions/session_access.h"
#include "net/device_bound_sessions/session_error.h"
#include "net/device_bound_sessions/session_event.h"
#include "net/device_bound_sessions/session_inclusion_rules_display.h"
#include "net/device_bound_sessions/session_key.h"
#include "net/device_bound_sessions/session_params.h"
#include "net/device_bound_sessions/url_rule_display.h"
#include "services/network/public/mojom/device_bound_sessions.mojom-shared.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace base {
class UnguessableToken;
}

namespace net::device_bound_sessions {
enum class ChallengeResult;
enum class InclusionResult;
enum class RefreshResult;
struct SessionDisplay;
}  // namespace net::device_bound_sessions

namespace net {
enum class CookieSameSite;
}

namespace mojo {

template <>
struct StructTraits<network::mojom::DeviceBoundSessionKeyDataView,
                    net::device_bound_sessions::SessionKey> {
  static const net::SchemefulSite& site(
      const net::device_bound_sessions::SessionKey& obj) {
    return obj.site;
  }

  static const std::string& id(
      const net::device_bound_sessions::SessionKey& obj) {
    return obj.id.value();
  }

  static bool Read(network::mojom::DeviceBoundSessionKeyDataView data,
                   net::device_bound_sessions::SessionKey* out);
};

template <>
struct EnumTraits<network::mojom::DeviceBoundSessionAccessType,
                  net::device_bound_sessions::SessionAccess::AccessType> {
  static network::mojom::DeviceBoundSessionAccessType ToMojom(
      net::device_bound_sessions::SessionAccess::AccessType access_type) {
    using enum net::device_bound_sessions::SessionAccess::AccessType;
    switch (access_type) {
      case kCreation:
        return network::mojom::DeviceBoundSessionAccessType::kCreation;
      case kUpdate:
        return network::mojom::DeviceBoundSessionAccessType::kUpdate;
      case kTermination:
        return network::mojom::DeviceBoundSessionAccessType::kTermination;
    }
  }

  static net::device_bound_sessions::SessionAccess::AccessType FromMojom(
      network::mojom::DeviceBoundSessionAccessType input) {
    using enum net::device_bound_sessions::SessionAccess::AccessType;
    switch (input) {
      case network::mojom::DeviceBoundSessionAccessType::kCreation:
        return kCreation;
      case network::mojom::DeviceBoundSessionAccessType::kUpdate:
        return kUpdate;
      case network::mojom::DeviceBoundSessionAccessType::kTermination:
        return kTermination;
    }
    NOTREACHED();
  }
};

template <>
struct StructTraits<network::mojom::DeviceBoundSessionAccessDataView,
                    net::device_bound_sessions::SessionAccess> {
  static net::device_bound_sessions::SessionAccess::AccessType access_type(
      const net::device_bound_sessions::SessionAccess& access) {
    return access.access_type;
  }

  static const net::device_bound_sessions::SessionKey& session_key(
      const net::device_bound_sessions::SessionAccess& access) {
    return access.session_key;
  }

  static const std::vector<std::string>& cookies(
      const net::device_bound_sessions::SessionAccess& access) {
    return access.cookies;
  }

  static bool Read(network::mojom::DeviceBoundSessionAccessDataView data,
                   net::device_bound_sessions::SessionAccess* out);
};

template <>
struct EnumTraits<network::mojom::DeviceBoundSessionDeletionReason,
                  net::device_bound_sessions::DeletionReason> {
  static network::mojom::DeviceBoundSessionDeletionReason ToMojom(
      net::device_bound_sessions::DeletionReason reason) {
    using enum net::device_bound_sessions::DeletionReason;
    switch (reason) {
      case kExpired:
        return network::mojom::DeviceBoundSessionDeletionReason::kExpired;
      case kFailedToRestoreKey:
        return network::mojom::DeviceBoundSessionDeletionReason::
            kFailedToRestoreKey;
      case kFailedToUnwrapKey:
        return network::mojom::DeviceBoundSessionDeletionReason::
            kFailedToUnwrapKey;
      case kStoragePartitionCleared:
        return network::mojom::DeviceBoundSessionDeletionReason::
            kStoragePartitionCleared;
      case kClearBrowsingData:
        return network::mojom::DeviceBoundSessionDeletionReason::
            kClearBrowsingData;
      case kServerRequested:
        return network::mojom::DeviceBoundSessionDeletionReason::
            kServerRequested;
      case kInvalidSessionParams:
        return network::mojom::DeviceBoundSessionDeletionReason::
            kInvalidSessionParams;
      case kRefreshFatalError:
        return network::mojom::DeviceBoundSessionDeletionReason::
            kRefreshFatalError;
      case kDevTools:
        return network::mojom::DeviceBoundSessionDeletionReason::kDevTools;
    }
  }

  static net::device_bound_sessions::DeletionReason FromMojom(
      network::mojom::DeviceBoundSessionDeletionReason input) {
    using enum net::device_bound_sessions::DeletionReason;
    switch (input) {
      case network::mojom::DeviceBoundSessionDeletionReason::kExpired:
        return kExpired;
      case network::mojom::DeviceBoundSessionDeletionReason::
          kFailedToRestoreKey:
        return kFailedToRestoreKey;
      case network::mojom::DeviceBoundSessionDeletionReason::kFailedToUnwrapKey:
        return kFailedToUnwrapKey;
      case network::mojom::DeviceBoundSessionDeletionReason::
          kStoragePartitionCleared:
        return kStoragePartitionCleared;
      case network::mojom::DeviceBoundSessionDeletionReason::kClearBrowsingData:
        return kClearBrowsingData;
      case network::mojom::DeviceBoundSessionDeletionReason::kServerRequested:
        return kServerRequested;
      case network::mojom::DeviceBoundSessionDeletionReason::
          kInvalidSessionParams:
        return kInvalidSessionParams;
      case network::mojom::DeviceBoundSessionDeletionReason::kRefreshFatalError:
        return kRefreshFatalError;
      case network::mojom::DeviceBoundSessionDeletionReason::kDevTools:
        return kDevTools;
    }
    NOTREACHED();
  }
};

template <>
struct EnumTraits<
    network::mojom::DeviceBoundSessionScopeSpecificationType,
    net::device_bound_sessions::SessionParams::Scope::Specification::Type> {
  static network::mojom::DeviceBoundSessionScopeSpecificationType ToMojom(
      net::device_bound_sessions::SessionParams::Scope::Specification::Type
          type) {
    using enum net::device_bound_sessions::SessionParams::Scope::Specification::
        Type;
    switch (type) {
      case kExclude:
        return network::mojom::DeviceBoundSessionScopeSpecificationType::
            kExclude;
      case kInclude:
        return network::mojom::DeviceBoundSessionScopeSpecificationType::
            kInclude;
    }
  }

  static net::device_bound_sessions::SessionParams::Scope::Specification::Type
  FromMojom(network::mojom::DeviceBoundSessionScopeSpecificationType input) {
    using enum net::device_bound_sessions::SessionParams::Scope::Specification::
        Type;
    switch (input) {
      case network::mojom::DeviceBoundSessionScopeSpecificationType::kExclude:
        return kExclude;
      case network::mojom::DeviceBoundSessionScopeSpecificationType::kInclude:
        return kInclude;
    }
    NOTREACHED();
  }
};

template <>
struct EnumTraits<network::mojom::DeviceBoundSessionError,
                  net::device_bound_sessions::SessionError::ErrorType> {
  static network::mojom::DeviceBoundSessionError ToMojom(
      net::device_bound_sessions::SessionError::ErrorType type) {
    using enum net::device_bound_sessions::SessionError::ErrorType;
    switch (type) {
      case kSuccess:
        return network::mojom::DeviceBoundSessionError::kSuccess;
      case kKeyError:
        return network::mojom::DeviceBoundSessionError::kKeyError;
      case kSigningError:
        return network::mojom::DeviceBoundSessionError::kSigningError;
      case kServerRequestedTermination:
        return network::mojom::DeviceBoundSessionError::
            kServerRequestedTermination;
      case kInvalidSessionId:
        return network::mojom::DeviceBoundSessionError::kInvalidSessionId;
      case kInvalidChallenge:
        return network::mojom::DeviceBoundSessionError::kInvalidChallenge;
      case kTooManyChallenges:
        return network::mojom::DeviceBoundSessionError::kTooManyChallenges;
      case kInvalidFetcherUrl:
        return network::mojom::DeviceBoundSessionError::kInvalidFetcherUrl;
      case kInvalidRefreshUrl:
        return network::mojom::DeviceBoundSessionError::kInvalidRefreshUrl;
      case kTransientHttpError:
        return network::mojom::DeviceBoundSessionError::kTransientHttpError;
      case kScopeOriginSameSiteMismatch:
        return network::mojom::DeviceBoundSessionError::
            kScopeOriginSameSiteMismatch;
      case kRefreshUrlSameSiteMismatch:
        return network::mojom::DeviceBoundSessionError::
            kRefreshUrlSameSiteMismatch;
      case kMismatchedSessionId:
        return network::mojom::DeviceBoundSessionError::kMismatchedSessionId;
      case kMissingScope:
        return network::mojom::DeviceBoundSessionError::kMissingScope;
      case kNoCredentials:
        return network::mojom::DeviceBoundSessionError::kNoCredentials;
      case kSubdomainRegistrationWellKnownUnavailable:
        return network::mojom::DeviceBoundSessionError::
            kSubdomainRegistrationWellKnownUnavailable;
      case kSubdomainRegistrationUnauthorized:
        return network::mojom::DeviceBoundSessionError::
            kSubdomainRegistrationUnauthorized;
      case kSubdomainRegistrationWellKnownMalformed:
        return network::mojom::DeviceBoundSessionError::
            kSubdomainRegistrationWellKnownMalformed;
      case kSessionProviderWellKnownUnavailable:
        return network::mojom::DeviceBoundSessionError::
            kSessionProviderWellKnownUnavailable;
      case kRelyingPartyWellKnownUnavailable:
        return network::mojom::DeviceBoundSessionError::
            kRelyingPartyWellKnownUnavailable;
      case kFederatedKeyThumbprintMismatch:
        return network::mojom::DeviceBoundSessionError::
            kFederatedKeyThumbprintMismatch;
      case kInvalidFederatedSessionUrl:
        return network::mojom::DeviceBoundSessionError::
            kInvalidFederatedSessionUrl;
      case kInvalidFederatedKey:
        return network::mojom::DeviceBoundSessionError::kInvalidFederatedKey;
      case kTooManyRelyingOriginLabels:
        return network::mojom::DeviceBoundSessionError::
            kTooManyRelyingOriginLabels;
      case kBoundCookieSetForbidden:
        return network::mojom::DeviceBoundSessionError::
            kBoundCookieSetForbidden;
      case kNetError:
        return network::mojom::DeviceBoundSessionError::kNetError;
      case kProxyError:
        return network::mojom::DeviceBoundSessionError::kProxyError;
      case kEmptySessionConfig:
        return network::mojom::DeviceBoundSessionError::kEmptySessionConfig;
      case kInvalidCredentialsConfig:
        return network::mojom::DeviceBoundSessionError::
            kInvalidCredentialsConfig;
      case kInvalidCredentialsType:
        return network::mojom::DeviceBoundSessionError::kInvalidCredentialsType;
      case kInvalidCredentialsEmptyName:
        return network::mojom::DeviceBoundSessionError::
            kInvalidCredentialsEmptyName;
      case kInvalidCredentialsCookie:
        return network::mojom::DeviceBoundSessionError::
            kInvalidCredentialsCookie;
      case kPersistentHttpError:
        return network::mojom::DeviceBoundSessionError::kPersistentHttpError;
      case kRegistrationAttemptedChallenge:
        return network::mojom::DeviceBoundSessionError::
            kRegistrationAttemptedChallenge;
      case kInvalidScopeOrigin:
        return network::mojom::DeviceBoundSessionError::kInvalidScopeOrigin;
      case kScopeOriginContainsPath:
        return network::mojom::DeviceBoundSessionError::
            kScopeOriginContainsPath;
      case kRefreshInitiatorNotString:
        return network::mojom::DeviceBoundSessionError::
            kRefreshInitiatorNotString;
      case kRefreshInitiatorInvalidHostPattern:
        return network::mojom::DeviceBoundSessionError::
            kRefreshInitiatorInvalidHostPattern;
      case kInvalidScopeSpecification:
        return network::mojom::DeviceBoundSessionError::
            kInvalidScopeSpecification;
      case kMissingScopeSpecificationType:
        return network::mojom::DeviceBoundSessionError::
            kMissingScopeSpecificationType;
      case kEmptyScopeSpecificationDomain:
        return network::mojom::DeviceBoundSessionError::
            kEmptyScopeSpecificationDomain;
      case kEmptyScopeSpecificationPath:
        return network::mojom::DeviceBoundSessionError::
            kEmptyScopeSpecificationPath;
      case kInvalidScopeSpecificationType:
        return network::mojom::DeviceBoundSessionError::
            kInvalidScopeSpecificationType;
      case kInvalidScopeIncludeSite:
        return network::mojom::DeviceBoundSessionError::
            kInvalidScopeIncludeSite;
      case kMissingScopeIncludeSite:
        return network::mojom::DeviceBoundSessionError::
            kMissingScopeIncludeSite;
      case kFederatedNotAuthorizedByProvider:
        return network::mojom::DeviceBoundSessionError::
            kFederatedNotAuthorizedByProvider;
      case kFederatedNotAuthorizedByRelyingParty:
        return network::mojom::DeviceBoundSessionError::
            kFederatedNotAuthorizedByRelyingParty;
      case kSessionProviderWellKnownMalformed:
        return network::mojom::DeviceBoundSessionError::
            kSessionProviderWellKnownMalformed;
      case kSessionProviderWellKnownHasProviderOrigin:
        return network::mojom::DeviceBoundSessionError::
            kSessionProviderWellKnownHasProviderOrigin;
      case kRelyingPartyWellKnownMalformed:
        return network::mojom::DeviceBoundSessionError::
            kRelyingPartyWellKnownMalformed;
      case kRelyingPartyWellKnownHasRelyingOrigins:
        return network::mojom::DeviceBoundSessionError::
            kRelyingPartyWellKnownHasRelyingOrigins;
      case kInvalidFederatedSessionProviderSessionMissing:
        return network::mojom::DeviceBoundSessionError::
            kInvalidFederatedSessionProviderSessionMissing;
      case kInvalidFederatedSessionWrongProviderOrigin:
        return network::mojom::DeviceBoundSessionError::
            kInvalidFederatedSessionWrongProviderOrigin;
      case kInvalidCredentialsCookieCreationTime:
        return network::mojom::DeviceBoundSessionError::
            kInvalidCredentialsCookieCreationTime;
      case kInvalidCredentialsCookieName:
        return network::mojom::DeviceBoundSessionError::
            kInvalidCredentialsCookieName;
      case kInvalidCredentialsCookieParsing:
        return network::mojom::DeviceBoundSessionError::
            kInvalidCredentialsCookieParsing;
      case kInvalidCredentialsCookieUnpermittedAttribute:
        return network::mojom::DeviceBoundSessionError::
            kInvalidCredentialsCookieUnpermittedAttribute;
      case kInvalidCredentialsCookieInvalidDomain:
        return network::mojom::DeviceBoundSessionError::
            kInvalidCredentialsCookieInvalidDomain;
      case kInvalidCredentialsCookiePrefix:
        return network::mojom::DeviceBoundSessionError::
            kInvalidCredentialsCookiePrefix;
      case kInvalidScopeRulePath:
        return network::mojom::DeviceBoundSessionError::kInvalidScopeRulePath;
      case kInvalidScopeRuleHostPattern:
        return network::mojom::DeviceBoundSessionError::
            kInvalidScopeRuleHostPattern;
      case kScopeRuleOriginScopedHostPatternMismatch:
        return network::mojom::DeviceBoundSessionError::
            kScopeRuleOriginScopedHostPatternMismatch;
      case kScopeRuleSiteScopedHostPatternMismatch:
        return network::mojom::DeviceBoundSessionError::
            kScopeRuleSiteScopedHostPatternMismatch;
      case kSigningQuotaExceeded:
        return network::mojom::DeviceBoundSessionError::kSigningQuotaExceeded;
      case kInvalidConfigJson:
        return network::mojom::DeviceBoundSessionError::kInvalidConfigJson;
      case kInvalidFederatedSessionProviderFailedToRestoreKey:
        return network::mojom::DeviceBoundSessionError::
            kInvalidFederatedSessionProviderFailedToRestoreKey;
      case kFailedToUnwrapKey:
        return network::mojom::DeviceBoundSessionError::kFailedToUnwrapKey;
      case kSessionDeletedDuringRefresh:
        return network::mojom::DeviceBoundSessionError::
            kSessionDeletedDuringRefresh;
      case kTransientSigningError:
        return network::mojom::DeviceBoundSessionError::kTransientSigningError;
    }
  }

  static net::device_bound_sessions::SessionError::ErrorType FromMojom(
      network::mojom::DeviceBoundSessionError input) {
    using enum net::device_bound_sessions::SessionError::ErrorType;
    switch (input) {
      case network::mojom::DeviceBoundSessionError::kSuccess:
        return kSuccess;
      case network::mojom::DeviceBoundSessionError::kKeyError:
        return kKeyError;
      case network::mojom::DeviceBoundSessionError::kSigningError:
        return kSigningError;
      case network::mojom::DeviceBoundSessionError::kServerRequestedTermination:
        return kServerRequestedTermination;
      case network::mojom::DeviceBoundSessionError::kInvalidSessionId:
        return kInvalidSessionId;
      case network::mojom::DeviceBoundSessionError::kInvalidChallenge:
        return kInvalidChallenge;
      case network::mojom::DeviceBoundSessionError::kTooManyChallenges:
        return kTooManyChallenges;
      case network::mojom::DeviceBoundSessionError::kInvalidFetcherUrl:
        return kInvalidFetcherUrl;
      case network::mojom::DeviceBoundSessionError::kInvalidRefreshUrl:
        return kInvalidRefreshUrl;
      case network::mojom::DeviceBoundSessionError::kTransientHttpError:
        return kTransientHttpError;
      case network::mojom::DeviceBoundSessionError::
          kScopeOriginSameSiteMismatch:
        return kScopeOriginSameSiteMismatch;
      case network::mojom::DeviceBoundSessionError::kRefreshUrlSameSiteMismatch:
        return kRefreshUrlSameSiteMismatch;
      case network::mojom::DeviceBoundSessionError::kMismatchedSessionId:
        return kMismatchedSessionId;
      case network::mojom::DeviceBoundSessionError::kMissingScope:
        return kMissingScope;
      case network::mojom::DeviceBoundSessionError::kNoCredentials:
        return kNoCredentials;
      case network::mojom::DeviceBoundSessionError::
          kSubdomainRegistrationWellKnownUnavailable:
        return kSubdomainRegistrationWellKnownUnavailable;
      case network::mojom::DeviceBoundSessionError::
          kSubdomainRegistrationUnauthorized:
        return kSubdomainRegistrationUnauthorized;
      case network::mojom::DeviceBoundSessionError::
          kSubdomainRegistrationWellKnownMalformed:
        return kSubdomainRegistrationWellKnownMalformed;
      case network::mojom::DeviceBoundSessionError::
          kSessionProviderWellKnownUnavailable:
        return kSessionProviderWellKnownUnavailable;
      case network::mojom::DeviceBoundSessionError::
          kRelyingPartyWellKnownUnavailable:
        return kRelyingPartyWellKnownUnavailable;
      case network::mojom::DeviceBoundSessionError::
          kFederatedKeyThumbprintMismatch:
        return kFederatedKeyThumbprintMismatch;
      case network::mojom::DeviceBoundSessionError::kInvalidFederatedSessionUrl:
        return kInvalidFederatedSessionUrl;
      case network::mojom::DeviceBoundSessionError::kInvalidFederatedKey:
        return kInvalidFederatedKey;
      case network::mojom::DeviceBoundSessionError::kTooManyRelyingOriginLabels:
        return kTooManyRelyingOriginLabels;
      case network::mojom::DeviceBoundSessionError::kBoundCookieSetForbidden:
        return kBoundCookieSetForbidden;
      case network::mojom::DeviceBoundSessionError::kNetError:
        return kNetError;
      case network::mojom::DeviceBoundSessionError::kProxyError:
        return kProxyError;
      case network::mojom::DeviceBoundSessionError::kEmptySessionConfig:
        return kEmptySessionConfig;
      case network::mojom::DeviceBoundSessionError::kInvalidCredentialsConfig:
        return kInvalidCredentialsConfig;
      case network::mojom::DeviceBoundSessionError::kInvalidCredentialsType:
        return kInvalidCredentialsType;
      case network::mojom::DeviceBoundSessionError::
          kInvalidCredentialsEmptyName:
        return kInvalidCredentialsEmptyName;
      case network::mojom::DeviceBoundSessionError::kInvalidCredentialsCookie:
        return kInvalidCredentialsCookie;
      case network::mojom::DeviceBoundSessionError::kPersistentHttpError:
        return kPersistentHttpError;
      case network::mojom::DeviceBoundSessionError::
          kRegistrationAttemptedChallenge:
        return kRegistrationAttemptedChallenge;
      case network::mojom::DeviceBoundSessionError::kInvalidScopeOrigin:
        return kInvalidScopeOrigin;
      case network::mojom::DeviceBoundSessionError::kScopeOriginContainsPath:
        return kScopeOriginContainsPath;
      case network::mojom::DeviceBoundSessionError::kRefreshInitiatorNotString:
        return kRefreshInitiatorNotString;
      case network::mojom::DeviceBoundSessionError::
          kRefreshInitiatorInvalidHostPattern:
        return kRefreshInitiatorInvalidHostPattern;
      case network::mojom::DeviceBoundSessionError::kInvalidScopeSpecification:
        return kInvalidScopeSpecification;
      case network::mojom::DeviceBoundSessionError::
          kMissingScopeSpecificationType:
        return kMissingScopeSpecificationType;
      case network::mojom::DeviceBoundSessionError::
          kEmptyScopeSpecificationDomain:
        return kEmptyScopeSpecificationDomain;
      case network::mojom::DeviceBoundSessionError::
          kEmptyScopeSpecificationPath:
        return kEmptyScopeSpecificationPath;
      case network::mojom::DeviceBoundSessionError::
          kInvalidScopeSpecificationType:
        return kInvalidScopeSpecificationType;
      case network::mojom::DeviceBoundSessionError::kInvalidScopeIncludeSite:
        return kInvalidScopeIncludeSite;
      case network::mojom::DeviceBoundSessionError::kMissingScopeIncludeSite:
        return kMissingScopeIncludeSite;
      case network::mojom::DeviceBoundSessionError::
          kFederatedNotAuthorizedByProvider:
        return kFederatedNotAuthorizedByProvider;
      case network::mojom::DeviceBoundSessionError::
          kFederatedNotAuthorizedByRelyingParty:
        return kFederatedNotAuthorizedByRelyingParty;
      case network::mojom::DeviceBoundSessionError::
          kSessionProviderWellKnownMalformed:
        return kSessionProviderWellKnownMalformed;
      case network::mojom::DeviceBoundSessionError::
          kSessionProviderWellKnownHasProviderOrigin:
        return kSessionProviderWellKnownHasProviderOrigin;
      case network::mojom::DeviceBoundSessionError::
          kRelyingPartyWellKnownMalformed:
        return kRelyingPartyWellKnownMalformed;
      case network::mojom::DeviceBoundSessionError::
          kRelyingPartyWellKnownHasRelyingOrigins:
        return kRelyingPartyWellKnownHasRelyingOrigins;
      case network::mojom::DeviceBoundSessionError::
          kInvalidFederatedSessionProviderSessionMissing:
        return kInvalidFederatedSessionProviderSessionMissing;
      case network::mojom::DeviceBoundSessionError::
          kInvalidFederatedSessionWrongProviderOrigin:
        return kInvalidFederatedSessionWrongProviderOrigin;
      case network::mojom::DeviceBoundSessionError::
          kInvalidCredentialsCookieCreationTime:
        return kInvalidCredentialsCookieCreationTime;
      case network::mojom::DeviceBoundSessionError::
          kInvalidCredentialsCookieName:
        return kInvalidCredentialsCookieName;
      case network::mojom::DeviceBoundSessionError::
          kInvalidCredentialsCookieParsing:
        return kInvalidCredentialsCookieParsing;
      case network::mojom::DeviceBoundSessionError::
          kInvalidCredentialsCookieUnpermittedAttribute:
        return kInvalidCredentialsCookieUnpermittedAttribute;
      case network::mojom::DeviceBoundSessionError::
          kInvalidCredentialsCookieInvalidDomain:
        return kInvalidCredentialsCookieInvalidDomain;
      case network::mojom::DeviceBoundSessionError::
          kInvalidCredentialsCookiePrefix:
        return kInvalidCredentialsCookiePrefix;
      case network::mojom::DeviceBoundSessionError::kInvalidScopeRulePath:
        return kInvalidScopeRulePath;
      case network::mojom::DeviceBoundSessionError::
          kInvalidScopeRuleHostPattern:
        return kInvalidScopeRuleHostPattern;
      case network::mojom::DeviceBoundSessionError::
          kScopeRuleOriginScopedHostPatternMismatch:
        return kScopeRuleOriginScopedHostPatternMismatch;
      case network::mojom::DeviceBoundSessionError::
          kScopeRuleSiteScopedHostPatternMismatch:
        return kScopeRuleSiteScopedHostPatternMismatch;
      case network::mojom::DeviceBoundSessionError::kSigningQuotaExceeded:
        return kSigningQuotaExceeded;
      case network::mojom::DeviceBoundSessionError::kInvalidConfigJson:
        return kInvalidConfigJson;
      case network::mojom::DeviceBoundSessionError::
          kInvalidFederatedSessionProviderFailedToRestoreKey:
        return kInvalidFederatedSessionProviderFailedToRestoreKey;
      case network::mojom::DeviceBoundSessionError::kFailedToUnwrapKey:
        return kFailedToUnwrapKey;
      case network::mojom::DeviceBoundSessionError::
          kSessionDeletedDuringRefresh:
        return kSessionDeletedDuringRefresh;
      case network::mojom::DeviceBoundSessionError::kTransientSigningError:
        return kTransientSigningError;
    }
    NOTREACHED();
  }
};

template <>
struct EnumTraits<network::mojom::DeviceBoundSessionInclusionResult,
                  net::device_bound_sessions::InclusionResult> {
  static network::mojom::DeviceBoundSessionInclusionResult ToMojom(
      net::device_bound_sessions::InclusionResult inclusion_result);

  static net::device_bound_sessions::InclusionResult FromMojom(
      network::mojom::DeviceBoundSessionInclusionResult input);
};

template <>
struct StructTraits<
    network::mojom::DeviceBoundSessionScopeSpecificationDataView,
    net::device_bound_sessions::SessionParams::Scope::Specification> {
  static net::device_bound_sessions::SessionParams::Scope::Specification::Type
  type(const net::device_bound_sessions::SessionParams::Scope::Specification&
           obj) {
    return obj.type;
  }

  static const std::string& domain(
      const net::device_bound_sessions::SessionParams::Scope::Specification&
          obj) {
    return obj.domain;
  }

  static const std::string& path(
      const net::device_bound_sessions::SessionParams::Scope::Specification&
          obj) {
    return obj.path;
  }

  static bool Read(
      network::mojom::DeviceBoundSessionScopeSpecificationDataView data,
      net::device_bound_sessions::SessionParams::Scope::Specification* out);
};

template <>
struct StructTraits<network::mojom::DeviceBoundSessionScopeDataView,
                    net::device_bound_sessions::SessionParams::Scope> {
  static bool include_site(
      const net::device_bound_sessions::SessionParams::Scope& obj) {
    return obj.include_site;
  }

  static const std::vector<
      net::device_bound_sessions::SessionParams::Scope::Specification>&
  specifications(const net::device_bound_sessions::SessionParams::Scope& obj) {
    return obj.specifications;
  }

  static const std::string& origin(
      const net::device_bound_sessions::SessionParams::Scope& obj) {
    return obj.origin;
  }

  static bool Read(network::mojom::DeviceBoundSessionScopeDataView data,
                   net::device_bound_sessions::SessionParams::Scope* out);
};

template <>
struct StructTraits<network::mojom::DeviceBoundSessionCredentialDataView,
                    net::device_bound_sessions::SessionParams::Credential> {
  static const std::string& name(
      const net::device_bound_sessions::SessionParams::Credential& obj) {
    return obj.name;
  }

  static const std::string& attributes(
      const net::device_bound_sessions::SessionParams::Credential& obj) {
    return obj.attributes;
  }

  static bool Read(network::mojom::DeviceBoundSessionCredentialDataView data,
                   net::device_bound_sessions::SessionParams::Credential* out);
};

template <>
struct StructTraits<network::mojom::DeviceBoundSessionParamsDataView,
                    net::device_bound_sessions::SessionParams> {
  static const std::string& session_id(
      const net::device_bound_sessions::SessionParams& obj) {
    return obj.session_id;
  }

  static const GURL& fetcher_url(
      const net::device_bound_sessions::SessionParams& obj) {
    return obj.fetcher_url;
  }

  static const std::string& refresh_url(
      const net::device_bound_sessions::SessionParams& obj) {
    return obj.refresh_url;
  }

  static const net::device_bound_sessions::SessionParams::Scope& scope(
      const net::device_bound_sessions::SessionParams& obj) {
    return obj.scope;
  }

  static const std::vector<
      net::device_bound_sessions::SessionParams::Credential>&
  credentials(const net::device_bound_sessions::SessionParams& obj) {
    return obj.credentials;
  }

  static const std::vector<std::string>& allowed_refresh_initiators(
      const net::device_bound_sessions::SessionParams& obj) {
    return obj.allowed_refresh_initiators;
  }

  static bool Read(network::mojom::DeviceBoundSessionParamsDataView data,
                   net::device_bound_sessions::SessionParams* out);
};

template <>
struct StructTraits<
    network::mojom::DeviceBoundSessionCookieCravingDisplayDataView,
    net::device_bound_sessions::CookieCravingDisplay> {
  static const std::string& name(
      const net::device_bound_sessions::CookieCravingDisplay& r);
  static const std::string& domain(
      const net::device_bound_sessions::CookieCravingDisplay& r);
  static const std::string& path(
      const net::device_bound_sessions::CookieCravingDisplay& r);
  static bool secure(const net::device_bound_sessions::CookieCravingDisplay& r);
  static bool http_only(
      const net::device_bound_sessions::CookieCravingDisplay& r);
  static net::CookieSameSite same_site(
      const net::device_bound_sessions::CookieCravingDisplay& r);
  static bool Read(
      network::mojom::DeviceBoundSessionCookieCravingDisplayDataView data,
      net::device_bound_sessions::CookieCravingDisplay* out);
};

template <>
struct StructTraits<network::mojom::DeviceBoundSessionUrlRuleDisplayDataView,
                    net::device_bound_sessions::UrlRuleDisplay> {
  static net::device_bound_sessions::InclusionResult rule_type(
      const net::device_bound_sessions::UrlRuleDisplay& r);

  static const std::string& host_pattern(
      const net::device_bound_sessions::UrlRuleDisplay& r);

  static const std::string& path_prefix(
      const net::device_bound_sessions::UrlRuleDisplay& r);

  static bool Read(
      network::mojom::DeviceBoundSessionUrlRuleDisplayDataView data,
      net::device_bound_sessions::UrlRuleDisplay* out);
};

template <>
struct StructTraits<
    network::mojom::DeviceBoundSessionInclusionRulesDisplayDataView,
    net::device_bound_sessions::SessionInclusionRulesDisplay> {
  static const std::string& origin(
      const net::device_bound_sessions::SessionInclusionRulesDisplay& r);

  static bool include_site(
      const net::device_bound_sessions::SessionInclusionRulesDisplay& r);

  static const std::vector<net::device_bound_sessions::UrlRuleDisplay>&
  url_rules(const net::device_bound_sessions::SessionInclusionRulesDisplay& r);

  static bool Read(
      network::mojom::DeviceBoundSessionInclusionRulesDisplayDataView data,
      net::device_bound_sessions::SessionInclusionRulesDisplay* out);
};

template <>
struct StructTraits<network::mojom::DeviceBoundSessionDisplayDataView,
                    net::device_bound_sessions::SessionDisplay> {
  static const net::device_bound_sessions::SessionKey& key(
      const net::device_bound_sessions::SessionDisplay& r);
  static const GURL& refresh_url(
      const net::device_bound_sessions::SessionDisplay& r);
  static const net::device_bound_sessions::SessionInclusionRulesDisplay&
  inclusion_rules(const net::device_bound_sessions::SessionDisplay& r);
  static const std::vector<net::device_bound_sessions::CookieCravingDisplay>&
  cookie_cravings(const net::device_bound_sessions::SessionDisplay& r);
  static base::Time expiry_date(
      const net::device_bound_sessions::SessionDisplay& r);
  static const std::optional<std::string>& cached_challenge(
      const net::device_bound_sessions::SessionDisplay& r);
  static const std::vector<std::string>& allowed_refresh_initiators(
      const net::device_bound_sessions::SessionDisplay& r);
  static bool Read(network::mojom::DeviceBoundSessionDisplayDataView data,
                   net::device_bound_sessions::SessionDisplay* out);
};

template <>
struct EnumTraits<network::mojom::DeviceBoundSessionRefreshResult,
                  net::device_bound_sessions::RefreshResult> {
  static network::mojom::DeviceBoundSessionRefreshResult ToMojom(
      net::device_bound_sessions::RefreshResult input);
  static net::device_bound_sessions::RefreshResult FromMojom(
      network::mojom::DeviceBoundSessionRefreshResult input);
};

template <>
struct EnumTraits<network::mojom::DeviceBoundSessionChallengeResult,
                  net::device_bound_sessions::ChallengeResult> {
  static network::mojom::DeviceBoundSessionChallengeResult ToMojom(
      net::device_bound_sessions::ChallengeResult input);
  static net::device_bound_sessions::ChallengeResult FromMojom(
      network::mojom::DeviceBoundSessionChallengeResult input);
};

// LINT.IfChange(SessionEventTypeDetails)
template <>
struct UnionTraits<network::mojom::DeviceBoundSessionEventTypeDetailsDataView,
                   net::device_bound_sessions::SessionEventTypeDetails> {
  static network::mojom::DeviceBoundSessionEventTypeDetailsDataView::Tag GetTag(
      const net::device_bound_sessions::SessionEventTypeDetails&
          event_details) {
    if (std::holds_alternative<
            net::device_bound_sessions::CreationEventDetails>(event_details)) {
      return network::mojom::DeviceBoundSessionEventTypeDetailsDataView::Tag::
          kCreation;
    }
    if (std::holds_alternative<net::device_bound_sessions::RefreshEventDetails>(
            event_details)) {
      return network::mojom::DeviceBoundSessionEventTypeDetailsDataView::Tag::
          kRefresh;
    }
    if (std::holds_alternative<
            net::device_bound_sessions::TerminationEventDetails>(
            event_details)) {
      return network::mojom::DeviceBoundSessionEventTypeDetailsDataView::Tag::
          kTermination;
    }
    if (std::holds_alternative<
            net::device_bound_sessions::ChallengeEventDetails>(event_details)) {
      return network::mojom::DeviceBoundSessionEventTypeDetailsDataView::Tag::
          kChallenge;
    }
    NOTREACHED();
  }
  // LINT.ThenChange(//net/device_bound_sessions/session_event.h:SessionEventTypeDetails)

  static const net::device_bound_sessions::CreationEventDetails& creation(
      const net::device_bound_sessions::SessionEventTypeDetails&
          event_type_details) {
    return std::get<net::device_bound_sessions::CreationEventDetails>(
        event_type_details);
  }

  static const net::device_bound_sessions::RefreshEventDetails& refresh(
      const net::device_bound_sessions::SessionEventTypeDetails&
          event_type_details) {
    return std::get<net::device_bound_sessions::RefreshEventDetails>(
        event_type_details);
  }

  static const net::device_bound_sessions::TerminationEventDetails& termination(
      const net::device_bound_sessions::SessionEventTypeDetails&
          event_type_details) {
    return std::get<net::device_bound_sessions::TerminationEventDetails>(
        event_type_details);
  }

  static const net::device_bound_sessions::ChallengeEventDetails& challenge(
      const net::device_bound_sessions::SessionEventTypeDetails&
          event_type_details) {
    return std::get<net::device_bound_sessions::ChallengeEventDetails>(
        event_type_details);
  }

  static bool Read(
      network::mojom::DeviceBoundSessionEventTypeDetailsDataView data,
      net::device_bound_sessions::SessionEventTypeDetails* out);
};

template <>
struct StructTraits<network::mojom::DeviceBoundSessionFailedRequestDataView,
                    net::device_bound_sessions::FailedRequest> {
  static const GURL& request_url(
      const net::device_bound_sessions::FailedRequest& r);

  static std::optional<int32_t> net_error(
      const net::device_bound_sessions::FailedRequest& r);

  static std::optional<int32_t> response_error(
      const net::device_bound_sessions::FailedRequest& r);

  static const std::optional<std::string>& response_error_body(
      const net::device_bound_sessions::FailedRequest& r);

  static bool Read(network::mojom::DeviceBoundSessionFailedRequestDataView data,
                   net::device_bound_sessions::FailedRequest* out);
};

template <>
struct StructTraits<network::mojom::DeviceBoundSessionCreationDetailsDataView,
                    net::device_bound_sessions::CreationEventDetails> {
  static net::device_bound_sessions::SessionError::ErrorType fetch_error(
      const net::device_bound_sessions::CreationEventDetails& event_details);

  static const std::optional<net::device_bound_sessions::SessionDisplay>&
  new_session_display(
      const net::device_bound_sessions::CreationEventDetails& obj);

  static const std::optional<net::device_bound_sessions::FailedRequest>&
  failed_request(
      const net::device_bound_sessions::CreationEventDetails& event_details);

  static bool Read(
      network::mojom::DeviceBoundSessionCreationDetailsDataView data,
      net::device_bound_sessions::CreationEventDetails* out);
};

template <>
struct StructTraits<network::mojom::DeviceBoundSessionRefreshDetailsDataView,
                    net::device_bound_sessions::RefreshEventDetails> {
  static net::device_bound_sessions::RefreshResult refresh_result(
      const net::device_bound_sessions::RefreshEventDetails& event_details);

  static const std::optional<
      net::device_bound_sessions::SessionError::ErrorType>&
  fetch_error(
      const net::device_bound_sessions::RefreshEventDetails& event_details);

  static bool was_fully_proactive_refresh(
      const net::device_bound_sessions::RefreshEventDetails& event_details);

  static const std::optional<net::device_bound_sessions::SessionDisplay>&
  new_session_display(
      const net::device_bound_sessions::RefreshEventDetails& obj);

  static const std::optional<net::device_bound_sessions::FailedRequest>&
  failed_request(
      const net::device_bound_sessions::RefreshEventDetails& event_details);

  static bool Read(
      network::mojom::DeviceBoundSessionRefreshDetailsDataView data,
      net::device_bound_sessions::RefreshEventDetails* out);
};

template <>
struct StructTraits<network::mojom::DeviceBoundSessionChallengeDetailsDataView,
                    net::device_bound_sessions::ChallengeEventDetails> {
  static net::device_bound_sessions::ChallengeResult challenge_result(
      const net::device_bound_sessions::ChallengeEventDetails& event_details);

  static const std::string& challenge(
      const net::device_bound_sessions::ChallengeEventDetails& event_details);

  static bool Read(
      network::mojom::DeviceBoundSessionChallengeDetailsDataView data,
      net::device_bound_sessions::ChallengeEventDetails* out);
};

template <>
struct StructTraits<
    network::mojom::DeviceBoundSessionTerminationDetailsDataView,
    net::device_bound_sessions::TerminationEventDetails> {
  static net::device_bound_sessions::DeletionReason deletion_reason(
      const net::device_bound_sessions::TerminationEventDetails& event_details);

  static bool Read(
      network::mojom::DeviceBoundSessionTerminationDetailsDataView data,
      net::device_bound_sessions::TerminationEventDetails* out);
};

template <>
struct StructTraits<network::mojom::DeviceBoundSessionEventDataView,
                    net::device_bound_sessions::SessionEvent> {
  static const base::UnguessableToken& event_id(
      const net::device_bound_sessions::SessionEvent& event);

  static const net::SchemefulSite& site(
      const net::device_bound_sessions::SessionEvent& event);

  static const std::optional<std::string>& session_id(
      const net::device_bound_sessions::SessionEvent& event);

  static bool succeeded(const net::device_bound_sessions::SessionEvent& event);

  static const net::device_bound_sessions::SessionEventTypeDetails&
  event_type_details(const net::device_bound_sessions::SessionEvent& event);

  static bool Read(network::mojom::DeviceBoundSessionEventDataView data,
                   net::device_bound_sessions::SessionEvent* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_DEVICE_BOUND_SESSIONS_MOJOM_TRAITS_H_
