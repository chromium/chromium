// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GOOGLE_API_KEYS_H_
#define GOOGLE_APIS_GOOGLE_API_KEYS_H_

#include <string>

#include "base/component_export.h"
#include "base/functional/callback_helpers.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "google_apis/buildflags.h"

namespace version_info {
enum class Channel;
}

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
// The keys must either be provided using preprocessor variables (set via e.g.
// GN args). Alternatively, they can be overridden at runtime using one of the
// following methods (in priority order):
// - Command line parameters (only for GOOGLE_CLIENT_ID_MAIN and
//   GOOGLE_CLIENT_SECRET_MAIN values). The command-line parameters are
//   --oauth2-client-id and --oauth2-client-secret.
// - Config file entry of the same name. Path to a config file is set via the
//   --gaia-config command line parameter. See google_apis/gaia/gaia_config.h
//   for syntax reference.
// - Environment variables of the same name. Environment variable overrides will
//   be ignored for official Google Chrome builds.
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
// - GOOGLE_CLIENT_SECRET_[client name]
//   (e.g. GOOGLE_CLIENT_SECRET_REMOTING, i.e. one for each item in
//   the OAuth2Client enumeration below)
//
// If some of the parameters mentioned above are not provided,
// Chromium will still build and run, but services that require them
// may fail to work without warning.  They should do so gracefully,
// similar to what would happen when a network connection is
// unavailable.

namespace google_apis {

class ApiKeyCache;

COMPONENT_EXPORT(GOOGLE_APIS) extern const char kAPIKeysDevelopersHowToURL[];

// Returns true if no dummy API key is set.
COMPONENT_EXPORT(GOOGLE_APIS) bool HasAPIKeyConfigured();

// Retrieves the API key, a.k.a. developer key, or a dummy string
// if not set.
//
// Note that the key should be escaped for the context you use it in,
// e.g. URL-escaped if you use it in a URL.
//
// If you want to attach the key to a network request, consider using
// `AddDefaultAPIKeyToRequest()` rather than calling this method and manually
// adding the key.
COMPONENT_EXPORT(GOOGLE_APIS)
const std::string& GetAPIKey(version_info::Channel channel);

// Retrieves the API key, for the stable channel.
//
// DEPRECATED: Use `GetAPIKey(channel)` to get the right key for your
// distribution channel instead of calling this function directly.
COMPONENT_EXPORT(GOOGLE_APIS) const std::string& GetAPIKey();

// Retrieves the Chrome Remote Desktop API key.
COMPONENT_EXPORT(GOOGLE_APIS) const std::string& GetRemotingAPIKey();

// Retrieves the Speech On-Device API (SODA) API Key.
COMPONENT_EXPORT(GOOGLE_APIS) const std::string& GetSodaAPIKey();

#if !BUILDFLAG(IS_ANDROID)
// Retrieves the HaTS API Key. This key is only used for desktop HaTS
// and the internal API Key is only defined in non-Android builds.
COMPONENT_EXPORT(GOOGLE_APIS) const std::string& GetHatsAPIKey();
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Retrieves the Sharing API Key.
COMPONENT_EXPORT(GOOGLE_APIS) const std::string& GetSharingAPIKey();

// Retrieves the ReadAloud API Key.
COMPONENT_EXPORT(GOOGLE_APIS) const std::string& GetReadAloudAPIKey();

// Retrieves the Fresnel API Key.
COMPONENT_EXPORT(GOOGLE_APIS) const std::string& GetFresnelAPIKey();

// Retrieves the Boca API Key.
COMPONENT_EXPORT(GOOGLE_APIS) const std::string& GetBocaAPIKey();
#endif

#if BUILDFLAG(SUPPORT_EXTERNAL_GOOGLE_API_KEY)
// Sets the API key.
//
// Note: This function must be called before any call to GetAPIKey().
COMPONENT_EXPORT(GOOGLE_APIS)
void SetAPIKey(const std::string& api_key);
#endif

// Retrieves the key used to sign metrics (UMA/UKM) uploads.
COMPONENT_EXPORT(GOOGLE_APIS) const std::string& GetMetricsKey();

// Represents the different sets of client IDs and secrets in use.
enum OAuth2Client {
  CLIENT_MAIN,  // Several different features use this.
  CLIENT_REMOTING,
  CLIENT_REMOTING_HOST,

  CLIENT_NUM_ITEMS  // Must be last item.
};

// Returns true if no dummy OAuth2 client ID and secret are set.
COMPONENT_EXPORT(GOOGLE_APIS) bool HasOAuthClientConfigured();

// Retrieves the OAuth2 client ID for the specified client, or the
// empty string if not set.
//
// Note that the ID should be escaped for the context you use it in,
// e.g. URL-escaped if you use it in a URL.
COMPONENT_EXPORT(GOOGLE_APIS)
const std::string& GetOAuth2ClientID(OAuth2Client client);

// Retrieves the OAuth2 client secret for the specified client, or the
// empty string if not set.
//
// Note that the secret should be escaped for the context you use it
// in, e.g. URL-escaped if you use it in a URL.
COMPONENT_EXPORT(GOOGLE_APIS)
const std::string& GetOAuth2ClientSecret(OAuth2Client client);

#if BUILDFLAG(IS_IOS)
// Sets the client id for the specified client. Should be called as early as
// possible before these ids are accessed.
COMPONENT_EXPORT(GOOGLE_APIS)
void SetOAuth2ClientID(OAuth2Client client, const std::string& client_id);

// Sets the client secret for the specified client. Should be called as early as
// possible before these secrets are accessed.
COMPONENT_EXPORT(GOOGLE_APIS)
void SetOAuth2ClientSecret(OAuth2Client client,
                           const std::string& client_secret);
#endif

// Returns if the API key using in the current build is the one for official
// Google Chrome.
COMPONENT_EXPORT(GOOGLE_APIS) bool IsGoogleChromeAPIKeyUsed();

// Sets a testing global instance of `ApiKeyCache` and returns a scoped object
// that will restore the previous value once destroyed.
COMPONENT_EXPORT(GOOGLE_APIS)
base::ScopedClosureRunner
    SetScopedApiKeyCacheForTesting(ApiKeyCache* api_key_cache);

}  // namespace google_apis

#endif  // GOOGLE_APIS_GOOGLE_API_KEYS_H_
