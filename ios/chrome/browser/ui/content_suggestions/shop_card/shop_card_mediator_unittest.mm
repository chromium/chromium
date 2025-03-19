// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/shop_card/shop_card_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/commerce/core/mock_shopping_service.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
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
        initWithShoppingService:shopping_service_.get()
                    prefService:pref_service()];
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kHomeCustomizationMagicStackShopCardPriceTrackingEnabled, true);
  }

  ~ShopCardMediatorTest() override {}

  void TearDown() override { [mediator_ disconnect]; }

  ShopCardMediator* mediator() { return mediator_; }

  PrefService* pref_service() { return &pref_service_; }

 protected:
  TestingPrefServiceSimple pref_service_;
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
