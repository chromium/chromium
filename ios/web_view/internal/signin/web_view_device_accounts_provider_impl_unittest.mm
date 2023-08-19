// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/signin/web_view_device_accounts_provider_impl.h"

#import <Foundation/Foundation.h>
#include <map>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "components/signin/public/identity_manager/ios/device_accounts_provider.h"
#import "ios/web_view/public/cwv_identity.h"
#import "ios/web_view/public/cwv_sync_controller.h"
#import "ios/web_view/public/cwv_sync_controller_data_source.h"
#import "ios/web_view/public/cwv_sync_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace ios_web_view {

using WebViewDeviceAccountsProviderImplTest = PlatformTest;

namespace {

id MatchIdentityByGaiaID(NSString* gaia_id) {
  return [OCMArg checkWithBlock:^BOOL(CWVIdentity* identity) {
    return [identity.gaiaID isEqualToString:gaia_id];
  }];
}

}  // namespace

// Tests that access tokens are fetched and returned.
TEST_F(WebViewDeviceAccountsProviderImplTest, GetAccessToken) {
  id data_source =
      OCMStrictProtocolMock(@protocol(CWVSyncControllerDataSource));
  CWVSyncController.dataSource = data_source;

  OCMExpect([data_source
                fetchAccessTokenForIdentity:MatchIdentityByGaiaID(@"gaia-id")
                                     scopes:(@[ @"scope-1", @"scope-2" ])
                                     completionHandler:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        __unsafe_unretained void (^block)(NSString*, NSDate*, NSError*) = nil;
        [invocation getArgument:&block atIndex:4];  // completionHandler: index
        block(@"access-token", base::Time::Max().ToNSDate(), nil);
      });

  bool callback_called = false;
  WebViewDeviceAccountsProviderImpl accounts_provider;
  accounts_provider.GetAccessToken(
      "gaia-id", "client-id", {"scope-1", "scope-2"},
      base::BindLambdaForTesting(
          [&](DeviceAccountsProvider::AccessTokenResult result) {
            callback_called = true;
            ASSERT_TRUE(result.has_value());
            EXPECT_EQ(result->token, "access-token");
            EXPECT_EQ(result->expiration_time, base::Time::Max());
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
}
