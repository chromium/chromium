// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/api_key_cache.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/feature_list.h"
#include "base/features.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringize_macros.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "google_apis/buildflags.h"
#include "google_apis/default_api_keys.h"
#include "google_apis/gaia/gaia_config.h"
#include "google_apis/gaia/gaia_switches.h"

#if BUILDFLAG(IS_APPLE)
#include "google_apis/google_api_keys_mac.h"
#endif

namespace google_apis {

namespace {

const base::FeatureParam<std::string> kOverrideAPIKeyFeatureParam{
    &kOverrideAPIKeyFeature, /*name=*/"api_key", /*default_value=*/""};

std::string GetAPIKeyOverrideViaFeature() {
  if (base::FeatureList::IsEnabled(kOverrideAPIKeyFeature)) {
    std::string override_api_key = kOverrideAPIKeyFeatureParam.Get();
    if (!override_api_key.empty()) {
      return override_api_key;
    }
  }
  return std::string();
}

// Gets a value for a key.  In priority order, this will be the value
// provided via:
// 1. Command-line switch
// 2. Config file
// 3. Environment variable
// 4. Value passed via a feature flag.
// 5. On macOS and iOS, the value passed in Info.plist
// 6. Baked into the build.
// |command_line_switch| may be NULL. Official Google Chrome builds will not
// use the value provided by an environment variable.
static std::string CalculateKeyValue(const char* baked_in_value,
                                     const char* environment_variable_name,
                                     const std::string& feature_value,
                                     const char* command_line_switch,
                                     const std::string& default_if_unset,
                                     base::Environment* environment,
                                     base::CommandLine* command_line,
                                     GaiaConfig* gaia_config,
                                     bool allow_override_via_environment,
                                     bool allow_unset_values) {
  std::string key_value = baked_in_value;
  std::string temp;
#if BUILDFLAG(IS_APPLE)
  // macOS and iOS can also override the API key with a value from the
  // Info.plist.
  temp = GetAPIKeyFromInfoPlist(environment_variable_name);
  if (!temp.empty()) {
    key_value = temp;
    VLOG(1) << "Overriding API key " << environment_variable_name
            << " with value from Info.plist.";
  }
#endif
  if (!feature_value.empty()) {
    key_value = feature_value;
    // `feature_value` should not be logged.
    VLOG(1) << "Overriding API key " << environment_variable_name
            << " with value passed via feature.";
  }

  if (allow_override_via_environment) {
    // Don't allow using the environment to override API keys for official
    // Google Chrome builds. There have been reports of mangled environments
    // affecting users (crbug.com/710575).
    if (environment->GetVar(environment_variable_name, &temp)) {
      key_value = temp;
      VLOG(1) << "Overriding API key " << environment_variable_name
              << " with value " << key_value << " from environment variable.";
    }
  }

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

  if (key_value == DefaultApiKeys::kUnsetApiToken) {
    // No key should be unset in an official build except the
    // GOOGLE_DEFAULT_* keys. The default keys don't trigger this
    // check as their "unset" value is not DefaultApiKeys::kUnsetApiToken.
    CHECK(allow_unset_values);
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
}  // namespace

BASE_FEATURE(kOverrideAPIKeyFeature,
             "OverrideAPIKey",
             base::FEATURE_DISABLED_BY_DEFAULT);

ApiKeyCache::ApiKeyCache(const DefaultApiKeys& default_api_keys) {
  std::unique_ptr<base::Environment> environment(base::Environment::Create());
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  GaiaConfig* gaia_config = GaiaConfig::GetInstance();

  std::string api_key_from_feature = GetAPIKeyOverrideViaFeature();
  api_key_ = CalculateKeyValue(default_api_keys.google_api_key,
                               STRINGIZE_NO_EXPANSION(GOOGLE_API_KEY),
                               api_key_from_feature, nullptr, std::string(),
                               environment.get(), command_line, gaia_config,
                               default_api_keys.allow_override_via_environment,
                               default_api_keys.allow_unset_values);
  base::UmaHistogramBoolean("Signin.APIKeyMatchesFeatureOnStartup",
                            api_key_from_feature == api_key_);

// A special non-stable key is at the moment defined only for Android Chrome.
#if BUILDFLAG(IS_ANDROID)
  api_key_non_stable_ = CalculateKeyValue(
      default_api_keys.google_api_key_android_non_stable,
      STRINGIZE_NO_EXPANSION(GOOGLE_API_KEY_ANDROID_NON_STABLE), std::string(),
      nullptr, std::string(), environment.get(), command_line, gaia_config,
      default_api_keys.allow_override_via_environment,
      default_api_keys.allow_unset_values);
#else
  api_key_non_stable_ = api_key_;
#endif

  api_key_remoting_ = CalculateKeyValue(
      default_api_keys.google_api_key_remoting,
      STRINGIZE_NO_EXPANSION(GOOGLE_API_KEY_REMOTING), std::string(), nullptr,
      std::string(), environment.get(), command_line, gaia_config,
      default_api_keys.allow_override_via_environment,
      default_api_keys.allow_unset_values);

  api_key_soda_ = CalculateKeyValue(
      default_api_keys.google_api_key_soda,
      STRINGIZE_NO_EXPANSION(GOOGLE_API_KEY_SODA), std::string(), nullptr,
      std::string(), environment.get(), command_line, gaia_config,
      default_api_keys.allow_override_via_environment,
      default_api_keys.allow_unset_values);
#if !BUILDFLAG(IS_ANDROID)
  api_key_hats_ = CalculateKeyValue(
      default_api_keys.google_api_key_hats,
      STRINGIZE_NO_EXPANSION(GOOGLE_API_KEY_HATS), std::string(), nullptr,
      std::string(), environment.get(), command_line, gaia_config,
      default_api_keys.allow_override_via_environment,
      default_api_keys.allow_unset_values);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  api_key_sharing_ = CalculateKeyValue(
      default_api_keys.google_api_key_sharing,
      STRINGIZE_NO_EXPANSION(GOOGLE_API_KEY_SHARING), std::string(), nullptr,
      std::string(), environment.get(), command_line, gaia_config,
      default_api_keys.allow_override_via_environment,
      default_api_keys.allow_unset_values);

  api_key_read_aloud_ = CalculateKeyValue(
      default_api_keys.google_api_key_read_aloud,
      STRINGIZE_NO_EXPANSION(GOOGLE_API_KEY_READ_ALOUD), std::string(), nullptr,
      std::string(), environment.get(), command_line, gaia_config,
      default_api_keys.allow_override_via_environment,
      default_api_keys.allow_unset_values);

  api_key_fresnel_ = CalculateKeyValue(
      default_api_keys.google_api_key_fresnel,
      STRINGIZE_NO_EXPANSION(GOOGLE_API_KEY_FRESNEL), std::string(), nullptr,
      std::string(), environment.get(), command_line, gaia_config,
      default_api_keys.allow_override_via_environment,
      default_api_keys.allow_unset_values);

  api_key_boca_ = CalculateKeyValue(
      default_api_keys.google_api_key_boca,
      STRINGIZE_NO_EXPANSION(GOOGLE_API_KEY_BOCA), std::string(), nullptr,
      std::string(), environment.get(), command_line, gaia_config,
      default_api_keys.allow_override_via_environment,
      default_api_keys.allow_unset_values);
#endif

  metrics_key_ = CalculateKeyValue(
      default_api_keys.google_metrics_signing_key,
      STRINGIZE_NO_EXPANSION(GOOGLE_METRICS_SIGNING_KEY), std::string(),
      nullptr, std::string(), environment.get(), command_line, gaia_config,
      default_api_keys.allow_override_via_environment,
      default_api_keys.allow_unset_values);

  std::string default_client_id = CalculateKeyValue(
      default_api_keys.google_default_client_id,
      STRINGIZE_NO_EXPANSION(GOOGLE_DEFAULT_CLIENT_ID), std::string(), nullptr,
      std::string(), environment.get(), command_line, gaia_config,
      default_api_keys.allow_override_via_environment,
      default_api_keys.allow_unset_values);
  std::string default_client_secret = CalculateKeyValue(
      default_api_keys.google_default_client_secret,
      STRINGIZE_NO_EXPANSION(GOOGLE_DEFAULT_CLIENT_SECRET), std::string(),
      nullptr, std::string(), environment.get(), command_line, gaia_config,
      default_api_keys.allow_override_via_environment,
      default_api_keys.allow_unset_values);

  // We currently only allow overriding the baked-in values for the
  // default OAuth2 client ID and secret using a command-line
  // argument and gaia config, since that is useful to enable testing against
  // staging servers, and since that was what was possible and
  // likely practiced by the QA team before this implementation was
  // written.
  client_ids_[CLIENT_MAIN] = CalculateKeyValue(
      default_api_keys.google_client_id_main,
      STRINGIZE_NO_EXPANSION(GOOGLE_CLIENT_ID_MAIN), std::string(),
      ::switches::kOAuth2ClientID, default_client_id, environment.get(),
      command_line, gaia_config,
      default_api_keys.allow_override_via_environment,
      default_api_keys.allow_unset_values);
  client_secrets_[CLIENT_MAIN] = CalculateKeyValue(
      default_api_keys.google_client_secret_main,
      STRINGIZE_NO_EXPANSION(GOOGLE_CLIENT_SECRET_MAIN), std::string(),
      ::switches::kOAuth2ClientSecret, default_client_secret, environment.get(),
      command_line, gaia_config,
      default_api_keys.allow_override_via_environment,
      default_api_keys.allow_unset_values);

  client_ids_[CLIENT_REMOTING] = CalculateKeyValue(
      default_api_keys.google_client_id_remoting,
      STRINGIZE_NO_EXPANSION(GOOGLE_CLIENT_ID_REMOTING), std::string(), nullptr,
      default_client_id, environment.get(), command_line, gaia_config,
      default_api_keys.allow_override_via_environment,
      default_api_keys.allow_unset_values);
  client_secrets_[CLIENT_REMOTING] = CalculateKeyValue(
      default_api_keys.google_client_secret_remoting,
      STRINGIZE_NO_EXPANSION(GOOGLE_CLIENT_SECRET_REMOTING), std::string(),
      nullptr, default_client_secret, environment.get(), command_line,
      gaia_config, default_api_keys.allow_override_via_environment,
      default_api_keys.allow_unset_values);

  client_ids_[CLIENT_REMOTING_HOST] = CalculateKeyValue(
      default_api_keys.google_client_id_remoting_host,
      STRINGIZE_NO_EXPANSION(GOOGLE_CLIENT_ID_REMOTING_HOST), std::string(),
      nullptr, default_client_id, environment.get(), command_line, gaia_config,
      default_api_keys.allow_override_via_environment,
      default_api_keys.allow_unset_values);
  client_secrets_[CLIENT_REMOTING_HOST] = CalculateKeyValue(
      default_api_keys.google_client_secret_remoting_host,
      STRINGIZE_NO_EXPANSION(GOOGLE_CLIENT_SECRET_REMOTING_HOST), std::string(),
      nullptr, default_client_secret, environment.get(), command_line,
      gaia_config, default_api_keys.allow_override_via_environment,
      default_api_keys.allow_unset_values);
}

ApiKeyCache::~ApiKeyCache() = default;

const std::string& ApiKeyCache::GetClientID(OAuth2Client client) const {
  DCHECK_LT(client, CLIENT_NUM_ITEMS);
  return client_ids_[client];
}

#if BUILDFLAG(IS_IOS)
void ApiKeyCache::SetClientID(OAuth2Client client,
                              const std::string& client_id) {
  client_ids_[client] = client_id;
}
#endif

const std::string& ApiKeyCache::GetClientSecret(OAuth2Client client) const {
  DCHECK_LT(client, CLIENT_NUM_ITEMS);
  return client_secrets_[client];
}

#if BUILDFLAG(IS_IOS)
void ApiKeyCache::SetClientSecret(OAuth2Client client,
                                  const std::string& client_secret) {
  client_secrets_[client] = client_secret;
}
#endif

bool ApiKeyCache::HasAPIKeyConfigured() const {
  return api_key_ != DefaultApiKeys::kUnsetApiToken;
}

bool ApiKeyCache::HasOAuthClientConfigured() const {
  auto is_unset = [](const std::string& value) {
    return value == DefaultApiKeys::kUnsetApiToken;
  };
  return std::ranges::none_of(client_ids_, is_unset) &&
         std::ranges::none_of(client_secrets_, is_unset);
}

}  // namespace google_apis
