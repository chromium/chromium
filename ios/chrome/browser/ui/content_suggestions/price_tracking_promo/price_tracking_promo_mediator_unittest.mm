// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_mediator.h"

#import <UserNotifications/UserNotifications.h>

#import "components/commerce/core/mock_shopping_service.h"
#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_mediator+testing.h"
#import "ios/testing/scoped_block_swizzler.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class PriceTrackingPromoMediatorTest : public PlatformTest {
 public:
  PriceTrackingPromoMediatorTest() {
    shopping_service_ = std::make_unique<commerce::MockShoppingService>();
    mediator_ = [[PriceTrackingPromoMediator alloc]
        initWithShoppingService:shopping_service_.get()];
    // Mock notifications settings response.
    mock_notification_center_ = OCMClassMock([UNUserNotificationCenter class]);
    UNUserNotificationCenter* (^swizzle_block)() =
        ^UNUserNotificationCenter*() {
          return mock_notification_center_;
        };
    notification_center_swizzler_ = std::make_unique<ScopedBlockSwizzler>(
        [UNUserNotificationCenter class], @selector(currentNotificationCenter),
        swizzle_block);
  }

  ~PriceTrackingPromoMediatorTest() override {}

  PriceTrackingPromoMediator* mediator() { return mediator_; }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<commerce::MockShoppingService> shopping_service_;
  PriceTrackingPromoMediator* mediator_;
  std::unique_ptr<ScopedBlockSwizzler> notification_center_swizzler_;
  id mock_notification_center_;
};

// First opt in flow (user has not enabled notifications at all for the app).
TEST_F(PriceTrackingPromoMediatorTest, TestAllowPriceTrackingNotifications) {
  id settings = OCMClassMock([UNNotificationSettings class]);
  OCMStub([mock_notification_center_
      getNotificationSettingsWithCompletionHandler:
          ([OCMArg invokeBlockWithArgs:settings, nil])]);
  OCMStub([settings authorizationStatus])
      .andReturn(UNAuthorizationStatusAuthorized);
  id mockDelegate =
      OCMStrictProtocolMock(@protocol(PriceTrackingPromoMediatorDelegate));
  OCMExpect([mockDelegate removePriceTrackingPromo]);
  mediator().delegate = mockDelegate;
  [mediator() allowPriceTrackingNotifications];
}

// Test disconnecting the mediator.
TEST_F(PriceTrackingPromoMediatorTest, TestDisconnect) {
  EXPECT_NE(nil, mediator().shoppingServiceForTesting);
  [mediator() disconnect];
  EXPECT_EQ(nil, mediator().shoppingServiceForTesting);
}

// Resets card and fetches most recent subscription, if available.
TEST_F(PriceTrackingPromoMediatorTest, TestReset) {
  EXPECT_NE(nil, mediator().priceTrackingPromoItemForTesting);
  [mediator() reset];
  EXPECT_EQ(nil, mediator().priceTrackingPromoItemForTesting);
}
