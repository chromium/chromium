// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/google_api_keys.h"

// If you add more includes to this list, you also need to add them to
// google_api_keys_unittest.cc.

#include <stddef.h>

#include <memory>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/stringize_macros.h"
#include "build/branding_buildflags.h"
#include "google_apis/gaia/gaia_switches.h"

#if defined(OS_MACOSX)
#include "google_apis/google_api_keys_mac.h"
#endif

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) || defined(USE_OFFICIAL_GOOGLE_API_KEYS)
#include "google_apis/internal/google_chrome_api_keys.h"
#include "google_apis/internal/metrics_signing_key.h"
#endif

// Used to indicate an unset key/id/secret.  This works better with
// various unit tests than leaving the token empty.
#define DUMMY_API_TOKEN "dummytoken"

#if !defined(GOOGLE_API_KEY)
#define GOOGLE_API_KEY DUMMY_API_TOKEN
#endif

#if !defined(GOOGLE_METRICS_SIGNING_KEY)
#define GOOGLE_METRICS_SIGNING_KEY DUMMY_API_TOKEN
#endif

#if !defined(GOOGLE_CLIENT_ID_MAIN)
#define GOOGLE_CLIENT_ID_MAIN DUMMY_API_TOKEN
#endif

#if !defined(GOOGLE_CLIENT_SECRET_MAIN)
#define GOOGLE_CLIENT_SECRET_MAIN DUMMY_API_TOKEN
#endif

#if !defined(GOOGLE_CLIENT_ID_CLOUD_PRINT)
#define GOOGLE_CLIENT_ID_CLOUD_PRINT DUMMY_API_TOKEN
#endif

#if !defined(GOOGLE_CLIENT_SECRET_CLOUD_PRINT)
#define GOOGLE_CLIENT_SECRET_CLOUD_PRINT DUMMY_API_TOKEN
#endif

#if !defined(GOOGLE_CLIENT_ID_REMOTING)
#define GOOGLE_CLIENT_ID_REMOTING DUMMY_API_TOKEN
#endif

#if !defined(GOOGLE_CLIENT_SECRET_REMOTING)
#define GOOGLE_CLIENT_SECRET_REMOTING DUMMY_API_TOKEN
#endif

#if !defined(GOOGLE_CLIENT_ID_REMOTING_HOST)
#define GOOGLE_CLIENT_ID_REMOTING_HOST DUMMY_API_TOKEN
#endif

#if !defined(GOOGLE_CLIENT_SECRET_REMOTING_HOST)
#define GOOGLE_CLIENT_SECRET_REMOTING_HOST DUMMY_API_TOKEN
#endif

// This is really the API key for non-stable channels on Android. It's
// named after the first feature that used it.
// TODO(jkrcal,rogerta): Rename this to GOOGLE_API_KEY_ANDROID_NON_STABLE.
#if !defined(GOOGLE_API_KEY_PHYSICAL_WEB_TEST)
#define GOOGLE_API_KEY_PHYSICAL_WEB_TEST DUMMY_API_TOKEN
#endif

#if !defined(GOOGLE_API_KEY_REMOTING)
#define GOOGLE_API_KEY_REMOTING DUMMY_API_TOKEN
#endif

// These are used as shortcuts for developers and users providing
// OAuth credentials via preprocessor defines or environment
// variables.  If set, they will be used to replace any of the client
// IDs and secrets above that have not been set (and only those; they
// will not override already-set values).
#if !defined(GOOGLE_DEFAULT_CLIENT_ID)
#define GOOGLE_DEFAULT_CLIENT_ID ""
#endif
#if !defined(GOOGLE_DEFAULT_CLIENT_SECRET)
#define GOOGLE_DEFAULT_CLIENT_SECRET ""
#endif

namespace google_apis {

const char kAPIKeysDevelopersHowToURL[] =
    "https://www.chromium.org/developers/how-tos/api-keys";

// This is used as a lazy instance to determine keys once and cache them.
class APIKeyCache {
 public:
  APIKeyCache() {
    std::unique_ptr<base::Environment> environment(base::Environment::Create());
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

    api_key_ = CalculateKeyValue(
        GOOGLE_API_KEY, STRINGIZE_NO_EXPANSION(GOOGLE_API_KEY), nullptr,
        std::string(), environment.get(), command_line);

// A special non-stable key is at the moment defined only for Android Chrome.
#if defined(OS_ANDROID)
    api_key_non_stable_ = CalculateKeyValue(
            GOOGLE_API_KEY_PHYSICAL_WEB_TEST,
            STRINGIZE_NO_EXPANSION(GOOGLE_API_KEY_PHYSICAL_WEB_TEST), NULL,
            std::string(), environment.get(), command_line);
#else
    api_key_non_stable_ = api_key_;
#endif

    api_key_remoting_ = CalculateKeyValue(
        GOOGLE_API_KEY_REMOTING,
        STRINGIZE_NO_EXPANSION(GOOGLE_API_KEY_REMOTING), nullptr, std::string(),
        environment.get(), command_line);

    metrics_key_ = CalculateKeyValue(
        GOOGLE_METRICS_SIGNING_KEY,
        STRINGIZE_NO_EXPANSION(GOOGLE_METRICS_SIGNING_KEY), nullptr,
        std::string(), environment.get(), command_line);

    std::string default_client_id = CalculateKeyValue(
        GOOGLE_DEFAULT_CLIENT_ID,
        STRINGIZE_NO_EXPANSION(GOOGLE_DEFAULT_CLIENT_ID), nullptr,
        std::string(), environment.get(), command_line);
    std::string default_client_secret = CalculateKeyValue(
        GOOGLE_DEFAULT_CLIENT_SECRET,
        STRINGIZE_NO_EXPANSION(GOOGLE_DEFAULT_CLIENT_SECRET), nullptr,
        std::string(), environment.get(), command_line);

    // We currently only allow overriding the baked-in values for the
    // default OAuth2 client ID and secret using a command-line
    // argument, since that is useful to enable testing against
    // staging servers, and since that was what was possible and
    // likely practiced by the QA team before this implementation was
    // written.
    client_ids_[CLIENT_MAIN] = CalculateKeyValue(
        GOOGLE_CLIENT_ID_MAIN,
        STRINGIZE_NO_EXPANSION(GOOGLE_CLIENT_ID_MAIN),
        switches::kOAuth2ClientID,
        default_client_id,
        environment.get(),
        command_line);
    client_secrets_[CLIENT_MAIN] = CalculateKeyValue(
        GOOGLE_CLIENT_SECRET_MAIN,
        STRINGIZE_NO_EXPANSION(GOOGLE_CLIENT_SECRET_MAIN),
        switches::kOAuth2ClientSecret,
        default_client_secret,
        environment.get(),
        command_line);

    client_ids_[CLIENT_CLOUD_PRINT] = CalculateKeyValue(
        GOOGLE_CLIENT_ID_CLOUD_PRINT,
        STRINGIZE_NO_EXPANSION(GOOGLE_CLIENT_ID_CLOUD_PRINT), nullptr,
        default_client_id, environment.get(), command_line);
    client_secrets_[CLIENT_CLOUD_PRINT] = CalculateKeyValue(
        GOOGLE_CLIENT_SECRET_CLOUD_PRINT,
        STRINGIZE_NO_EXPANSION(GOOGLE_CLIENT_SECRET_CLOUD_PRINT), nullptr,
        default_client_secret, environment.get(), command_line);

    client_ids_[CLIENT_REMOTING] = CalculateKeyValue(
        GOOGLE_CLIENT_ID_REMOTING,
        STRINGIZE_NO_EXPANSION(GOOGLE_CLIENT_ID_REMOTING), nullptr,
        default_client_id, environment.get(), command_line);
    client_secrets_[CLIENT_REMOTING] = CalculateKeyValue(
        GOOGLE_CLIENT_SECRET_REMOTING,
        STRINGIZE_NO_EXPANSION(GOOGLE_CLIENT_SECRET_REMOTING), nullptr,
        default_client_secret, environment.get(), command_line);

    client_ids_[CLIENT_REMOTING_HOST] = CalculateKeyValue(
        GOOGLE_CLIENT_ID_REMOTING_HOST,
        STRINGIZE_NO_EXPANSION(GOOGLE_CLIENT_ID_REMOTING_HOST), nullptr,
        default_client_id, environment.get(), command_line);
    client_secrets_[CLIENT_REMOTING_HOST] = CalculateKeyValue(
        GOOGLE_CLIENT_SECRET_REMOTING_HOST,
        STRINGIZE_NO_EXPANSION(GOOGLE_CLIENT_SECRET_REMOTING_HOST), nullptr,
        default_client_secret, environment.get(), command_line);
  }

  std::string api_key() const { return api_key_; }
#if defined(OS_IOS)
  void set_api_key(const std::string& api_key) { api_key_ = api_key; }
#endif
  std::string api_key_non_stable() const { return api_key_non_stable_; }
  std::string api_key_remoting() const { return api_key_remoting_; }

  std::string metrics_key() const { return metrics_key_; }

  std::string GetClientID(OAuth2Client client) const {
    DCHECK_LT(client, CLIENT_NUM_ITEMS);
    return client_ids_[client];
  }

#if defined(OS_IOS)
  void SetClientID(OAuth2Client client, const std::string& client_id) {
    client_ids_[client] = client_id;
  }
#endif

  std::string GetClientSecret(OAuth2Client client) const {
    DCHECK_LT(client, CLIENT_NUM_ITEMS);
    return client_secrets_[client];
  }

#if defined(OS_IOS)
  void SetClientSecret(OAuth2Client client, const std::string& client_secret) {
    client_secrets_[client] = client_secret;
  }
#endif

  std::string GetSpdyProxyAuthValue() {
#if defined(SPDY_PROXY_AUTH_VALUE)
    return SPDY_PROXY_AUTH_VALUE;
#else
    return std::string();
#endif
  }

 private:
  // Gets a value for a key.  In priority order, this will be the value
  // provided via a command-line switch, the value provided via an
  // environment variable, or finally a value baked into the build.
  // |command_line_switch| may be NULL. Official Google Chrome builds will not
  // use the value provided by an environment variable.
  static std::string CalculateKeyValue(const char* baked_in_value,
                                       const char* environment_variable_name,
                                       const char* command_line_switch,
                                       const std::string& default_if_unset,
                                       base::Environment* environment,
                                       base::CommandLine* command_line) {
    std::string key_value = baked_in_value;
    std::string temp;
#if defined(OS_MACOSX)
    // macOS and iOS can also override the API key with a value from the
    // Info.plist.
    temp = ::google_apis::GetAPIKeyFromInfoPlist(environment_variable_name);
    if (!temp.empty()) {
      key_value = temp;
      VLOG(1) << "Overriding API key " << environment_variable_name
              << " with value " << key_value << " from Info.plist.";
    }
#endif

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
    // Don't allow using the environment to override API keys for official
    // Google Chrome builds. There have been reports of mangled environments
    // affecting users (crbug.com/710575).
    if (environment->GetVar(environment_variable_name, &temp)) {
      key_value = temp;
      VLOG(1) << "Overriding API key " << environment_variable_name
              << " with value " << key_value << " from environment variable.";
    }
#endif

    if (command_line_switch && command_line->HasSwitch(command_line_switch)) {
      key_value = command_line->GetSwitchValueASCII(command_line_switch);
      VLOG(1) << "Overriding API key " << environment_variable_name
              << " with value " << key_value << " from command-line switch.";
    }

    if (key_value == DUMMY_API_TOKEN) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      // No key should be unset in an official build except the
      // GOOGLE_DEFAULT_* keys.  The default keys don't trigger this
      // check as their "unset" value is not DUMMY_API_TOKEN.
      CHECK(false);
#endif
      if (default_if_unset.size() > 0) {
        VLOG(1) << "Using default value \"" << default_if_unset
                << "\" for API key " << environment_variable_name;
        key_value = default_if_unset;
      }
    }

    // This should remain a debug-only log.
    DVLOG(1) << "API key " << environment_variable_name << "=" << key_value;

    return key_value;
  }

  std::string api_key_;
  std::string api_key_non_stable_;
  std::string api_key_remoting_;
  std::string metrics_key_;
  std::string client_ids_[CLIENT_NUM_ITEMS];
  std::string client_secrets_[CLIENT_NUM_ITEMS];
};

static base::LazyInstance<APIKeyCache>::DestructorAtExit g_api_key_cache =
    LAZY_INSTANCE_INITIALIZER;

bool HasAPIKeyConfigured() {
  return GetAPIKey() != DUMMY_API_TOKEN;
}

std::string GetAPIKey() {
  return g_api_key_cache.Get().api_key();
}

std::string GetNonStableAPIKey() {
  return g_api_key_cache.Get().api_key_non_stable();
}

std::string GetRemotingAPIKey() {
  return g_api_key_cache.Get().api_key_remoting();
}

#if defined(OS_IOS)
void SetAPIKey(const std::string& api_key) {
  g_api_key_cache.Get().set_api_key(api_key);
}
#endif

std::string GetMetricsKey() {
  return g_api_key_cache.Get().metrics_key();
}

bool HasOAuthClientConfigured() {
  for (size_t client_id = 0; client_id < CLIENT_NUM_ITEMS; ++client_id) {
    OAuth2Client client = static_cast<OAuth2Client>(client_id);
    if (GetOAuth2ClientID(client) == DUMMY_API_TOKEN ||
        GetOAuth2ClientSecret(client) == DUMMY_API_TOKEN) {
      return false;
    }
  }

  return true;
}

std::string GetOAuth2ClientID(OAuth2Client client) {
  return g_api_key_cache.Get().GetClientID(client);
}

std::string GetOAuth2ClientSecret(OAuth2Client client) {
  return g_api_key_cache.Get().GetClientSecret(client);
}

#if defined(OS_IOS)
void SetOAuth2ClientID(OAuth2Client client, const std::string& client_id) {
  g_api_key_cache.Get().SetClientID(client, client_id);
}

void SetOAuth2ClientSecret(OAuth2Client client,
                           const std::string& client_secret) {
  g_api_key_cache.Get().SetClientSecret(client, client_secret);
}
#endif

std::string GetSpdyProxyAuthValue() {
  return g_api_key_cache.Get().GetSpdyProxyAuthValue();
}

bool IsGoogleChromeAPIKeyUsed() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) || defined(USE_OFFICIAL_GOOGLE_API_KEYS)
  return true;
#else
  return false;
#endif
}

}  // namespace google_apis
