// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for implementation of google_api_keys namespace.
//
// Because the file deals with a lot of preprocessor defines and
// optionally includes an internal header, the way we test is by
// including the .cc file multiple times with different defines set.
// This is a little unorthodox, but it lets us test the behavior as
// close to unmodified as possible.

#include "google_apis/google_api_keys_unittest.h"

#include "base/mac/bundle_locations.h"
#include "base/macros.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "google_apis/gaia/gaia_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "third_party/ocmock/OCMock/OCMock.h"

// We need to include everything included by google_api_keys.cc once
// at global scope so that things like STL and classes from base don't
// get defined when we re-include the google_api_keys.cc file
// below. We used to include that file in its entirety here, but that
// can cause problems if the linker decides the version of symbols
// from that file included here is the "right" version.

#include <stddef.h>

#include <string>
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/stringize_macros.h"
#include "google_apis/google_api_keys_mac.h"

// After this test, for the remainder of this compilation unit, we
// need official keys to not be used.
#undef BUILDFLAG_INTERNAL_CHROMIUM_BRANDING
#undef BUILDFLAG_INTERNAL_GOOGLE_CHROME_BRANDING
#define BUILDFLAG_INTERNAL_CHROMIUM_BRANDING() (1)
#define BUILDFLAG_INTERNAL_GOOGLE_CHROME_BRANDING() (0)
#undef USE_OFFICIAL_GOOGLE_API_KEYS

// Override some keys using both preprocessor defines and Info.plist entries.
// The Info.plist entries should win.
namespace override_some_keys_info_plist {

// We start every test by creating a clean environment for the
// preprocessor defines used in google_api_keys.cc
#undef DUMMY_API_TOKEN
#undef GOOGLE_API_KEY
#undef GOOGLE_CLIENT_ID_MAIN
#undef GOOGLE_CLIENT_SECRET_MAIN
#undef GOOGLE_CLIENT_ID_CLOUD_PRINT
#undef GOOGLE_CLIENT_SECRET_CLOUD_PRINT
#undef GOOGLE_CLIENT_ID_REMOTING
#undef GOOGLE_CLIENT_SECRET_REMOTING
#undef GOOGLE_CLIENT_ID_REMOTING_HOST
#undef GOOGLE_CLIENT_SECRET_REMOTING_HOST
#undef GOOGLE_DEFAULT_CLIENT_ID
#undef GOOGLE_DEFAULT_CLIENT_SECRET

#define GOOGLE_API_KEY "API_KEY"
#define GOOGLE_CLIENT_ID_MAIN "ID_MAIN"
#define GOOGLE_CLIENT_SECRET_MAIN "SECRET_MAIN"
#define GOOGLE_CLIENT_ID_CLOUD_PRINT "ID_CLOUD_PRINT"
#define GOOGLE_CLIENT_SECRET_CLOUD_PRINT "SECRET_CLOUD_PRINT"
#define GOOGLE_CLIENT_ID_REMOTING "ID_REMOTING"
#define GOOGLE_CLIENT_SECRET_REMOTING "SECRET_REMOTING"
#define GOOGLE_CLIENT_ID_REMOTING_HOST "ID_REMOTING_HOST"
#define GOOGLE_CLIENT_SECRET_REMOTING_HOST "SECRET_REMOTING_HOST"

// Undef include guard so things get defined again, within this namespace.
#undef GOOGLE_APIS_GOOGLE_API_KEYS_H_
#undef GOOGLE_APIS_INTERNAL_GOOGLE_CHROME_API_KEYS_
#include "google_apis/google_api_keys.cc"

}  // namespace override_all_keys_env

TEST_F(GoogleAPIKeysTest, OverrideSomeKeysUsingInfoPlist) {
  namespace testcase = override_some_keys_info_plist::google_apis;

  id mock_bundle = [OCMockObject mockForClass:[NSBundle class]];
  [[[mock_bundle stub] andReturn:@"plist-API_KEY"]
      objectForInfoDictionaryKey:@"GOOGLE_API_KEY"];
  [[[mock_bundle stub] andReturn:@"plist-ID_MAIN"]
      objectForInfoDictionaryKey:@"GOOGLE_CLIENT_ID_MAIN"];
  [[[mock_bundle stub] andReturn:nil] objectForInfoDictionaryKey:[OCMArg any]];
  base::mac::SetOverrideFrameworkBundle(mock_bundle);

  EXPECT_TRUE(testcase::HasAPIKeyConfigured());
  EXPECT_TRUE(testcase::HasOAuthClientConfigured());

  // Once the keys have been configured, the bundle isn't used anymore.
  base::mac::SetOverrideFrameworkBundle(nil);

  std::string api_key = testcase::g_api_key_cache.Get().api_key();
  std::string id_main =
      testcase::g_api_key_cache.Get().GetClientID(testcase::CLIENT_MAIN);
  std::string secret_main =
      testcase::g_api_key_cache.Get().GetClientSecret(testcase::CLIENT_MAIN);
  std::string id_cloud_print =
      testcase::g_api_key_cache.Get().GetClientID(testcase::CLIENT_CLOUD_PRINT);
  std::string secret_cloud_print =
      testcase::g_api_key_cache.Get().GetClientSecret(
          testcase::CLIENT_CLOUD_PRINT);
  std::string id_remoting =
      testcase::g_api_key_cache.Get().GetClientID(testcase::CLIENT_REMOTING);
  std::string secret_remoting = testcase::g_api_key_cache.Get().GetClientSecret(
      testcase::CLIENT_REMOTING);
  std::string id_remoting_host = testcase::g_api_key_cache.Get().GetClientID(
      testcase::CLIENT_REMOTING_HOST);
  std::string secret_remoting_host =
      testcase::g_api_key_cache.Get().GetClientSecret(
          testcase::CLIENT_REMOTING_HOST);

  EXPECT_EQ("plist-API_KEY", api_key);
  EXPECT_EQ("plist-ID_MAIN", id_main);
  EXPECT_EQ("SECRET_MAIN", secret_main);
  EXPECT_EQ("ID_CLOUD_PRINT", id_cloud_print);
  EXPECT_EQ("SECRET_CLOUD_PRINT", secret_cloud_print);
  EXPECT_EQ("ID_REMOTING", id_remoting);
  EXPECT_EQ("SECRET_REMOTING", secret_remoting);
  EXPECT_EQ("ID_REMOTING_HOST", id_remoting_host);
  EXPECT_EQ("SECRET_REMOTING_HOST", secret_remoting_host);
}
