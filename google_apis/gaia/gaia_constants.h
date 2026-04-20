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
inline constexpr char kChromeOSSource[] = "chromeos";
inline constexpr char kChromeSource[] = "ChromiumBrowser";
// Used as Gaia source suffix to detect retry requests because of
// |GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE|.
inline constexpr char kUnexpectedServiceResponse[] =
    "UnexpectedServiceResponse";

// OAuth2 scopes.
inline constexpr char kOAuth1LoginScope[] =
    "https://www.google.com/accounts/OAuthLogin";

// Service/scope names for device management (cloud-based policy) server.
inline constexpr char kDeviceManagementServiceOAuth[] =
    "https://www.googleapis.com/auth/chromeosdevicemanagement";

// OAuth2 scope for access to all Google APIs.
inline constexpr char kAnyApiOAuth2Scope[] =
    "https://www.googleapis.com/auth/any-api";

// OAuth2 scope for access to Chrome sync APIs
inline constexpr char kChromeSyncOAuth2Scope[] =
    "https://www.googleapis.com/auth/chromesync";

// OAuth2 scope for access to Google account information.
inline constexpr char kGoogleUserInfoEmail[] =
    "https://www.googleapis.com/auth/userinfo.email";
inline constexpr char kGoogleUserInfoProfile[] =
    "https://www.googleapis.com/auth/userinfo.profile";

// OAuth2 scope for FCM, the Firebase Cloud Messaging service.
inline constexpr char kFCMOAuthScope[] =
    "https://www.googleapis.com/auth/firebase.messaging";

// OAuth2 scope for access to Drive.
inline constexpr char kDriveOAuth2Scope[] =
    "https://www.googleapis.com/auth/drive";

// OAuth2 scope for access to Account Capabilities API.
inline constexpr char kAccountCapabilitiesOAuth2Scope[] =
    "https://www.googleapis.com/auth/account.capabilities";

// OAuth2 scope for support content API.
inline constexpr char kSupportContentOAuth2Scope[] =
    "https://www.googleapis.com/auth/supportcontent";

// OAuth2 scope for access to the SecureConnect API.
inline constexpr char kSecureConnectOAuth2Scope[] =
    "https://www.googleapis.com/auth/bce.secureconnect";

// Used to build ClientOAuth requests.  These are the names of keys used when
// building base::DictionaryValue that represent the json data that makes up
// the ClientOAuth endpoint protocol.  The comment above each constant explains
// what value is associated with that key.
//
// Canonical email of the account to sign in.
inline constexpr char kClientOAuthEmailKey[] = "email";

// Refresh token that is guaranteed to be invalid.
inline constexpr char kInvalidRefreshToken[] = "invalid_refresh_token";

// Name of the Google authentication cookie.
inline constexpr char kGaiaSigninCookieName[] = "SAPISID";

// Constants for the Chrome Refresh Token Binding.
inline constexpr std::string_view kTokenBindingAssertionSentinel =
    "DBSC_CHALLENGE_IF_REQUIRED";
inline constexpr std::string_view kTokenBindingAssertionFailedPlaceholder =
    "SIGNATURE_FAILED";

}  // namespace GaiaConstants

#endif  // GOOGLE_APIS_GAIA_GAIA_CONSTANTS_H_
