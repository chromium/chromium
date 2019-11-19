// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GOOGLE_API_KEYS_H_
#define GOOGLE_APIS_GOOGLE_API_KEYS_H_

// If you add more includes to this file, you also need to add them to
// google_api_keys_unittest.cc.
#include <string>

// These functions enable you to retrieve keys to use for Google APIs
// such as Translate and Safe Browsing.
//
// You can retrieve either an "API key" (sometimes called a developer
// key) which identifies you (or the company you work for) as a
// developer, or you can retrieve the "client ID" and "client secret"
// used by you (or the company you work for) to generate OAuth2
// requests.
//
// Each developer (or group of developers working together for a
// single company) must request a Google API key and the client ID and
// client secret for OAuth2.  See
// https://developers.google.com/console/help/ and
// https://developers.google.com/console/.
//
// The keys must either be provided using preprocessor variables (set
// via e.g. ~/.gyp/include.gypi). Alternatively, in Chromium builds, they can be
// overridden at runtime using environment variables of the same name.
// Environment variable overrides will be ignored for official Google Chrome
// builds.
//
// The names of the preprocessor variables (or environment variables
// to override them at runtime in Chromium builds) are as follows:
// - GOOGLE_API_KEY: The API key, a.k.a. developer key.
// - GOOGLE_DEFAULT_CLIENT_ID: If set, this is used as the default for
//   all client IDs not otherwise set.  This is intended only for
//   development.
// - GOOGLE_DEFAULT_CLIENT_SECRET: If set, this is used as the default
//   for all client secrets.  This is intended only for development.
// - GOOGLE_CLIENT_ID_[client name]
//   (e.g. GOOGLE_CLIENT_ID_CLOUD_PRINT, i.e. one for each item in the
//   OAuth2Client enumeration below)
// - GOOGLE_CLIENT_SECRET_[client name]
//   (e.g. GOOGLE_CLIENT_SECRET_CLOUD_PRINT, i.e. one for each item in
//   the OAuth2Client enumeration below)
//
// The GOOGLE_CLIENT_ID_MAIN and GOOGLE_CLIENT_SECRET_MAIN values can
// also be set via the command line (this overrides any other
// setting).  The command-line parameters are --oauth2-client-id and
// --oauth2-client-secret.
//
// If some of the parameters mentioned above are not provided,
// Chromium will still build and run, but services that require them
// may fail to work without warning.  They should do so gracefully,
// similar to what would happen when a network connection is
// unavailable.

namespace google_apis {

extern const char kAPIKeysDevelopersHowToURL[];

// Returns true if no dummy API key is set.
bool HasAPIKeyConfigured();

// Retrieves the API key, a.k.a. developer key, or a dummy string
// if not set.
//
// Note that the key should be escaped for the context you use it in,
// e.g. URL-escaped if you use it in a URL.
std::string GetAPIKey();

// Non-stable channels may have a different Google API key.
std::string GetNonStableAPIKey();

// Retrieves the Chrome Remote Desktop API key.
std::string GetRemotingAPIKey();

#if defined(OS_IOS)
// Sets the API key. This should be called as early as possible before this
// API key is even accessed.
void SetAPIKey(const std::string& api_key);
#endif

// Retrieves the key used to sign metrics (UMA/UKM) uploads.
std::string GetMetricsKey();

// Represents the different sets of client IDs and secrets in use.
enum OAuth2Client {
  CLIENT_MAIN,         // Several different features use this.
  CLIENT_CLOUD_PRINT,
  CLIENT_REMOTING,
  CLIENT_REMOTING_HOST,

  CLIENT_NUM_ITEMS     // Must be last item.
};

// Returns true if no dummy OAuth2 client ID and secret are set.
bool HasOAuthClientConfigured();

// Retrieves the OAuth2 client ID for the specified client, or the
// empty string if not set.
//
// Note that the ID should be escaped for the context you use it in,
// e.g. URL-escaped if you use it in a URL.
std::string GetOAuth2ClientID(OAuth2Client client);

// Retrieves the OAuth2 client secret for the specified client, or the
// empty string if not set.
//
// Note that the secret should be escaped for the context you use it
// in, e.g. URL-escaped if you use it in a URL.
std::string GetOAuth2ClientSecret(OAuth2Client client);

#if defined(OS_IOS)
// Sets the client id for the specified client. Should be called as early as
// possible before these ids are accessed.
void SetOAuth2ClientID(OAuth2Client client, const std::string& client_id);

// Sets the client secret for the specified client. Should be called as early as
// possible before these secrets are accessed.
void SetOAuth2ClientSecret(OAuth2Client client,
                           const std::string& client_secret);
#endif
// Returns the auth token for the data reduction proxy.
std::string GetSpdyProxyAuthValue();

// Returns if the API key using in the current build is the one for official
// Google Chrome.
bool IsGoogleChromeAPIKeyUsed();

}  // namespace google_apis

#endif  // GOOGLE_APIS_GOOGLE_API_KEYS_H_
