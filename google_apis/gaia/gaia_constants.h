// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Google Authentication service constants.

#ifndef GOOGLE_APIS_GAIA_GAIA_CONSTANTS_H_
#define GOOGLE_APIS_GAIA_GAIA_CONSTANTS_H_

namespace GaiaConstants {

// Gaia sources for accounting
extern const char kChromeOSSource[];
extern const char kChromeSource[];
// Used as Gaia source suffix to detect retry requests because of
// |GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE|.
extern const char kUnexpectedServiceResponse[];

// OAuth2 scopes.
extern const char kOAuth1LoginScope[];
extern const char kDeviceManagementServiceOAuth[];
extern const char kAnyApiOAuth2Scope[];
extern const char kChromeSyncOAuth2Scope[];
extern const char kChromeSyncSupervisedOAuth2Scope[];
extern const char kKidManagementPrivilegedOAuth2Scope[];
extern const char kKidsSupervisionSetupChildOAuth2Scope[];
extern const char kGoogleTalkOAuth2Scope[];
extern const char kGoogleUserInfoEmail[];
extern const char kGoogleUserInfoProfile[];
extern const char kParentApprovalOAuth2Scope[];
extern const char kPeopleApiReadOnlyOAuth2Scope[];
extern const char kProgrammaticChallengeOAuth2Scope[];
extern const char kAccountsReauthOAuth2Scope[];
extern const char kAuditRecordingOAuth2Scope[];
extern const char kClearCutOAuth2Scope[];
extern const char kFCMOAuthScope[];
extern const char kTachyonOAuthScope[];
extern const char kPhotosOAuth2Scope[];
extern const char kCastBackdropOAuth2Scope[];
extern const char kCloudTranslationOAuth2Scope[];
extern const char kPasswordsLeakCheckOAuth2Scope[];
extern const char kChromeSafeBrowsingOAuth2Scope[];
extern const char kClassifyUrlKidPermissionOAuth2Scope[];
extern const char kKidFamilyReadonlyOAuth2Scope[];
extern const char kPaymentsOAuth2Scope[];
extern const char kCryptAuthOAuth2Scope[];
extern const char kDriveOAuth2Scope[];
extern const char kDriveReadOnlyOAuth2Scope[];
extern const char kAssistantOAuth2Scope[];
extern const char kCloudPlatformProjectsOAuth2Scope[];
extern const char kNearbyShareOAuth2Scope[];
extern const char kGCMGroupServerOAuth2Scope[];
extern const char kGCMCheckinServerOAuth2Scope[];
extern const char kChromeWebstoreOAuth2Scope[];
extern const char kAccountCapabilitiesOAuth2Scope[];
extern const char kSupportContentOAuth2Scope[];
extern const char kPhotosModuleOAuth2Scope[];
extern const char kPhotosModuleImageOAuth2Scope[];
extern const char kSecureConnectOAuth2Scope[];
extern const char kFeedOAuth2Scope[];
extern const char kKAnonymityServiceOAuth2Scope[];
extern const char kCalendarReadOnlyOAuth2Scope[];

// Used by wallet sign in helper.
extern const char kClientOAuthEmailKey[];

// Refresh token that is guaranteed to be invalid.
extern const char kInvalidRefreshToken[];

// Name of the Google authentication cookie.
extern const char kGaiaSigninCookieName[];
}  // namespace GaiaConstants

#endif  // GOOGLE_APIS_GAIA_GAIA_CONSTANTS_H_
