// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "google_apis/google_api_keys.h"

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/version_info/channel.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "google_apis/api_key_cache.h"
#include "google_apis/buildflags.h"
#include "google_apis/default_api_keys.h"

#if defined(USE_OFFICIAL_GOOGLE_API_KEYS)
#include "google_apis/internal/google_chrome_api_keys.h"
#include "google_apis/internal/metrics_signing_key.h"
#endif

namespace google_apis {

namespace {

// Import `GetDefaultApiKeysFromDefinedValues()` definition based on the current
// preprocessor directives.
#include "google_apis/default_api_keys-inc.cc"

std::atomic<ApiKeyCache*> g_api_key_cache_instance = nullptr;

ApiKeyCache& CreateLeakyApiKeyCacheInstance() {
  static base::NoDestructor<ApiKeyCache> instance(
      GetDefaultApiKeysFromDefinedValues());
  // `g_api_key_cache_instance` is always assigned to the same value but it
  // might happen simultaneously from multiple threads, so use atomics.
  ApiKeyCache* expected = nullptr;
  g_api_key_cache_instance.compare_exchange_strong(expected, instance.get());
  return *instance.get();
}

ApiKeyCache& GetApiKeyCacheInstance() {
  if (!g_api_key_cache_instance) {
    return CreateLeakyApiKeyCacheInstance();
  }
  return *g_api_key_cache_instance;
}

}  // namespace

const char kAPIKeysDevelopersHowToURL[] =
    "https://www.chromium.org/developers/how-tos/api-keys";

bool HasAPIKeyConfigured() {
  return GetApiKeyCacheInstance().HasAPIKeyConfigured();
}

const std::string& GetAPIKey(version_info::Channel channel) {
  return channel == version_info::Channel::STABLE
             ? GetAPIKey()
             : GetApiKeyCacheInstance().api_key_non_stable();
}

const std::string& GetAPIKey() {
  return GetApiKeyCacheInstance().api_key();
}

const std::string& GetRemotingAPIKey() {
  return GetApiKeyCacheInstance().api_key_remoting();
}

const std::string& GetSodaAPIKey() {
  return GetApiKeyCacheInstance().api_key_soda();
}

#if !BUILDFLAG(IS_ANDROID)
const std::string& GetHatsAPIKey() {
  return GetApiKeyCacheInstance().api_key_hats();
}
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
const std::string& GetSharingAPIKey() {
  return GetApiKeyCacheInstance().api_key_sharing();
}

const std::string& GetReadAloudAPIKey() {
  return GetApiKeyCacheInstance().api_key_read_aloud();
}

const std::string& GetFresnelAPIKey() {
  return GetApiKeyCacheInstance().api_key_fresnel();
}

const std::string& GetBocaAPIKey() {
  return GetApiKeyCacheInstance().api_key_boca();
}
#endif

#if BUILDFLAG(SUPPORT_EXTERNAL_GOOGLE_API_KEY)
void SetAPIKey(const std::string& api_key) {
  GetApiKeyCacheInstance().set_api_key(api_key);
}
#endif

const std::string& GetMetricsKey() {
  return GetApiKeyCacheInstance().metrics_key();
}

bool HasOAuthClientConfigured() {
  return GetApiKeyCacheInstance().HasOAuthClientConfigured();
}

const std::string& GetOAuth2ClientID(OAuth2Client client) {
  return GetApiKeyCacheInstance().GetClientID(client);
}

const std::string& GetOAuth2ClientSecret(OAuth2Client client) {
  return GetApiKeyCacheInstance().GetClientSecret(client);
}

#if BUILDFLAG(IS_IOS)
void SetOAuth2ClientID(OAuth2Client client, const std::string& client_id) {
  GetApiKeyCacheInstance().SetClientID(client, client_id);
}

void SetOAuth2ClientSecret(OAuth2Client client,
                           const std::string& client_secret) {
  GetApiKeyCacheInstance().SetClientSecret(client, client_secret);
}
#endif

bool IsGoogleChromeAPIKeyUsed() {
#if defined(USE_OFFICIAL_GOOGLE_API_KEYS)
  return true;
#else
  return false;
#endif
}

base::ScopedClosureRunner SetScopedApiKeyCacheForTesting(
    ApiKeyCache* api_key_cache) {
  ApiKeyCache* previous_value =
      g_api_key_cache_instance.exchange(api_key_cache);
  return base::ScopedClosureRunner(base::BindOnce(
      [](ApiKeyCache* previous_value) {
        g_api_key_cache_instance = previous_value;
      },
      previous_value));
}

}  // namespace google_apis
