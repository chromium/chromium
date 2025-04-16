// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/shop_card_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/test/test_bookmark_client.h"
#import "components/commerce/core/commerce_feature_list.h"
#import "components/commerce/core/mock_shopping_service.h"
#import "components/image_fetcher/core/image_data_fetcher.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/impression_limits/impression_limit_service.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/impression_limits/impression_limit_service_factory.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/shop_card_data.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/shop_card_item.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/shop_card_mediator+testing.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/shop_card_prefs.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/ntp_tiles/model/tab_resumption/tab_resumption_prefs.h"
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

namespace {

const char* kPrefsToRegister[] = {
    shop_card_prefs::kShopCardPriceDropUrlImpressions,
    tab_resumption_prefs::kTabResumptionRegularUrlImpressions,
    tab_resumption_prefs::kTabResumptionWithPriceDropUrlImpressions,
    tab_resumption_prefs::kTabResumptionWithPriceTrackableUrlImpressions,
};

}  // namespace

class ShopCardMediatorTest : public PlatformTest {
 public:
  ShopCardMediatorTest() {
    scoped_feature_list_.InitWithFeatures(
        /* enabled_features*/ {commerce::kShopCardImpressionLimits},
        /* disabled_features*/ {});

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        commerce::ShoppingServiceFactory::GetInstance(),
        base::BindRepeating(
            [](web::BrowserState*) -> std::unique_ptr<KeyedService> {
              return commerce::MockShoppingService::Build();
            }));
    builder.AddTestingFactory(ios::HistoryServiceFactory::GetInstance(),
                              ios::HistoryServiceFactory::GetDefaultFactory());
    bookmark_model_ = bookmarks::TestBookmarkClient::CreateModel();
    for (auto* const pref : kPrefsToRegister) {
      pref_service_.registry()->RegisterDictionaryPref(pref);
    }
    builder.AddTestingFactory(
        ImpressionLimitServiceFactory::GetInstance(),
        base::BindRepeating(
            [](PrefService* pref_service,
               bookmarks::BookmarkModel* bookmark_model,
               web::BrowserState* browser_state)
                -> std::unique_ptr<KeyedService> {
              ProfileIOS* profile = ProfileIOS::FromBrowserState(browser_state);
              return std::make_unique<ImpressionLimitService>(
                  pref_service,
                  ios::HistoryServiceFactory::GetForProfile(
                      profile, ServiceAccessType::EXPLICIT_ACCESS),
                  bookmark_model,
                  commerce::ShoppingServiceFactory::GetForProfile(profile));
            },
            &pref_service_, bookmark_model_.get()));

    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));

    shopping_service_ = std::make_unique<commerce::MockShoppingService>();
    favicon_loader_ =
        IOSChromeFaviconLoaderFactory::GetForProfile(profile_.get());

    mediator_ = [[ShopCardMediator alloc]
        initWithShoppingService:shopping_service_.get()
                    prefService:pref_service()
                  bookmarkModel:bookmark_model_.get()
                   imageFetcher:std::make_unique<
                                    image_fetcher::ImageDataFetcher>(
                                    profile_->GetSharedURLLoaderFactory())
                  faviconLoader:favicon_loader_
         impressionLimitService:ImpressionLimitServiceFactory::GetForProfile(
                                    profile_)];
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
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  raw_ptr<TestProfileIOS> profile_;
  TestProfileManagerIOS profile_manager_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<commerce::MockShoppingService> shopping_service_;
  ShopCardMediator* mediator_;
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

// Tests impression limit functions. ShopCards should not be
// shown for the same URL more than 3 times. The below functions are
// used for making this determination.
TEST_F(ShopCardMediatorTest, TestImpressions) {
  ShopCardItem* item = [[ShopCardItem alloc] init];
  item.shopCardData = [[ShopCardData alloc] init];
  item.shopCardData.productURL = GURL("https://example.com/");
  [mediator() logImpressionForItemForTesting:item];
  EXPECT_FALSE([mediator()
      hasReachedImpressionLimitForTesting:GURL("https://example.com/")]);
  [mediator() logImpressionForItemForTesting:item];
  EXPECT_FALSE([mediator()
      hasReachedImpressionLimitForTesting:GURL("https://example.com/")]);
  [mediator() logImpressionForItemForTesting:item];
  EXPECT_TRUE([mediator()
      hasReachedImpressionLimitForTesting:GURL("https://example.com/")]);
}

// Test opening the ShopCard URL logs the engagement and the utility
// function hasBeenOpened used to determine if we should show card for
// the URL is working as expected.
TEST_F(ShopCardMediatorTest, TestUrlOpened) {
  ShopCardItem* item = [[ShopCardItem alloc] init];
  item.shopCardData = [[ShopCardData alloc] init];
  item.shopCardData.productURL = GURL("https://example.com/");
  [mediator() logEngagementForItemForTesting:item];
  EXPECT_TRUE(
      [mediator() hasBeenOpenedForTesting:GURL("https://example.com/")]);
}

// Test hasBeenOpened works as expected when no engagement has been
// logged for the URL.
TEST_F(ShopCardMediatorTest, TestUrlNotOpened) {
  EXPECT_FALSE(
      [mediator() hasBeenOpenedForTesting:GURL("https://example.com/")]);
}

TEST_F(ShopCardMediatorTest, TestUntrackedNoShopCardData) {
  [mediator() setShopCardItemForTesting:nil];
  // Shouldn't crash
  [mediator() onUrlUntrackedForTesting:GURL("https://example.com/")];
}
