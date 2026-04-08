// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Constants definitions

#include "google_apis/gaia/gaia_constants.h"

namespace GaiaConstants {

// Gaia uses this for accounting where login is coming from.
const char kChromeOSSource[] = "chromeos";
const char kChromeSource[] = "ChromiumBrowser";
const char kUnexpectedServiceResponse[] = "UnexpectedServiceResponse";

// OAuth scopes.
const char kOAuth1LoginScope[] = "https://www.google.com/accounts/OAuthLogin";

// Service/scope names for device management (cloud-based policy) server.
const char kDeviceManagementServiceOAuth[] =
    "https://www.googleapis.com/auth/chromeosdevicemanagement";

// OAuth2 scope for access to all Google APIs.
const char kAnyApiOAuth2Scope[] = "https://www.googleapis.com/auth/any-api";

// OAuth2 scope for access to Chrome sync APIs
const char kChromeSyncOAuth2Scope[] =
    "https://www.googleapis.com/auth/chromesync";

// OAuth2 scope for access to Google account information.
const char kGoogleUserInfoEmail[] =
    "https://www.googleapis.com/auth/userinfo.email";
const char kGoogleUserInfoProfile[] =
    "https://www.googleapis.com/auth/userinfo.profile";

// OAuth2 scope for read-write access to contacts.
const char kContactsOAuth2Scope[] = "https://www.googleapis.com/auth/contacts";

// OAuth2 scope for FCM, the Firebase Cloud Messaging service.
const char kFCMOAuthScope[] =
    "https://www.googleapis.com/auth/firebase.messaging";

// OAuth2 scope for access to Tachyon api.
const char kTachyonOAuthScope[] = "https://www.googleapis.com/auth/tachyon";

// OAuth2 scope for access to the SecureConnect API.
extern const char kSecureConnectOAuth2Scope[] =
    "https://www.googleapis.com/auth/bce.secureconnect";

// OAuth2 scope for access to Drive.
const char kDriveOAuth2Scope[] = "https://www.googleapis.com/auth/drive";

// The scope required for an access token in order to query ItemSuggest.
const char kDriveReadOnlyOAuth2Scope[] =
    "https://www.googleapis.com/auth/drive.readonly";

// OAuth2 scope for access to Account Capabilities API.
const char kAccountCapabilitiesOAuth2Scope[] =
    "https://www.googleapis.com/auth/account.capabilities";

// OAuth2 scope for support content API.
const char kSupportContentOAuth2Scope[] =
    "https://www.googleapis.com/auth/supportcontent";

// OAuth2 scopes for Lens.
const char kLensOAuth2Scope[] = "https://www.googleapis.com/auth/lens";

// Used to build ClientOAuth requests.  These are the names of keys used when
// building base::DictionaryValue that represent the json data that makes up
// the ClientOAuth endpoint protocol.  The comment above each constant explains
// what value is associated with that key.

// Canonical email of the account to sign in.
const char kClientOAuthEmailKey[] = "email";

// Used as an Invalid refresh token.
const char kInvalidRefreshToken[] = "invalid_refresh_token";

// Name of the Google authentication cookie.
const char kGaiaSigninCookieName[] = "SAPISID";

}  // namespace GaiaConstants
