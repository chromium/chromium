// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/google_api_keys.h"

// If you add more includes to this list, you also need to add them to
// google_api_keys_unittest.cc and google_api_keys_mac_unittest.mm.

#include <stddef.h>

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/stringize_macros.h"
#include "build/branding_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "google_apis/buildflags.h"
#include "google_apis/gaia/gaia_config.h"
#include "google_apis/gaia/gaia_switches.h"

#if BUILDFLAG(IS_APPLE)
#include "google_apis/google_api_keys_mac.h"
#endif

#if defined(USE_OFFICIAL_GOOGLE_API_KEYS)
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

#if BUILDFLAG(IS_ANDROID)
#if !defined(GOOGLE_API_KEY_ANDROID_NON_STABLE)
#define GOOGLE_API_KEY_ANDROID_NON_STABLE DUMMY_API_TOKEN
#endif
#endif

#if !defined(GOOGLE_API_KEY_REMOTING)
#define GOOGLE_API_KEY_REMOTING DUMMY_API_TOKEN
#endif

// API key for the Speech On-Device API (SODA).
#if !defined(GOOGLE_API_KEY_SODA)
#define GOOGLE_API_KEY_SODA DUMMY_API_TOKEN
#endif

#if !BUILDFLAG(IS_ANDROID)
// API key for the HaTS API.
#if !defined(GOOGLE_API_KEY_HATS)
#define GOOGLE_API_KEY_HATS DUMMY_API_TOKEN
#endif
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// API key for the Nearby Sharing Service.
#if !defined(GOOGLE_API_KEY_SHARING)
#define GOOGLE_API_KEY_SHARING DUMMY_API_TOKEN
#endif

// API key for the ReadAloud API.
#if !defined(GOOGLE_API_KEY_READ_ALOUD)
#define GOOGLE_API_KEY_READ_ALOUD DUMMY_API_TOKEN
#endif

// API key for the Fresnel API.
#if !defined(GOOGLE_API_KEY_FRESNEL)
#define GOOGLE_API_KEY_FRESNEL DUMMY_API_TOKEN
#endif
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
    GaiaConfig* gaia_config = GaiaConfig::GetInstance();

    api_key_ = CalculateKeyValue(
        GOOGLE_API_KEY, STRINGIZE_NO_EXPANSION(GOOGLE_API_KEY), nullptr,
        std::string(), environment.get(), command_line, gaia_config);

// A special non-stable key is at the moment defined only for Android Chrome.
#if BUILDFLAG(IS_ANDROID)
    api_key_non_stable_ = CalculateKeyValue(
        GOOGLE_API_KEY_ANDROID_NON_STABLE,
        STRINGIZE_NO_EXPANSION(GOOGLE_API_KEY_ANDROID_NON_STABLE), nullptr,
        std::string(), environment.get(), command_line, gaia_config);
#else
    api_key_non_stable_ = api_key_;
#endif

    api_key_remoting_ = CalculateKeyValue(
        GOOGLE_API_KEY_REMOTING,
        STRINGIZE_NO_EXPANSION(GOOGLE_API_KEY_REMOTING), nullptr, std::string(),
        environment.get(), command_line, gaia_config);

    api_key_soda_ = CalculateKeyValue(
        GOOGLE_API_KEY_SODA, STRINGIZE_NO_EXPANSION(GOOGLE_API_KEY_SODA),
        nullptr, std::string(), environment.get(), command_line, gaia_config);
#if !BUILDFLAG(IS_ANDROID)
    api_key_hats_ = CalculateKeyValue(
        GOOGLE_API_KEY_HATS, STRINGIZE_NO_EXPANSION(GOOGLE_API_KEY_HATS),
        nullptr, std::string(), environment.get(), command_line, gaia_config);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    api_key_sharing_ = CalculateKeyValue(
        GOOGLE_API_KEY_SHARING, STRINGIZE_NO_EXPANSION(GOOGLE_API_KEY_SHARING),
        nullptr, std::string(), environment.get(), command_line, gaia_config);

    api_key_read_aloud_ = CalculateKeyValue(
        GOOGLE_API_KEY_READ_ALOUD,
        STRINGIZE_NO_EXPANSION(GOOGLE_API_KEY_READ_ALOUD), nullptr,
        std::string(), environment.get(), command_line, gaia_config);

    api_key_fresnel_ = CalculateKeyValue(
        GOOGLE_API_KEY_FRESNEL, STRINGIZE_NO_EXPANSION(GOOGLE_API_KEY_FRESNEL),
        nullptr, std::string(), environment.get(), command_line, gaia_config);
#endif

    metrics_key_ = CalculateKeyValue(
        GOOGLE_METRICS_SIGNING_KEY,
        STRINGIZE_NO_EXPANSION(GOOGLE_METRICS_SIGNING_KEY), nullptr,
        std::string(), environment.get(), command_line, gaia_config);

    std::string default_client_id = CalculateKeyValue(
        GOOGLE_DEFAULT_CLIENT_ID,
        STRINGIZE_NO_EXPANSION(GOOGLE_DEFAULT_CLIENT_ID), nullptr,
        std::string(), environment.get(), command_line, gaia_config);
    std::string default_client_secret = CalculateKeyValue(
        GOOGLE_DEFAULT_CLIENT_SECRET,
        STRINGIZE_NO_EXPANSION(GOOGLE_DEFAULT_CLIENT_SECRET), nullptr,
        std::string(), environment.get(), command_line, gaia_config);

    // We currently only allow overriding the baked-in values for the
    // default OAuth2 client ID and secret using a command-line
    // argument and gaia config, since that is useful to enable testing against
    // staging servers, and since that was what was possible and
    // likely practiced by the QA team before this implementation was
    // written.
    client_ids_[CLIENT_MAIN] = CalculateKeyValue(
        GOOGLE_CLIENT_ID_MAIN, STRINGIZE_NO_EXPANSION(GOOGLE_CLIENT_ID_MAIN),
        ::switches::kOAuth2ClientID, default_client_id, environment.get(),
        command_line, gaia_config);
    client_secrets_[CLIENT_MAIN] = CalculateKeyValue(
        GOOGLE_CLIENT_SECRET_MAIN,
        STRINGIZE_NO_EXPANSION(GOOGLE_CLIENT_SECRET_MAIN),
        ::switches::kOAuth2ClientSecret, default_client_secret,
        environment.get(), command_line, gaia_config);

    client_ids_[CLIENT_REMOTING] = CalculateKeyValue(
        GOOGLE_CLIENT_ID_REMOTING,
        STRINGIZE_NO_EXPANSION(GOOGLE_CLIENT_ID_REMOTING), nullptr,
        default_client_id, environment.get(), command_line, gaia_config);
    client_secrets_[CLIENT_REMOTING] = CalculateKeyValue(
        GOOGLE_CLIENT_SECRET_REMOTING,
        STRINGIZE_NO_EXPANSION(GOOGLE_CLIENT_SECRET_REMOTING), nullptr,
        default_client_secret, environment.get(), command_line, gaia_config);

    client_ids_[CLIENT_REMOTING_HOST] = CalculateKeyValue(
        GOOGLE_CLIENT_ID_REMOTING_HOST,
        STRINGIZE_NO_EXPANSION(GOOGLE_CLIENT_ID_REMOTING_HOST), nullptr,
        default_client_id, environment.get(), command_line, gaia_config);
    client_secrets_[CLIENT_REMOTING_HOST] = CalculateKeyValue(
        GOOGLE_CLIENT_SECRET_REMOTING_HOST,
        STRINGIZE_NO_EXPANSION(GOOGLE_CLIENT_SECRET_REMOTING_HOST), nullptr,
        default_client_secret, environment.get(), command_line, gaia_config);
  }

  std::string api_key() const { return api_key_; }
#if BUILDFLAG(SUPPORT_EXTERNAL_GOOGLE_API_KEY)
  void set_api_key(const std::string& api_key) { api_key_ = api_key; }
#endif
  std::string api_key_non_stable() const { return api_key_non_stable_; }
  std::string api_key_remoting() const { return api_key_remoting_; }
  std::string api_key_soda() const { return api_key_soda_; }
#if !BUILDFLAG(IS_ANDROID)
  std::string api_key_hats() const { return api_key_hats_; }
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::string api_key_sharing() const { return api_key_sharing_; }
  std::string api_key_read_aloud() const { return api_key_read_aloud_; }
  std::string api_key_fresnel() const { return api_key_fresnel_; }
#endif

  std::string metrics_key() const { return metrics_key_; }

  std::string GetClientID(OAuth2Client client) const {
    DCHECK_LT(client, CLIENT_NUM_ITEMS);
    return client_ids_[client];
  }

#if BUILDFLAG(IS_IOS)
  void SetClientID(OAuth2Client client, const std::string& client_id) {
    client_ids_[client] = client_id;
  }
#endif

  std::string GetClientSecret(OAuth2Client client) const {
    DCHECK_LT(client, CLIENT_NUM_ITEMS);
    return client_secrets_[client];
  }

#if BUILDFLAG(IS_IOS)
  void SetClientSecret(OAuth2Client client, const std::string& client_secret) {
    client_secrets_[client] = client_secret;
  }
#endif

 private:
  // Gets a value for a key.  In priority order, this will be the value
  // provided via:
  // 1. Command-line switch
  // 2. Config file
  // 3. Environment variable
  // 4. Baked into the build
  // |command_line_switch| may be NULL. Official Google Chrome builds will not
  // use the value provided by an environment variable.
  static std::string CalculateKeyValue(const char* baked_in_value,
                                       const char* environment_variable_name,
                                       const char* command_line_switch,
                                       const std::string& default_if_unset,
                                       base::Environment* environment,
                                       base::CommandLine* command_line,
                                       GaiaConfig* gaia_config) {
    std::string key_value = baked_in_value;
    std::string temp;
#if BUILDFLAG(IS_APPLE)
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

    if (gaia_config &&
        gaia_config->GetAPIKeyIfExists(environment_variable_name, &temp)) {
      key_value = temp;
      VLOG(1) << "Overriding API key " << environment_variable_name
              << " with value " << key_value << " from gaia config.";
    }

    if (command_line_switch && command_line->HasSwitch(command_line_switch)) {
      key_value = command_line->GetSwitchValueASCII(command_line_switch);
      VLOG(1) << "Overriding API key " << environment_variable_name
              << " with value " << key_value << " from command-line switch.";
    }

    if (key_value == DUMMY_API_TOKEN) {
// TODO(crbug.com/1294915): Rewrite this condition using
// BUILDFLAG(SUPPORT_EXTERNAL_GOOGLE_API_KEY).
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_FUCHSIA)
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
  std::string api_key_soda_;
#if !BUILDFLAG(IS_ANDROID)
  std::string api_key_hats_;
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::string api_key_sharing_;
  std::string api_key_read_aloud_;
  std::string api_key_fresnel_;
#endif
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

std::string GetSodaAPIKey() {
  return g_api_key_cache.Get().api_key_soda();
}

#if !BUILDFLAG(IS_ANDROID)
std::string GetHatsAPIKey() {
  return g_api_key_cache.Get().api_key_hats();
}
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
std::string GetSharingAPIKey() {
  return g_api_key_cache.Get().api_key_sharing();
}

std::string GetReadAloudAPIKey() {
  return g_api_key_cache.Get().api_key_read_aloud();
}

std::string GetFresnelAPIKey() {
  return g_api_key_cache.Get().api_key_fresnel();
}
#endif

#if BUILDFLAG(SUPPORT_EXTERNAL_GOOGLE_API_KEY)
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

#if BUILDFLAG(IS_IOS)
void SetOAuth2ClientID(OAuth2Client client, const std::string& client_id) {
  g_api_key_cache.Get().SetClientID(client, client_id);
}

void SetOAuth2ClientSecret(OAuth2Client client,
                           const std::string& client_secret) {
  g_api_key_cache.Get().SetClientSecret(client, client_secret);
}
#endif

bool IsGoogleChromeAPIKeyUsed() {
#if defined(USE_OFFICIAL_GOOGLE_API_KEYS)
  return true;
#else
  return false;
#endif
}

}  // namespace google_apis
