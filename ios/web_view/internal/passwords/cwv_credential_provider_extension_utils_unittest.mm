// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/public/cwv_credential_provider_extension_utils.h"

#include "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#include "components/autofill/core/browser/proto/password_requirements.pb.h"
#import "ios/components/credential_provider_extension/password_spec_fetcher.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

using base::test::ios::kWaitForActionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace ios_web_view {

class CWVCredentialProviderExtensionUtilsTest : public PlatformTest {
 protected:
  void SetUp() override {
    // Delete all kSecClassGenericPassword in the keychain so that every test
    // starts with a clean slate.
    NSDictionary* spec =
        @{(__bridge id)kSecClass : (__bridge id)kSecClassGenericPassword};
    SecItemDelete((__bridge CFDictionaryRef)spec);
  }
  web::WebTaskEnvironment task_environment_;
};

// Tests that nil is returned for empty identifiers when retrieving.
TEST_F(CWVCredentialProviderExtensionUtilsTest, RetrieveEmptyIdentifier) {
  EXPECT_FALSE([CWVCredentialProviderExtensionUtils
      retrievePasswordForKeychainIdentifier:@""]);
}

// Tests that nil is returned for nil identifiers when retrieving.
TEST_F(CWVCredentialProviderExtensionUtilsTest, RetrieveNilForNilIdentifier) {
  NSString* identifier = nil;  // Required to pass nullability check.
  EXPECT_FALSE([CWVCredentialProviderExtensionUtils
      retrievePasswordForKeychainIdentifier:identifier]);
}

// Tests that nil is returned for identifiers for which there is no data.
TEST_F(CWVCredentialProviderExtensionUtilsTest, RetrieveNotStoredIdentifier) {
  EXPECT_FALSE([CWVCredentialProviderExtensionUtils
      retrievePasswordForKeychainIdentifier:@"identifier"]);
}

// Tests that a password can be retreived if it exists.
TEST_F(CWVCredentialProviderExtensionUtilsTest, RetrievePassword) {
  EXPECT_TRUE([CWVCredentialProviderExtensionUtils
      storePasswordForKeychainIdentifier:@"identifier"
                                password:@"password"]);
  EXPECT_NSEQ(@"password",
              [CWVCredentialProviderExtensionUtils
                  retrievePasswordForKeychainIdentifier:@"identifier"]);
}

// Tests that NO is returned for empty identifiers when storing.
TEST_F(CWVCredentialProviderExtensionUtilsTest, StoreEmptyIdentifier) {
  EXPECT_FALSE([CWVCredentialProviderExtensionUtils
      storePasswordForKeychainIdentifier:@""
                                password:@"password"]);
}

// Tests that NO is returned for nil identifiers when storing.
TEST_F(CWVCredentialProviderExtensionUtilsTest, StoreNilIdentifier) {
  NSString* identifier = nil;  // Required to pass nullability check.
  EXPECT_FALSE([CWVCredentialProviderExtensionUtils
      storePasswordForKeychainIdentifier:identifier
                                password:@"password"]);
}

// Tests that a new password is successfully stored.
TEST_F(CWVCredentialProviderExtensionUtilsTest, CanStorePassword) {
  EXPECT_TRUE([CWVCredentialProviderExtensionUtils
      storePasswordForKeychainIdentifier:@"identifier"
                                password:@"password"]);
}

// Tests that an existing password cannot be overriden.
TEST_F(CWVCredentialProviderExtensionUtilsTest, CannotOverridePassword) {
  EXPECT_TRUE([CWVCredentialProviderExtensionUtils
      storePasswordForKeychainIdentifier:@"identifier"
                                password:@"password1"]);
  EXPECT_FALSE([CWVCredentialProviderExtensionUtils
      storePasswordForKeychainIdentifier:@"identifier"
                                password:@"password2"]);
}

// Tests that a random password is generated.
TEST_F(CWVCredentialProviderExtensionUtilsTest, GenerateRandomPassword) {
  id mock = OCMClassMock([PasswordSpecFetcher class]);
  OCMStub([mock alloc]).andReturn(mock);
  OCMStub([mock initWithHost:@"www.chromium.org" APIKey:@"apikey"])
      .andReturn(mock);
  OCMExpect([mock
      fetchSpecWithCompletion:[OCMArg checkWithBlock:^BOOL(void (
                                  ^completionHandler)(
                                  autofill::PasswordRequirementsSpec spec)) {
        completionHandler(autofill::PasswordRequirementsSpec());
        return YES;
      }]]);
  __block bool completion_handler_called = false;
  [CWVCredentialProviderExtensionUtils
      generateRandomPasswordForHost:@"www.chromium.org"
                             APIKey:@"apikey"
                  completionHandler:^(NSString* generatedPassword) {
                    EXPECT_TRUE(generatedPassword != nil);
                    EXPECT_LT(0u, generatedPassword.length);
                    completion_handler_called = true;
                  }];
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return completion_handler_called;
  }));
  [mock verify];
}

}  // namespace ios_web_view
