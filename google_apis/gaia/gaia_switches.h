// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_GAIA_SWITCHES_H_
#define GOOGLE_APIS_GAIA_GAIA_SWITCHES_H_

namespace switches {

// Specifies the domain of the SAPISID cookie. The default value is
// "http://.google.com".
extern const char kGoogleUrl[];

// Specifies the path for GAIA authentication URL. The default value is
// "https://accounts.google.com".
extern const char kGaiaUrl[];

// Specifies the backend server used for Google API calls.
// "https://www.googleapis.com".
extern const char kGoogleApisUrl[];

// Specifies the backend server used for lso authentication calls.
// "https://accounts.google.com".
extern const char kLsoUrl[];

// Specifies the backend server used for OAuth issue token calls.
// "https://oauthaccountmanager.googleapis.com".
extern const char kOAuthAccountManagerUrl[];

// Specifies custom OAuth2 client id for testing purposes.
extern const char kOAuth2ClientID[];

// Specifies custom OAuth2 client secret for testing purposes.
extern const char kOAuth2ClientSecret[];

}  // namespace switches

#endif  // GOOGLE_APIS_GAIA_GAIA_SWITCHES_H_
