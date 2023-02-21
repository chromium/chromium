// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_GAIA_SWITCHES_H_
#define GOOGLE_APIS_GAIA_GAIA_SWITCHES_H_

#include "base/component_export.h"

namespace switches {

// Specifies the path to a config file containing GAIA urls.
// See "google_apis/test/data/gaia/all_urls.json" for a format example.
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kGaiaConfigPath[];

// Specifies a string containing the JSON config for GAIA urls. This is
// equivalent to pointing to a file with the same content via kGaiaConfigPath.
// See "google_apis/test/data/gaia/all_urls.json" for a format example.
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kGaiaConfigContents[];

// Specifies the domain of the SAPISID cookie. The default value is
// "http://.google.com".
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kGoogleUrl[];

// Specifies the path for GAIA authentication URL. The default value is
// "https://accounts.google.com".
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kGaiaUrl[];

// Specifies the backend server used for Google API calls.
// "https://www.googleapis.com".
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kGoogleApisUrl[];

// Specifies the backend server used for lso authentication calls.
// "https://accounts.google.com".
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kLsoUrl[];

// Specifies the backend server used for OAuth issue token calls.
// "https://oauthaccountmanager.googleapis.com".
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kOAuthAccountManagerUrl[];

// Specifies custom OAuth2 client id for testing purposes.
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kOAuth2ClientID[];

// Specifies custom OAuth2 client secret for testing purposes.
COMPONENT_EXPORT(GOOGLE_APIS) extern const char kOAuth2ClientSecret[];

}  // namespace switches

#endif  // GOOGLE_APIS_GAIA_GAIA_SWITCHES_H_
