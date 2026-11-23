// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/account_switching_promo/coordinator/account_switching_promo_display_handler.h"

#import <Foundation/Foundation.h>

#import "components/feature_engagement/public/feature_constants.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/promo_config.h"
#import "ios/chrome/browser/shared/public/commands/promos_manager_commands.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using AccountSwitchingPromoDisplayHandlerTest = PlatformTest;

// Tests that `AccountSwitchingPromoDisplayHandler` returns the Account
// Switching promo configuration.
TEST_F(AccountSwitchingPromoDisplayHandlerTest, TestConfig) {
  AccountSwitchingPromoDisplayHandler* handler =
      [[AccountSwitchingPromoDisplayHandler alloc] init];
  PromoConfig config = [handler config];
  EXPECT_EQ(promos_manager::Promo::AccountSwitching, config.identifier);
  EXPECT_EQ(feature_engagement::kIPHAccountSwitchingPromoFeature,
            config.feature_engagement_feature);
}

// Tests that `AccountSwitchingPromoDisplayHandler` asks its command handler to
// show the Account Switching promo.
TEST_F(AccountSwitchingPromoDisplayHandlerTest, TestHandleDisplay) {
  AccountSwitchingPromoDisplayHandler* display_handler =
      [[AccountSwitchingPromoDisplayHandler alloc] init];
  id mock_promos_manager_handler =
      OCMStrictProtocolMock(@protocol(PromosManagerCommands));
  display_handler.handler = mock_promos_manager_handler;
  OCMExpect([mock_promos_manager_handler showAccountSwitchingPromo]);
  [display_handler handleDisplay];
  EXPECT_OCMOCK_VERIFY(mock_promos_manager_handler);
}
