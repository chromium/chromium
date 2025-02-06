// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/shop_card/shop_card_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/commerce/core/mock_shopping_service.h"
#import "ios/chrome/browser/ui/content_suggestions/shop_card/shop_card_item.h"
#import "ios/chrome/browser/ui/content_suggestions/shop_card/shop_card_mediator+testing.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class ShopCardMediatorTest : public PlatformTest {
 public:
  ShopCardMediatorTest() {
    shopping_service_ = std::make_unique<commerce::MockShoppingService>();
    mediator_ = [[ShopCardMediator alloc]
        initWithShoppingService:shopping_service_.get()];
  }

  ~ShopCardMediatorTest() override {}

  void TearDown() override { [mediator_ disconnect]; }

  ShopCardMediator* mediator() { return mediator_; }

 protected:
  std::unique_ptr<commerce::MockShoppingService> shopping_service_;
  ShopCardMediator* mediator_;
  web::WebTaskEnvironment task_environment_;
};

// Test disconnecting the mediator.
TEST_F(ShopCardMediatorTest, TestDisconnect) {
  EXPECT_NE(nil, mediator().shoppingServiceForTesting);
  [mediator() disconnect];
  EXPECT_EQ(nil, mediator().shoppingServiceForTesting);
}

// Resets card.
TEST_F(ShopCardMediatorTest, TestReset) {
  ShopCardItem* item = [[ShopCardItem alloc] init];
  [mediator() setShopCardItemForTesting:item];
  EXPECT_NE(nil, mediator().shopCardItemForTesting);
  [mediator() reset];
  EXPECT_EQ(nil, mediator().shopCardItemForTesting);
}

TEST_F(ShopCardMediatorTest, TestRemoveShopCard) {
  id mockDelegate = OCMStrictProtocolMock(@protocol(ShopCardMediatorDelegate));
  OCMExpect([mockDelegate removeShopCard]);
  mediator().delegate = mockDelegate;
  [mediator() disableModule];
}
