// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_DEVICE_BOUND_SESSIONS_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_DEVICE_BOUND_SESSIONS_MOJOM_TRAITS_H_

#include "net/device_bound_sessions/deletion_reason.h"
#include "net/device_bound_sessions/session_access.h"
#include "net/device_bound_sessions/session_error.h"
#include "net/device_bound_sessions/session_key.h"
#include "net/device_bound_sessions/session_params.h"
#include "services/network/public/mojom/device_bound_sessions.mojom-shared.h"

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

  static bool FromMojom(
      network::mojom::DeviceBoundSessionAccessType input,
      net::device_bound_sessions::SessionAccess::AccessType* output) {
    using enum net::device_bound_sessions::SessionAccess::AccessType;
    switch (input) {
      case network::mojom::DeviceBoundSessionAccessType::kCreation:
        *output = kCreation;
        return true;
      case network::mojom::DeviceBoundSessionAccessType::kUpdate:
        *output = kUpdate;
        return true;
      case network::mojom::DeviceBoundSessionAccessType::kTermination:
        *output = kTermination;
        return true;
    }
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
    }
  }

  static bool FromMojom(network::mojom::DeviceBoundSessionDeletionReason input,
                        net::device_bound_sessions::DeletionReason* output) {
    using enum net::device_bound_sessions::DeletionReason;
    switch (input) {
      case network::mojom::DeviceBoundSessionDeletionReason::kExpired:
        *output = kExpired;
        return true;
      case network::mojom::DeviceBoundSessionDeletionReason::
          kFailedToRestoreKey:
        *output = kFailedToRestoreKey;
        return true;
      case network::mojom::DeviceBoundSessionDeletionReason::kFailedToUnwrapKey:
        *output = kFailedToUnwrapKey;
        return true;
      case network::mojom::DeviceBoundSessionDeletionReason::
          kStoragePartitionCleared:
        *output = kStoragePartitionCleared;
        return true;
      case network::mojom::DeviceBoundSessionDeletionReason::kClearBrowsingData:
        *output = kClearBrowsingData;
        return true;
      case network::mojom::DeviceBoundSessionDeletionReason::kServerRequested:
        *output = kServerRequested;
        return true;
      case network::mojom::DeviceBoundSessionDeletionReason::
          kInvalidSessionParams:
        *output = kInvalidSessionParams;
        return true;
      case network::mojom::DeviceBoundSessionDeletionReason::kRefreshFatalError:
        *output = kRefreshFatalError;
        return true;
    }
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

  static bool FromMojom(
      network::mojom::DeviceBoundSessionScopeSpecificationType input,
      net::device_bound_sessions::SessionParams::Scope::Specification::Type*
          output) {
    using enum net::device_bound_sessions::SessionParams::Scope::Specification::
        Type;
    switch (input) {
      case network::mojom::DeviceBoundSessionScopeSpecificationType::kExclude:
        *output = kExclude;
        return true;
      case network::mojom::DeviceBoundSessionScopeSpecificationType::kInclude:
        *output = kInclude;
        return true;
    }
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
    }
  }

  static bool FromMojom(
      network::mojom::DeviceBoundSessionError input,
      net::device_bound_sessions::SessionError::ErrorType* output) {
    using enum net::device_bound_sessions::SessionError::ErrorType;
    switch (input) {
      case network::mojom::DeviceBoundSessionError::kSuccess:
        *output = kSuccess;
        return true;
      case network::mojom::DeviceBoundSessionError::kKeyError:
        *output = kKeyError;
        return true;
      case network::mojom::DeviceBoundSessionError::kSigningError:
        *output = kSigningError;
        return true;
      case network::mojom::DeviceBoundSessionError::kServerRequestedTermination:
        *output = kServerRequestedTermination;
        return true;
      case network::mojom::DeviceBoundSessionError::kInvalidSessionId:
        *output = kInvalidSessionId;
        return true;
      case network::mojom::DeviceBoundSessionError::kInvalidChallenge:
        *output = kInvalidChallenge;
        return true;
      case network::mojom::DeviceBoundSessionError::kTooManyChallenges:
        *output = kTooManyChallenges;
        return true;
      case network::mojom::DeviceBoundSessionError::kInvalidFetcherUrl:
        *output = kInvalidFetcherUrl;
        return true;
      case network::mojom::DeviceBoundSessionError::kInvalidRefreshUrl:
        *output = kInvalidRefreshUrl;
        return true;
      case network::mojom::DeviceBoundSessionError::kTransientHttpError:
        *output = kTransientHttpError;
        return true;
      case network::mojom::DeviceBoundSessionError::
          kScopeOriginSameSiteMismatch:
        *output = kScopeOriginSameSiteMismatch;
        return true;
      case network::mojom::DeviceBoundSessionError::kRefreshUrlSameSiteMismatch:
        *output = kRefreshUrlSameSiteMismatch;
        return true;
      case network::mojom::DeviceBoundSessionError::kMismatchedSessionId:
        *output = kMismatchedSessionId;
        return true;
      case network::mojom::DeviceBoundSessionError::kMissingScope:
        *output = kMissingScope;
        return true;
      case network::mojom::DeviceBoundSessionError::kNoCredentials:
        *output = kNoCredentials;
        return true;
      case network::mojom::DeviceBoundSessionError::
          kSubdomainRegistrationWellKnownUnavailable:
        *output = kSubdomainRegistrationWellKnownUnavailable;
        return true;
      case network::mojom::DeviceBoundSessionError::
          kSubdomainRegistrationUnauthorized:
        *output = kSubdomainRegistrationUnauthorized;
        return true;
      case network::mojom::DeviceBoundSessionError::
          kSubdomainRegistrationWellKnownMalformed:
        *output = kSubdomainRegistrationWellKnownMalformed;
        return true;
      case network::mojom::DeviceBoundSessionError::
          kSessionProviderWellKnownUnavailable:
        *output = kSessionProviderWellKnownUnavailable;
        return true;
      case network::mojom::DeviceBoundSessionError::
          kRelyingPartyWellKnownUnavailable:
        *output = kRelyingPartyWellKnownUnavailable;
        return true;
      case network::mojom::DeviceBoundSessionError::
          kFederatedKeyThumbprintMismatch:
        *output = kFederatedKeyThumbprintMismatch;
        return true;
      case network::mojom::DeviceBoundSessionError::kInvalidFederatedSessionUrl:
        *output = kInvalidFederatedSessionUrl;
        return true;
      case network::mojom::DeviceBoundSessionError::kInvalidFederatedKey:
        *output = kInvalidFederatedKey;
        return true;
      case network::mojom::DeviceBoundSessionError::kTooManyRelyingOriginLabels:
        *output = kTooManyRelyingOriginLabels;
        return true;
      case network::mojom::DeviceBoundSessionError::kBoundCookieSetForbidden:
        *output = kBoundCookieSetForbidden;
        return true;
      case network::mojom::DeviceBoundSessionError::kNetError:
        *output = kNetError;
        return true;
      case network::mojom::DeviceBoundSessionError::kProxyError:
        *output = kProxyError;
        return true;
      case network::mojom::DeviceBoundSessionError::kEmptySessionConfig:
        *output = kEmptySessionConfig;
        return true;
      case network::mojom::DeviceBoundSessionError::kInvalidCredentialsConfig:
        *output = kInvalidCredentialsConfig;
        return true;
      case network::mojom::DeviceBoundSessionError::kInvalidCredentialsType:
        *output = kInvalidCredentialsType;
        return true;
      case network::mojom::DeviceBoundSessionError::
          kInvalidCredentialsEmptyName:
        *output = kInvalidCredentialsEmptyName;
        return true;
      case network::mojom::DeviceBoundSessionError::kInvalidCredentialsCookie:
        *output = kInvalidCredentialsCookie;
        return true;
      case network::mojom::DeviceBoundSessionError::kPersistentHttpError:
        *output = kPersistentHttpError;
        return true;
      case network::mojom::DeviceBoundSessionError::
          kRegistrationAttemptedChallenge:
        *output = kRegistrationAttemptedChallenge;
        return true;
      case network::mojom::DeviceBoundSessionError::kInvalidScopeOrigin:
        *output = kInvalidScopeOrigin;
        return true;
      case network::mojom::DeviceBoundSessionError::kScopeOriginContainsPath:
        *output = kScopeOriginContainsPath;
        return true;
      case network::mojom::DeviceBoundSessionError::kRefreshInitiatorNotString:
        *output = kRefreshInitiatorNotString;
        return true;
      case network::mojom::DeviceBoundSessionError::
          kRefreshInitiatorInvalidHostPattern:
        *output = kRefreshInitiatorInvalidHostPattern;
        return true;
      case network::mojom::DeviceBoundSessionError::kInvalidScopeSpecification:
        *output = kInvalidScopeSpecification;
        return true;
      case network::mojom::DeviceBoundSessionError::
          kMissingScopeSpecificationType:
        *output = kMissingScopeSpecificationType;
        return true;
      case network::mojom::DeviceBoundSessionError::
          kEmptyScopeSpecificationDomain:
        *output = kEmptyScopeSpecificationDomain;
        return true;
      case network::mojom::DeviceBoundSessionError::
          kEmptyScopeSpecificationPath:
        *output = kEmptyScopeSpecificationPath;
        return true;
      case network::mojom::DeviceBoundSessionError::
          kInvalidScopeSpecificationType:
        *output = kInvalidScopeSpecificationType;
        return true;
      case network::mojom::DeviceBoundSessionError::kInvalidScopeIncludeSite:
        *output = kInvalidScopeIncludeSite;
        return true;
      case network::mojom::DeviceBoundSessionError::kMissingScopeIncludeSite:
        *output = kMissingScopeIncludeSite;
        return true;
      case network::mojom::DeviceBoundSessionError::
          kFederatedNotAuthorizedByProvider:
        *output = kFederatedNotAuthorizedByProvider;
        return true;
      case network::mojom::DeviceBoundSessionError::
          kFederatedNotAuthorizedByRelyingParty:
        *output = kFederatedNotAuthorizedByRelyingParty;
        return true;
      case network::mojom::DeviceBoundSessionError::
          kSessionProviderWellKnownMalformed:
        *output = kSessionProviderWellKnownMalformed;
        return true;
      case network::mojom::DeviceBoundSessionError::
          kSessionProviderWellKnownHasProviderOrigin:
        *output = kSessionProviderWellKnownHasProviderOrigin;
        return true;
      case network::mojom::DeviceBoundSessionError::
          kRelyingPartyWellKnownMalformed:
        *output = kRelyingPartyWellKnownMalformed;
        return true;
      case network::mojom::DeviceBoundSessionError::
          kRelyingPartyWellKnownHasRelyingOrigins:
        *output = kRelyingPartyWellKnownHasRelyingOrigins;
        return true;
      case network::mojom::DeviceBoundSessionError::
          kInvalidFederatedSessionProviderSessionMissing:
        *output = kInvalidFederatedSessionProviderSessionMissing;
        return true;
      case network::mojom::DeviceBoundSessionError::
          kInvalidFederatedSessionWrongProviderOrigin:
        *output = kInvalidFederatedSessionWrongProviderOrigin;
        return true;
      case network::mojom::DeviceBoundSessionError::
          kInvalidCredentialsCookieCreationTime:
        *output = kInvalidCredentialsCookieCreationTime;
        return true;
      case network::mojom::DeviceBoundSessionError::
          kInvalidCredentialsCookieName:
        *output = kInvalidCredentialsCookieName;
        return true;
      case network::mojom::DeviceBoundSessionError::
          kInvalidCredentialsCookieParsing:
        *output = kInvalidCredentialsCookieParsing;
        return true;
      case network::mojom::DeviceBoundSessionError::
          kInvalidCredentialsCookieUnpermittedAttribute:
        *output = kInvalidCredentialsCookieUnpermittedAttribute;
        return true;
      case network::mojom::DeviceBoundSessionError::
          kInvalidCredentialsCookieInvalidDomain:
        *output = kInvalidCredentialsCookieInvalidDomain;
        return true;
      case network::mojom::DeviceBoundSessionError::
          kInvalidCredentialsCookiePrefix:
        *output = kInvalidCredentialsCookiePrefix;
        return true;
      case network::mojom::DeviceBoundSessionError::kInvalidScopeRulePath:
        *output = kInvalidScopeRulePath;
        return true;
      case network::mojom::DeviceBoundSessionError::
          kInvalidScopeRuleHostPattern:
        *output = kInvalidScopeRuleHostPattern;
        return true;
      case network::mojom::DeviceBoundSessionError::
          kScopeRuleOriginScopedHostPatternMismatch:
        *output = kScopeRuleOriginScopedHostPatternMismatch;
        return true;
      case network::mojom::DeviceBoundSessionError::
          kScopeRuleSiteScopedHostPatternMismatch:
        *output = kScopeRuleSiteScopedHostPatternMismatch;
        return true;
      case network::mojom::DeviceBoundSessionError::kSigningQuotaExceeded:
        *output = kSigningQuotaExceeded;
        return true;
      case network::mojom::DeviceBoundSessionError::kInvalidConfigJson:
        *output = kInvalidConfigJson;
        return true;
      case network::mojom::DeviceBoundSessionError::
          kInvalidFederatedSessionProviderFailedToRestoreKey:
        *output = kInvalidFederatedSessionProviderFailedToRestoreKey;
        return true;
      case network::mojom::DeviceBoundSessionError::kFailedToUnwrapKey:
        *output = kFailedToUnwrapKey;
        return true;
    }
  }
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

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_DEVICE_BOUND_SESSIONS_MOJOM_TRAITS_H_
