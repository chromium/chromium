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
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kGoogleUserInfoEmail[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kGoogleUserInfoProfile[];
COMPONENT_EXPORT(GOOGLE_APIS)
extern const char kContactsOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kFCMOAuthScope[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kTachyonOAuthScope[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kDriveOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kDriveReadOnlyOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS)
extern const char kAccountCapabilitiesOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kSupportContentOAuth2Scope[];
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kSecureConnectOAuth2Scope[];

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
