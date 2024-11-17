// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for implementation of google_apis/google_api_keys.h.
//
// Because the file deals with a lot of preprocessor defines and
// optionally includes an internal header, the way we test is by
// including the .cc file multiple times with different defines set.
// This is a little unorthodox, but it lets us test the behavior as
// close to unmodified as possible.

#include "google_apis/google_api_keys.h"

#include "base/apple/bundle_locations.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "google_apis/api_key_cache.h"
#include "google_apis/default_api_keys.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/google_api_keys.h"
#include "google_apis/google_api_keys_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "third_party/ocmock/OCMock/OCMock.h"

// We need official keys to not be used.
#undef BUILDFLAG_INTERNAL_CHROMIUM_BRANDING
#undef BUILDFLAG_INTERNAL_GOOGLE_CHROME_BRANDING
#define BUILDFLAG_INTERNAL_CHROMIUM_BRANDING() (1)
#define BUILDFLAG_INTERNAL_GOOGLE_CHROME_BRANDING() (0)
#undef USE_OFFICIAL_GOOGLE_API_KEYS

// Override some keys using both preprocessor defines and Info.plist entries.
// The Info.plist entries should win.
namespace override_some_keys_info_plist {

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

}  // namespace override_some_keys_info_plist

TEST_F(GoogleAPIKeysTest, OverrideSomeKeysUsingInfoPlist) {
  id mock_bundle = [OCMockObject mockForClass:[NSBundle class]];
  [[[mock_bundle stub] andReturn:@"plist-API_KEY"]
      objectForInfoDictionaryKey:@"GOOGLE_API_KEY"];
  [[[mock_bundle stub] andReturn:@"plist-ID_MAIN"]
      objectForInfoDictionaryKey:@"GOOGLE_CLIENT_ID_MAIN"];
  [[[mock_bundle stub] andReturn:nil] objectForInfoDictionaryKey:[OCMArg any]];
  base::apple::SetOverrideFrameworkBundle(mock_bundle);

  google_apis::ApiKeyCache api_key_cache(
      override_some_keys_info_plist::GetDefaultApiKeysFromDefinedValues());
  auto scoped_override =
      google_apis::SetScopedApiKeyCacheForTesting(&api_key_cache);

  EXPECT_TRUE(google_apis::HasAPIKeyConfigured());
  EXPECT_TRUE(google_apis::HasOAuthClientConfigured());

  // Once the keys have been configured, the bundle isn't used anymore.
  base::apple::SetOverrideFrameworkBundle(nil);

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

  EXPECT_EQ("plist-API_KEY", api_key);
  EXPECT_EQ("plist-ID_MAIN", id_main);
  EXPECT_EQ("SECRET_MAIN", secret_main);
  EXPECT_EQ("ID_REMOTING", id_remoting);
  EXPECT_EQ("SECRET_REMOTING", secret_remoting);
  EXPECT_EQ("ID_REMOTING_HOST", id_remoting_host);
  EXPECT_EQ("SECRET_REMOTING_HOST", secret_remoting_host);
}

// Override some keys using both preprocessor defines and Info.plist entries.
// The Info.plist entries should win.
namespace override_apikey_from_plist_with_feature {

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

}  // namespace override_apikey_from_plist_with_feature

TEST_F(GoogleAPIKeysTest, OverrideAPIKeyFromPlistWithFeature) {
  id mock_bundle = [OCMockObject mockForClass:[NSBundle class]];
  [[[mock_bundle stub] andReturn:@"plist-API_KEY"]
      objectForInfoDictionaryKey:@"GOOGLE_API_KEY"];
  [[[mock_bundle stub] andReturn:nil] objectForInfoDictionaryKey:[OCMArg any]];
  base::apple::SetOverrideFrameworkBundle(mock_bundle);

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      google_apis::kOverrideAPIKeyFeature, {{"api_key", "feature-API_KEY"}});

  base::HistogramTester tester;

  google_apis::ApiKeyCache api_key_cache(
      override_apikey_from_plist_with_feature::
          GetDefaultApiKeysFromDefinedValues());
  auto scoped_override =
      google_apis::SetScopedApiKeyCacheForTesting(&api_key_cache);

  EXPECT_EQ("feature-API_KEY", google_apis::GetAPIKey());

  tester.ExpectUniqueSample("Signin.APIKeyMatchesFeatureOnStartup", 1, 1);

  // Once the keys have been configured, the bundle isn't used anymore.
  base::apple::SetOverrideFrameworkBundle(nil);
}
