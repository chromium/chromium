// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// Unit tests for functions in google_apis/google_api_keys.h.
//
// Because the file deals with a lot of preprocessor defines and
// optionally includes an internal header, the way we test is by
// including the .cc file multiple times with different defines set.
// This is a little unorthodox, but it lets us test the behavior as
// close to unmodified as possible.

#include "google_apis/google_api_keys_unittest.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/version_info/channel.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "google_apis/api_key_cache.h"
#include "google_apis/default_api_keys.h"
#include "google_apis/gaia/gaia_config.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/google_api_keys.h"

GoogleAPIKeysTest::GoogleAPIKeysTest() : env_(base::Environment::Create()) {
  static_assert(9 == 3 + 2 * google_apis::CLIENT_NUM_ITEMS,
                "Unexpected number of key entries.");
  env_cache_[0].variable_name = "GOOGLE_API_KEY";
  env_cache_[1].variable_name = "GOOGLE_CLIENT_ID_MAIN";
  env_cache_[2].variable_name = "GOOGLE_CLIENT_SECRET_MAIN";
  env_cache_[3].variable_name = "GOOGLE_CLIENT_ID_REMOTING";
  env_cache_[4].variable_name = "GOOGLE_CLIENT_SECRET_REMOTING";
  env_cache_[5].variable_name = "GOOGLE_CLIENT_ID_REMOTING_HOST";
  env_cache_[6].variable_name = "GOOGLE_CLIENT_SECRET_REMOTING_HOST";
  env_cache_[7].variable_name = "GOOGLE_DEFAULT_CLIENT_ID";
  env_cache_[8].variable_name = "GOOGLE_DEFAULT_CLIENT_SECRET";
}

GoogleAPIKeysTest::~GoogleAPIKeysTest() {}

void GoogleAPIKeysTest::SetUp() {
  // Unset all environment variables that can affect these tests,
  // for the duration of the tests.
  for (size_t i = 0; i < std::size(env_cache_); ++i) {
    EnvironmentCache& cache = env_cache_[i];
    cache.was_set = env_->HasVar(cache.variable_name);
    cache.value.clear();
    if (cache.was_set) {
      env_->GetVar(cache.variable_name, &cache.value);
      env_->UnSetVar(cache.variable_name);
    }
  }
}

void GoogleAPIKeysTest::TearDown() {
  // Restore environment.
  for (size_t i = 0; i < std::size(env_cache_); ++i) {
    EnvironmentCache& cache = env_cache_[i];
    if (cache.was_set) {
      env_->SetVar(cache.variable_name, cache.value);
    }
  }
}

base::FilePath GetTestFilePath(const std::string& relative_path) {
  base::FilePath path;
  if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &path)) {
    return base::FilePath();
  }
  return path.AppendASCII("google_apis")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("gaia")
      .AppendASCII(relative_path);
}

#if defined(USE_OFFICIAL_GOOGLE_API_KEYS)
// Test official build behavior, since we are in a checkout where this
// is possible.
namespace official_build {

// We start every test by creating a clean environment for the
// preprocessor defines used in define_baked_in_api_keys-inc.cc
#undef GOOGLE_API_KEY
#undef GOOGLE_CLIENT_ID_MAIN
#undef GOOGLE_CLIENT_SECRET_MAIN
#undef GOOGLE_CLIENT_ID_REMOTING
#undef GOOGLE_CLIENT_SECRET_REMOTING
#undef GOOGLE_CLIENT_ID_REMOTING_HOST
#undef GOOGLE_CLIENT_SECRET_REMOTING_HOST
#undef GOOGLE_DEFAULT_CLIENT_ID
#undef GOOGLE_DEFAULT_CLIENT_SECRET

// Try setting some keys, these should be ignored since it's a build
// with official keys.
#define GOOGLE_API_KEY "bogus api_key"
#define GOOGLE_CLIENT_ID_MAIN "bogus client_id_main"

// Undef include guard so things get defined again, within this namespace.
#undef GOOGLE_APIS_GOOGLE_API_KEYS_H_
#undef GOOGLE_APIS_INTERNAL_GOOGLE_CHROME_API_KEYS_
#undef GOOGLE_APIS_INTERNAL_METRICS_SIGNING_KEY_H_
#include "google_apis/internal/google_chrome_api_keys.h"
#include "google_apis/internal/metrics_signing_key.h"

// This file must be included after the internal files defining official keys.
#include "google_apis/default_api_keys-inc.cc"

}  // namespace official_build

TEST_F(GoogleAPIKeysTest, OfficialKeys) {
  google_apis::ApiKeyCache api_key_cache(
      official_build::GetDefaultApiKeysFromDefinedValues());
  auto scoped_override =
      google_apis::SetScopedApiKeyCacheForTesting(&api_key_cache);

  EXPECT_TRUE(google_apis::HasAPIKeyConfigured());
  EXPECT_TRUE(google_apis::HasOAuthClientConfigured());

  std::string api_key = google_apis::GetAPIKey();
  std::string id_main =
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_MAIN);
  std::string secret_main =
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_MAIN);
  std::string id_remoting =
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING);
  std::string secret_remoting =
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_REMOTING);
  std::string id_remoting_host =
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING_HOST);
  std::string secret_remoting_host =
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_REMOTING_HOST);

  EXPECT_NE(0u, api_key.size());
  EXPECT_NE(google_apis::DefaultApiKeys::kUnsetApiToken, api_key);
  EXPECT_NE("bogus api_key", api_key);

  EXPECT_NE(0u, id_main.size());
  EXPECT_NE(google_apis::DefaultApiKeys::kUnsetApiToken, id_main);
  EXPECT_NE("bogus client_id_main", id_main);

  EXPECT_NE(0u, secret_main.size());
  EXPECT_NE(google_apis::DefaultApiKeys::kUnsetApiToken, secret_main);

  EXPECT_NE(0u, id_remoting.size());
  EXPECT_NE(google_apis::DefaultApiKeys::kUnsetApiToken, id_remoting);

  EXPECT_NE(0u, secret_remoting.size());
  EXPECT_NE(google_apis::DefaultApiKeys::kUnsetApiToken, secret_remoting);

  EXPECT_NE(0u, id_remoting_host.size());
  EXPECT_NE(google_apis::DefaultApiKeys::kUnsetApiToken, id_remoting_host);

  EXPECT_NE(0u, secret_remoting_host.size());
  EXPECT_NE(google_apis::DefaultApiKeys::kUnsetApiToken, secret_remoting_host);
}
#endif  // defined(USE_OFFICIAL_GOOGLE_API_KEYS)

// After this test, for the remainder of this compilation unit, we
// need official keys to not be used.
#undef BUILDFLAG_INTERNAL_CHROMIUM_BRANDING
#undef BUILDFLAG_INTERNAL_GOOGLE_CHROME_BRANDING
#define BUILDFLAG_INTERNAL_CHROMIUM_BRANDING() (1)
#define BUILDFLAG_INTERNAL_GOOGLE_CHROME_BRANDING() (0)
#undef USE_OFFICIAL_GOOGLE_API_KEYS

// Test the set of keys temporarily baked into Chromium by default.
namespace default_keys {

// We start every test by creating a clean environment for the
// preprocessor defines used in define_baked_in_api_keys-inc.cc
#undef GOOGLE_API_KEY
#undef GOOGLE_CLIENT_ID_MAIN
#undef GOOGLE_CLIENT_SECRET_MAIN
#undef GOOGLE_CLIENT_ID_REMOTING
#undef GOOGLE_CLIENT_SECRET_REMOTING
#undef GOOGLE_CLIENT_ID_REMOTING_HOST
#undef GOOGLE_CLIENT_SECRET_REMOTING_HOST
#undef GOOGLE_DEFAULT_CLIENT_ID
#undef GOOGLE_DEFAULT_CLIENT_SECRET

#include "google_apis/default_api_keys-inc.cc"

}  // namespace default_keys

TEST_F(GoogleAPIKeysTest, DefaultKeys) {
  google_apis::ApiKeyCache api_key_cache(
      default_keys::GetDefaultApiKeysFromDefinedValues());
  auto scoped_override =
      google_apis::SetScopedApiKeyCacheForTesting(&api_key_cache);

  EXPECT_FALSE(google_apis::HasAPIKeyConfigured());
  EXPECT_FALSE(google_apis::HasOAuthClientConfigured());

  std::string api_key = google_apis::GetAPIKey();
  std::string id_main =
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_MAIN);
  std::string secret_main =
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_MAIN);
  std::string id_remoting =
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING);
  std::string secret_remoting =
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_REMOTING);
  std::string id_remoting_host =
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING_HOST);
  std::string secret_remoting_host =
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_REMOTING_HOST);

  EXPECT_EQ(google_apis::DefaultApiKeys::kUnsetApiToken, api_key);
  EXPECT_EQ(google_apis::DefaultApiKeys::kUnsetApiToken, id_main);
  EXPECT_EQ(google_apis::DefaultApiKeys::kUnsetApiToken, secret_main);
  EXPECT_EQ(google_apis::DefaultApiKeys::kUnsetApiToken, id_remoting);
  EXPECT_EQ(google_apis::DefaultApiKeys::kUnsetApiToken, secret_remoting);
  EXPECT_EQ(google_apis::DefaultApiKeys::kUnsetApiToken, id_remoting_host);
  EXPECT_EQ(google_apis::DefaultApiKeys::kUnsetApiToken, secret_remoting_host);
}

// Override a couple of keys, leave the rest default.
namespace override_some_keys {

// We start every test by creating a clean environment for the
// preprocessor defines used in define_baked_in_api_keys-inc.cc
#undef GOOGLE_API_KEY
#undef GOOGLE_CLIENT_ID_MAIN
#undef GOOGLE_CLIENT_SECRET_MAIN
#undef GOOGLE_CLIENT_ID_REMOTING
#undef GOOGLE_CLIENT_SECRET_REMOTING
#undef GOOGLE_CLIENT_ID_REMOTING_HOST
#undef GOOGLE_CLIENT_SECRET_REMOTING_HOST
#undef GOOGLE_DEFAULT_CLIENT_ID
#undef GOOGLE_DEFAULT_CLIENT_SECRET

#define GOOGLE_API_KEY "API_KEY override"
#define GOOGLE_CLIENT_ID_REMOTING "CLIENT_ID_REMOTING override"

#include "google_apis/default_api_keys-inc.cc"

}  // namespace override_some_keys

TEST_F(GoogleAPIKeysTest, OverrideSomeKeys) {
  google_apis::ApiKeyCache api_key_cache(
      override_some_keys::GetDefaultApiKeysFromDefinedValues());
  auto scoped_override =
      google_apis::SetScopedApiKeyCacheForTesting(&api_key_cache);

  EXPECT_TRUE(google_apis::HasAPIKeyConfigured());
  EXPECT_FALSE(google_apis::HasOAuthClientConfigured());

  std::string api_key = google_apis::GetAPIKey();
  std::string id_main =
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_MAIN);
  std::string secret_main =
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_MAIN);
  std::string id_remoting =
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING);
  std::string secret_remoting =
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_REMOTING);
  std::string id_remoting_host =
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING_HOST);
  std::string secret_remoting_host =
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_REMOTING_HOST);

  EXPECT_EQ("API_KEY override", api_key);
  EXPECT_EQ(google_apis::DefaultApiKeys::kUnsetApiToken, id_main);
  EXPECT_EQ(google_apis::DefaultApiKeys::kUnsetApiToken, secret_main);
  EXPECT_EQ("CLIENT_ID_REMOTING override", id_remoting);
  EXPECT_EQ(google_apis::DefaultApiKeys::kUnsetApiToken, secret_remoting);
  EXPECT_EQ(google_apis::DefaultApiKeys::kUnsetApiToken, id_remoting_host);
  EXPECT_EQ(google_apis::DefaultApiKeys::kUnsetApiToken, secret_remoting_host);
}

// Override all keys.
namespace override_all_keys {

// We start every test by creating a clean environment for the
// preprocessor defines used in define_baked_in_api_keys-inc.cc
#undef GOOGLE_API_KEY
#undef GOOGLE_CLIENT_ID_MAIN
#undef GOOGLE_CLIENT_SECRET_MAIN
#undef GOOGLE_CLIENT_ID_REMOTING
#undef GOOGLE_CLIENT_SECRET_REMOTING
#undef GOOGLE_CLIENT_ID_REMOTING_HOST
#undef GOOGLE_CLIENT_SECRET_REMOTING_HOST
#undef GOOGLE_DEFAULT_CLIENT_ID
#undef GOOGLE_DEFAULT_CLIENT_SECRET

#define GOOGLE_API_KEY "API_KEY"
#define GOOGLE_CLIENT_ID_MAIN "ID_MAIN"
#define GOOGLE_CLIENT_SECRET_MAIN "SECRET_MAIN"
#define GOOGLE_CLIENT_ID_REMOTING "ID_REMOTING"
#define GOOGLE_CLIENT_SECRET_REMOTING "SECRET_REMOTING"
#define GOOGLE_CLIENT_ID_REMOTING_HOST "ID_REMOTING_HOST"
#define GOOGLE_CLIENT_SECRET_REMOTING_HOST "SECRET_REMOTING_HOST"

#include "google_apis/default_api_keys-inc.cc"

}  // namespace override_all_keys

TEST_F(GoogleAPIKeysTest, OverrideAllKeys) {
  google_apis::ApiKeyCache api_key_cache(
      override_all_keys::GetDefaultApiKeysFromDefinedValues());
  auto scoped_override =
      google_apis::SetScopedApiKeyCacheForTesting(&api_key_cache);

  EXPECT_TRUE(google_apis::HasAPIKeyConfigured());
  EXPECT_TRUE(google_apis::HasOAuthClientConfigured());

  std::string api_key = google_apis::GetAPIKey();
  std::string id_main =
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_MAIN);
  std::string secret_main =
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_MAIN);
  std::string id_remoting =
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING);
  std::string secret_remoting =
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_REMOTING);
  std::string id_remoting_host =
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING_HOST);
  std::string secret_remoting_host =
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_REMOTING_HOST);

  EXPECT_EQ("API_KEY", api_key);
  EXPECT_EQ("ID_MAIN", id_main);
  EXPECT_EQ("SECRET_MAIN", secret_main);
  EXPECT_EQ("ID_REMOTING", id_remoting);
  EXPECT_EQ("SECRET_REMOTING", secret_remoting);
  EXPECT_EQ("ID_REMOTING_HOST", id_remoting_host);
  EXPECT_EQ("SECRET_REMOTING_HOST", secret_remoting_host);
}

// Override API key via an experiment feature.
namespace override_api_key_via_feature_without_param {

// We start every test by creating a clean environment for the
// preprocessor defines used in define_baked_in_api_keys-inc.cc
#undef GOOGLE_API_KEY
#undef GOOGLE_CLIENT_ID_MAIN
#undef GOOGLE_CLIENT_SECRET_MAIN
#undef GOOGLE_CLIENT_ID_REMOTING
#undef GOOGLE_CLIENT_SECRET_REMOTING
#undef GOOGLE_CLIENT_ID_REMOTING_HOST
#undef GOOGLE_CLIENT_SECRET_REMOTING_HOST
#undef GOOGLE_DEFAULT_CLIENT_ID
#undef GOOGLE_DEFAULT_CLIENT_SECRET

#define GOOGLE_API_KEY "API_KEY"

#include "google_apis/default_api_keys-inc.cc"

}  // namespace override_api_key_via_feature_without_param

TEST_F(GoogleAPIKeysTest, OverrideApiKeyViaFeatureWithNoParamIsIgnored) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(google_apis::kOverrideAPIKeyFeature);

  base::HistogramTester tester;

  google_apis::ApiKeyCache api_key_cache(
      override_api_key_via_feature_without_param::
          GetDefaultApiKeysFromDefinedValues());
  auto scoped_override =
      google_apis::SetScopedApiKeyCacheForTesting(&api_key_cache);

  EXPECT_EQ("API_KEY", google_apis::GetAPIKey());

  tester.ExpectUniqueSample("Signin.APIKeyMatchesFeatureOnStartup", 0, 1);
}

// Override API key via an experiment feature.
namespace override_api_key_via_feature {

// We start every test by creating a clean environment for the
// preprocessor defines used in define_baked_in_api_keys-inc.cc
#undef GOOGLE_API_KEY
#undef GOOGLE_CLIENT_ID_MAIN
#undef GOOGLE_CLIENT_SECRET_MAIN
#undef GOOGLE_CLIENT_ID_REMOTING
#undef GOOGLE_CLIENT_SECRET_REMOTING
#undef GOOGLE_CLIENT_ID_REMOTING_HOST
#undef GOOGLE_CLIENT_SECRET_REMOTING_HOST
#undef GOOGLE_DEFAULT_CLIENT_ID
#undef GOOGLE_DEFAULT_CLIENT_SECRET

#define GOOGLE_API_KEY "API_KEY"

#include "google_apis/default_api_keys-inc.cc"

}  // namespace override_api_key_via_feature

TEST_F(GoogleAPIKeysTest, OverrideApiKeyViaFeature) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      google_apis::kOverrideAPIKeyFeature, {{"api_key", "API_KEY2"}});
  base::HistogramTester tester;

  google_apis::ApiKeyCache api_key_cache(
      override_api_key_via_feature::GetDefaultApiKeysFromDefinedValues());
  auto scoped_override =
      google_apis::SetScopedApiKeyCacheForTesting(&api_key_cache);

  EXPECT_EQ("API_KEY2", google_apis::GetAPIKey());

  tester.ExpectUniqueSample("Signin.APIKeyMatchesFeatureOnStartup", 1, 1);
}

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)

// Override all keys using both preprocessor defines and environment
// variables.  The environment variables should win.
namespace override_all_keys_env {

// We start every test by creating a clean environment for the
// preprocessor defines used in define_baked_in_api_keys-inc.cc
#undef GOOGLE_API_KEY
#undef GOOGLE_CLIENT_ID_MAIN
#undef GOOGLE_CLIENT_SECRET_MAIN
#undef GOOGLE_CLIENT_ID_REMOTING
#undef GOOGLE_CLIENT_SECRET_REMOTING
#undef GOOGLE_CLIENT_ID_REMOTING_HOST
#undef GOOGLE_CLIENT_SECRET_REMOTING_HOST
#undef GOOGLE_DEFAULT_CLIENT_ID
#undef GOOGLE_DEFAULT_CLIENT_SECRET

#define GOOGLE_API_KEY "API_KEY"
#define GOOGLE_CLIENT_ID_MAIN "ID_MAIN"
#define GOOGLE_CLIENT_SECRET_MAIN "SECRET_MAIN"
#define GOOGLE_CLIENT_ID_REMOTING "ID_REMOTING"
#define GOOGLE_CLIENT_SECRET_REMOTING "SECRET_REMOTING"
#define GOOGLE_CLIENT_ID_REMOTING_HOST "ID_REMOTING_HOST"
#define GOOGLE_CLIENT_SECRET_REMOTING_HOST "SECRET_REMOTING_HOST"

#include "google_apis/default_api_keys-inc.cc"

}  // namespace override_all_keys_env

TEST_F(GoogleAPIKeysTest, OverrideAllKeysUsingEnvironment) {
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  env->SetVar("GOOGLE_API_KEY", "env-API_KEY");
  env->SetVar("GOOGLE_CLIENT_ID_MAIN", "env-ID_MAIN");
  env->SetVar("GOOGLE_CLIENT_ID_REMOTING", "env-ID_REMOTING");
  env->SetVar("GOOGLE_CLIENT_ID_REMOTING_HOST", "env-ID_REMOTING_HOST");
  env->SetVar("GOOGLE_CLIENT_SECRET_MAIN", "env-SECRET_MAIN");
  env->SetVar("GOOGLE_CLIENT_SECRET_REMOTING", "env-SECRET_REMOTING");
  env->SetVar("GOOGLE_CLIENT_SECRET_REMOTING_HOST", "env-SECRET_REMOTING_HOST");

  google_apis::ApiKeyCache api_key_cache(
      override_all_keys_env::GetDefaultApiKeysFromDefinedValues());
  auto scoped_override =
      google_apis::SetScopedApiKeyCacheForTesting(&api_key_cache);

  EXPECT_TRUE(google_apis::HasAPIKeyConfigured());
  EXPECT_TRUE(google_apis::HasOAuthClientConfigured());

  // It's important that the first call to Get() only happen after the
  // environment variables have been set.
  std::string api_key = google_apis::GetAPIKey();
  std::string id_main =
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_MAIN);
  std::string secret_main =
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_MAIN);
  std::string id_remoting =
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING);
  std::string secret_remoting =
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_REMOTING);
  std::string id_remoting_host =
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING_HOST);
  std::string secret_remoting_host =
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_REMOTING_HOST);

  EXPECT_EQ("env-API_KEY", api_key);
  EXPECT_EQ("env-ID_MAIN", id_main);
  EXPECT_EQ("env-SECRET_MAIN", secret_main);
  EXPECT_EQ("env-ID_REMOTING", id_remoting);
  EXPECT_EQ("env-SECRET_REMOTING", secret_remoting);
  EXPECT_EQ("env-ID_REMOTING_HOST", id_remoting_host);
  EXPECT_EQ("env-SECRET_REMOTING_HOST", secret_remoting_host);
}

#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if BUILDFLAG(IS_IOS)
// Override all keys using both preprocessor defines and setters.
// Setters should win.
namespace override_all_keys_setters {

// We start every test by creating a clean environment for the
// preprocessor defines used in define_baked_in_api_keys-inc.cc
#undef GOOGLE_API_KEY
#undef GOOGLE_CLIENT_ID_MAIN
#undef GOOGLE_CLIENT_SECRET_MAIN
#undef GOOGLE_CLIENT_ID_REMOTING
#undef GOOGLE_CLIENT_SECRET_REMOTING
#undef GOOGLE_CLIENT_ID_REMOTING_HOST
#undef GOOGLE_CLIENT_SECRET_REMOTING_HOST
#undef GOOGLE_DEFAULT_CLIENT_ID
#undef GOOGLE_DEFAULT_CLIENT_SECRET

#define GOOGLE_API_KEY "API_KEY"
#define GOOGLE_CLIENT_ID_MAIN "ID_MAIN"
#define GOOGLE_CLIENT_SECRET_MAIN "SECRET_MAIN"
#define GOOGLE_CLIENT_ID_REMOTING "ID_REMOTING"
#define GOOGLE_CLIENT_SECRET_REMOTING "SECRET_REMOTING"
#define GOOGLE_CLIENT_ID_REMOTING_HOST "ID_REMOTING_HOST"
#define GOOGLE_CLIENT_SECRET_REMOTING_HOST "SECRET_REMOTING_HOST"

#include "google_apis/default_api_keys-inc.cc"

}  // namespace override_all_keys_setters

TEST_F(GoogleAPIKeysTest, OverrideAllKeysUsingSetters) {
  google_apis::ApiKeyCache api_key_cache(
      override_all_keys_setters::GetDefaultApiKeysFromDefinedValues());
  auto scoped_override =
      google_apis::SetScopedApiKeyCacheForTesting(&api_key_cache);

  std::string api_key("setter-API_KEY");
  google_apis::SetAPIKey(api_key);

  std::string id_main("setter-ID_MAIN");
  std::string secret_main("setter-SECRET_MAIN");
  google_apis::SetOAuth2ClientID(google_apis::CLIENT_MAIN, id_main);
  google_apis::SetOAuth2ClientSecret(google_apis::CLIENT_MAIN, secret_main);

  std::string id_remoting("setter-ID_REMOTING");
  std::string secret_remoting("setter-SECRET_REMOTING");
  google_apis::SetOAuth2ClientID(google_apis::CLIENT_REMOTING, id_remoting);
  google_apis::SetOAuth2ClientSecret(google_apis::CLIENT_REMOTING,
                                     secret_remoting);

  std::string id_remoting_host("setter-ID_REMOTING_HOST");
  std::string secret_remoting_host("setter-SECRET_REMOTING_HOST");
  google_apis::SetOAuth2ClientID(google_apis::CLIENT_REMOTING_HOST,
                                 id_remoting_host);
  google_apis::SetOAuth2ClientSecret(google_apis::CLIENT_REMOTING_HOST,
                                     secret_remoting_host);

  EXPECT_TRUE(google_apis::HasAPIKeyConfigured());
  EXPECT_TRUE(google_apis::HasOAuthClientConfigured());

  EXPECT_EQ(api_key, google_apis::GetAPIKey(::version_info::Channel::STABLE));
  EXPECT_EQ(api_key, google_apis::GetAPIKey());

  EXPECT_EQ(id_main, google_apis::GetOAuth2ClientID(google_apis::CLIENT_MAIN));
  EXPECT_EQ(secret_main,
            google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_MAIN));

  EXPECT_EQ(id_remoting,
            google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING));
  EXPECT_EQ(secret_remoting,
            google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_REMOTING));

  EXPECT_EQ(id_remoting_host,
            google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING_HOST));
  EXPECT_EQ(secret_remoting_host, google_apis::GetOAuth2ClientSecret(
                                      google_apis::CLIENT_REMOTING_HOST));
}
#endif  // BUILDFLAG(IS_IOS)

// Override all keys using both preprocessor defines and gaia config.
// Config should win.
namespace override_all_keys_config {

// We start every test by creating a clean environment for the
// preprocessor defines used in define_baked_in_api_keys-inc.cc
#undef GOOGLE_API_KEY
#undef GOOGLE_CLIENT_ID_MAIN
#undef GOOGLE_CLIENT_SECRET_MAIN
#undef GOOGLE_CLIENT_ID_REMOTING
#undef GOOGLE_CLIENT_SECRET_REMOTING
#undef GOOGLE_CLIENT_ID_REMOTING_HOST
#undef GOOGLE_CLIENT_SECRET_REMOTING_HOST
#undef GOOGLE_DEFAULT_CLIENT_ID
#undef GOOGLE_DEFAULT_CLIENT_SECRET

#define GOOGLE_API_KEY "API_KEY"
#define GOOGLE_CLIENT_ID_MAIN "ID_MAIN"
#define GOOGLE_CLIENT_SECRET_MAIN "SECRET_MAIN"
#define GOOGLE_CLIENT_ID_REMOTING "ID_REMOTING"
#define GOOGLE_CLIENT_SECRET_REMOTING "SECRET_REMOTING"
#define GOOGLE_CLIENT_ID_REMOTING_HOST "ID_REMOTING_HOST"
#define GOOGLE_CLIENT_SECRET_REMOTING_HOST "SECRET_REMOTING_HOST"

#include "google_apis/default_api_keys-inc.cc"

}  // namespace override_all_keys_config

TEST_F(GoogleAPIKeysTest, OverrideAllKeysUsingConfig) {
  auto command_line = std::make_unique<base::test::ScopedCommandLine>();
  command_line->GetProcessCommandLine()->AppendSwitchPath(
      "gaia-config", GetTestFilePath("api_keys.json"));
  GaiaConfig::ResetInstanceForTesting();

  google_apis::ApiKeyCache api_key_cache(
      override_all_keys_config::GetDefaultApiKeysFromDefinedValues());
  auto scoped_override =
      google_apis::SetScopedApiKeyCacheForTesting(&api_key_cache);

  EXPECT_TRUE(google_apis::HasAPIKeyConfigured());
  EXPECT_TRUE(google_apis::HasOAuthClientConfigured());

  EXPECT_EQ("config-API_KEY",
            google_apis::GetAPIKey(version_info::Channel::STABLE));
  EXPECT_EQ("config-API_KEY", google_apis::GetAPIKey());
  EXPECT_EQ("config-ID_MAIN",
            google_apis::GetOAuth2ClientID(google_apis::CLIENT_MAIN));
  EXPECT_EQ("config-SECRET_MAIN",
            google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_MAIN));
  EXPECT_EQ("config-ID_REMOTING",
            google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING));
  EXPECT_EQ("config-SECRET_REMOTING",
            google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_REMOTING));
  EXPECT_EQ("config-ID_REMOTING_HOST",
            google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING_HOST));
  EXPECT_EQ(
      "config-SECRET_REMOTING_HOST",
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_REMOTING_HOST));

  // It's important to reset the global config state for other tests running in
  // the same process.
  command_line.reset();
  GaiaConfig::ResetInstanceForTesting();
}
