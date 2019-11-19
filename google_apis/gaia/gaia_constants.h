// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Constants used by IssueAuthToken and ClientLogin

#ifndef GOOGLE_APIS_GAIA_GAIA_CONSTANTS_H_
#define GOOGLE_APIS_GAIA_GAIA_CONSTANTS_H_

namespace GaiaConstants {

// Gaia sources for accounting
extern const char kChromeOSSource[];
extern const char kChromeSource[];
// Used as Gaia source suffix to detect retry requests because of
// |GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE|.
extern const char kUnexpectedServiceResponse[];

// Gaia services for requesting
extern const char kGaiaService[];  // uber token
extern const char kPicasaService[];
extern const char kSyncService[];
extern const char kRemotingService[];

// OAuth2 scopes.
extern const char kOAuth1LoginScope[];
extern const char kOAuthWrapBridgeUserInfoScope[];
extern const char kDeviceManagementServiceOAuth[];
extern const char kAnyApiOAuth2Scope[];
extern const char kChromeSyncOAuth2Scope[];
extern const char kChromeSyncSupervisedOAuth2Scope[];
extern const char kKidsSupervisionSetupChildOAuth2Scope[];
extern const char kGoogleTalkOAuth2Scope[];
extern const char kGoogleUserInfoEmail[];
extern const char kGoogleUserInfoProfile[];
extern const char kPeopleApiReadOnlyOAuth2Scope[];
extern const char kAccountsReauthOAuth2Scope[];
extern const char kAuditRecordingOAuth2Scope[];
extern const char kClearCutOAuth2Scope[];

// Used with uber auth tokens when needed.
extern const char kGaiaSid[];
extern const char kGaiaLsid[];
extern const char kGaiaOAuthToken[];
extern const char kGaiaOAuthSecret[];
extern const char kGaiaOAuthDuration[];
extern const char kGaiaOAuth2LoginRefreshToken[];

// Used to construct a channel ID for push messaging.
extern const char kObfuscatedGaiaId[];

// Used by wallet sign in helper.
extern const char kClientOAuthEmailKey[];

// Refresh token that is guaranteed to be invalid.
extern const char kInvalidRefreshToken[];
}  // namespace GaiaConstants

#endif  // GOOGLE_APIS_GAIA_GAIA_CONSTANTS_H_
