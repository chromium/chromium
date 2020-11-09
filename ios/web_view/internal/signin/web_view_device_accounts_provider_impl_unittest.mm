// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/signin/web_view_device_accounts_provider_impl.h"

#import <Foundation/Foundation.h>
#include <map>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/test/bind_test_util.h"
#include "components/signin/public/identity_manager/ios/device_accounts_provider.h"
#import "ios/web_view/public/cwv_identity.h"
#import "ios/web_view/public/cwv_sync_controller.h"
#import "ios/web_view/public/cwv_sync_controller_data_source.h"
#import "ios/web_view/public/cwv_sync_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

using WebViewDeviceAccountsProviderImplTest = PlatformTest;

// Tests that access tokens are fetched and returned.
TEST_F(WebViewDeviceAccountsProviderImplTest, GetAccessToken) {
  id data_source =
      OCMStrictProtocolMock(@protocol(CWVSyncControllerDataSource));
  CWVSyncController.dataSource = data_source;

  OCMExpect(([data_source
      fetchAccessTokenForIdentity:[OCMArg checkWithBlock:^BOOL(
                                              CWVIdentity* identity) {
        return [identity.gaiaID isEqualToString:@"gaia-id"];
      }]
                           scopes:@[ @"scope-1", @"scope-2" ]
                completionHandler:[OCMArg checkWithBlock:^(void (^completion)(
                                      NSString*, NSDate*, NSError*)) {
                  completion(@"access-token", NSDate.distantFuture, nil);
                  return YES;
                }]]));

  bool callback_called = false;
  WebViewDeviceAccountsProviderImpl accounts_provider;
  accounts_provider.GetAccessToken(
      "gaia-id", "client-id", {"scope-1", "scope-2"},
      base::BindLambdaForTesting(
          [&](NSString* access_token, NSDate* expiration_date, NSError* error) {
            callback_called = true;
            EXPECT_NSEQ(@"access-token", access_token);
            EXPECT_NSEQ(NSDate.distantFuture, expiration_date);
            EXPECT_FALSE(error);
          }));

  EXPECT_TRUE(callback_called);

  [data_source verify];
}

// Tests that all device accounts are properly returned.
TEST_F(WebViewDeviceAccountsProviderImplTest, GetAllAccounts) {
  id data_source =
      OCMStrictProtocolMock(@protocol(CWVSyncControllerDataSource));
  CWVSyncController.dataSource = data_source;

  CWVIdentity* identity = [[CWVIdentity alloc] initWithEmail:@"foo@chromium.org"
                                                    fullName:nil
                                                      gaiaID:@"gaia-id"];
  OCMExpect([data_source allKnownIdentities]).andReturn(@[ identity ]);

  WebViewDeviceAccountsProviderImpl accounts_provider;
  std::vector<DeviceAccountsProvider::AccountInfo> accounts =
      accounts_provider.GetAllAccounts();

  ASSERT_EQ(1UL, accounts.size());
  DeviceAccountsProvider::AccountInfo account_info = accounts[0];
  EXPECT_EQ("foo@chromium.org", account_info.email);
  EXPECT_EQ("gaia-id", account_info.gaia);

  [data_source verify];
}

// Tests that authentication error categories are properly mapped.
TEST_F(WebViewDeviceAccountsProviderImplTest, GetAuthenticationErrorCategory) {
  id data_source =
      OCMStrictProtocolMock(@protocol(CWVSyncControllerDataSource));
  CWVSyncController.dataSource = data_source;

  std::map<CWVSyncError, AuthenticationErrorCategory> error_mapping = {
      {CWVSyncErrorInvalidGAIACredentials,
       kAuthenticationErrorCategoryAuthorizationErrors},
      {CWVSyncErrorUserNotSignedUp,
       kAuthenticationErrorCategoryUnknownIdentityErrors},
      {CWVSyncErrorConnectionFailed,
       kAuthenticationErrorCategoryNetworkServerErrors},
      {CWVSyncErrorServiceUnavailable,
       kAuthenticationErrorCategoryAuthorizationForbiddenErrors},
      {CWVSyncErrorRequestCanceled,
       kAuthenticationErrorCategoryUserCancellationErrors},
      {CWVSyncErrorUnexpectedServiceResponse,
       kAuthenticationErrorCategoryUnknownErrors}};

  NSError* error = [NSError errorWithDomain:@"TestErrorDomain"
                                       code:-100
                                   userInfo:nil];
  WebViewDeviceAccountsProviderImpl accounts_provider;
  for (const auto& pair : error_mapping) {
    OCMExpect([data_source
                  syncErrorForNSError:error
                             identity:[OCMArg checkWithBlock:^BOOL(
                                                  CWVIdentity* identity) {
                               return
                                   [identity.gaiaID isEqualToString:@"gaia-id"];
                             }]])
        .andReturn(pair.first);
    EXPECT_EQ(pair.second, accounts_provider.GetAuthenticationErrorCategory(
                               "gaia-id", error));
  }

  [data_source verify];
}

}
