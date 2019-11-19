// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/api/identity/identity_api.h"

#include "base/values.h"
#include "extensions/browser/api_unittest.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"

namespace extensions {
namespace shell {

class IdentityApiTest : public ApiUnitTest {
 public:
  IdentityApiTest() {}
  ~IdentityApiTest() override {}

  // testing::Test:
  void SetUp() override {
    ApiUnitTest::SetUp();
    DictionaryBuilder oauth2;
    oauth2.Set("client_id", "123456.apps.googleusercontent.com")
        .Set("scopes", ListBuilder()
                           .Append("https://www.googleapis.com/auth/drive")
                           .Build());
    // Create an extension with OAuth2 scopes.
    set_extension(ExtensionBuilder("Test")
                      .SetManifestKey("oauth2", oauth2.Build())
                      .Build());
  }
};

// Verifies that the removeCachedAuthToken function exists and can be called
// without crashing.
TEST_F(IdentityApiTest, RemoveCachedAuthToken) {
  // Function succeeds and returns nothing (for its callback).
  std::unique_ptr<base::Value> result = RunFunctionAndReturnValue(
      new IdentityRemoveCachedAuthTokenFunction, "[{}]");
  EXPECT_FALSE(result.get());
}

}  // namespace shell
}  // namespace extensions
