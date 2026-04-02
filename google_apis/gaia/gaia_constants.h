// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Google Authentication service constants.

#ifndef GOOGLE_APIS_GAIA_GAIA_CONSTANTS_H_
#define GOOGLE_APIS_GAIA_GAIA_CONSTANTS_H_

#include <string_view>

#include "base/component_export.h"

namespace GaiaConstants {

// Gaia sources for accounting
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kChromeOSSource[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kChromeSource[];
// Used as Gaia source suffix to detect retry requests because of
// |GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE|.
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kUnexpectedServiceResponse[];

// OAuth2 scopes.
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kOAuth1LoginScope[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kDeviceManagementServiceOAuth[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kAnyApiOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kChromeSyncOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS)
extern const char kChromeSyncSupervisedOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS)
extern const char kKidManagementPrivilegedOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS)
extern const char kKidsSupervisionSetupChildOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kGoogleTalkOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kGoogleUserInfoEmail[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kGoogleUserInfoProfile[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kParentApprovalOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kPeopleApiReadOnlyOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS)
extern const char kPeopleApiReadWriteOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS)
extern const char kContactsOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS)
extern const char kProfileLanguageReadOnlyOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS)
extern const char kProgrammaticChallengeOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kAccountsReauthOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kAuditRecordingOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kClearCutOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kFCMOAuthScope[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kTachyonOAuthScope[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kPhotosOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kCastBackdropOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS)
extern const char kPasswordsLeakCheckOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS)
extern const char kChromeSafeBrowsingOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS)
extern const char kClassifyUrlKidPermissionOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kKidFamilyReadonlyOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kPaymentsOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kCryptAuthOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kDriveOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kDriveAppsOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS)
extern const char kExperimentsAndConfigsOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kClientChannelOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kDriveReadOnlyOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kDriveAppsReadonlyOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kAssistantOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS)
extern const char kNearbyDevicesOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kNearbyShareOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kNearbyPresenceOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kGCMGroupServerOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kGCMCheckinServerOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kChromeWebstoreOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS)
extern const char kAccountCapabilitiesOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kSupportContentOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kPhotosModuleOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kPhotosModuleImageOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kSecureConnectOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kFeedOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kKAnonymityServiceOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kCalendarReadOnlyOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kPasskeysEnclaveOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS)
extern const char kCloudSearchQueryOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS)
extern const char kDiscoveryEngineCompleteQueryOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS)
extern const char kDiscoveryOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS)
extern const char kGeminiOAuth2Scope[];
// OAuth2 scopes for Optimization Guide.
COMPONENT_EXPORT(GOOGLE_APIS)
extern const char kOptimizationGuideServiceGetHintsOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS)
extern const char kOptimizationGuideServiceModelExecutionOAuth2Scope[];

// OAuth2 scopes for Lens.
COMPONENT_EXPORT(GOOGLE_APIS)
extern const char kLensOAuth2Scope[];

// Used by wallet sign in helper.
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kClientOAuthEmailKey[];

// Refresh token that is guaranteed to be invalid.
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kInvalidRefreshToken[];

// Name of the Google authentication cookie.
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kGaiaSigninCookieName[];

// Constants for the Chrome Refresh Token Binding.
inline constexpr std::string_view kTokenBindingAssertionSentinel =
    "DBSC_CHALLENGE_IF_REQUIRED";
inline constexpr std::string_view kTokenBindingAssertionFailedPlaceholder =
    "SIGNATURE_FAILED";

}  // namespace GaiaConstants

#endif  // GOOGLE_APIS_GAIA_GAIA_CONSTANTS_H_
