// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/shop_card_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/test/test_bookmark_client.h"
#import "components/commerce/core/mock_shopping_service.h"
#import "components/image_fetcher/core/image_data_fetcher.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/shop_card_item.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/shop_card_mediator+testing.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class ShopCardMediatorTest : public PlatformTest {
 public:
  ShopCardMediatorTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        commerce::ShoppingServiceFactory::GetInstance(),
        base::BindRepeating(
            [](web::BrowserState*) -> std::unique_ptr<KeyedService> {
              return commerce::MockShoppingService::Build();
            }));
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));

    shopping_service_ = std::make_unique<commerce::MockShoppingService>();
    bookmark_model_ = bookmarks::TestBookmarkClient::CreateModel();
    favicon_loader_ =
        IOSChromeFaviconLoaderFactory::GetForProfile(profile_.get());

    mediator_ = [[ShopCardMediator alloc]
        initWithShoppingService:shopping_service_.get()
                    prefService:pref_service()
                  bookmarkModel:bookmark_model_.get()
                   imageFetcher:std::make_unique<
                                    image_fetcher::ImageDataFetcher>(
                                    profile_->GetSharedURLLoaderFactory())
                  faviconLoader:favicon_loader_];
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kHomeCustomizationMagicStackShopCardPriceTrackingEnabled, true);
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kHomeCustomizationMagicStackShopCardReviewsEnabled, true);
  }

  ~ShopCardMediatorTest() override {}

  void TearDown() override { [mediator_ disconnect]; }

  ShopCardMediator* mediator() { return mediator_; }

  PrefService* pref_service() { return &pref_service_; }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  raw_ptr<TestProfileIOS> profile_;
  TestProfileManagerIOS profile_manager_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<commerce::MockShoppingService> shopping_service_;
  ShopCardMediator* mediator_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  raw_ptr<FaviconLoader> favicon_loader_;
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
