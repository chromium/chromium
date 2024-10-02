// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/price_notifications/price_notifications_price_tracking_mediator.h"

#import <UserNotifications/UserNotifications.h>

#import <string_view>

#import "base/apple/foundation_util.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/bookmarks/test/bookmark_test_helpers.h"
#import "components/commerce/core/mock_shopping_service.h"
#import "components/commerce/core/price_tracking_utils.h"
#import "components/commerce/core/test_utils.h"
#import "components/image_fetcher/core/cached_image_fetcher.h"
#import "components/image_fetcher/core/image_data_fetcher.h"
#import "components/power_bookmarks/core/power_bookmark_utils.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/price_insights/coordinator/test_price_insights_consumer.h"
#import "ios/chrome/browser/price_insights/ui/price_insights_item.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/price_notifications/cells/price_notifications_table_view_item.h"
#import "ios/chrome/browser/ui/price_notifications/price_notifications_mutator.h"
#import "ios/chrome/browser/ui/price_notifications/price_notifications_table_view_controller.h"
#import "ios/chrome/browser/ui/price_notifications/test_price_notifications_consumer.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/public/provider/chrome/browser/push_notification/push_notification_api.h"
#import "ios/testing/scoped_block_swizzler.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

const char kTestUrl[] = "https://www.merchant.com/price_drop_product";
const char kTestUrlVariant[] =
    "https://www.merchant.com/price_drop_product?variant=1";
const char kBookmarkTitle[] = "My product title";
uint64_t kClusterId = 12345L;

PriceInsightsItem* GetPriceInsightsItem() {
  PriceInsightsItem* item = [[PriceInsightsItem alloc] init];
  item.title = base::SysUTF8ToNSString(kBookmarkTitle);
  item.productURL = GURL(kTestUrl);
  item.buyingOptionsURL = GURL(kTestUrl);
  return item;
}

void TrackBookmark(commerce::ShoppingService* shopping_service,
                   bookmarks::BookmarkModel* bookmark_model,
                   const bookmarks::BookmarkNode* product) {
  base::RunLoop run_loop;
  SetPriceTrackingStateForBookmark(
      shopping_service, bookmark_model, product, true,
      base::BindOnce(
          [](base::RunLoop* run_loop, bool success) {
            EXPECT_TRUE(success);
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

const bookmarks::BookmarkNode* PrepareSubscription(
    commerce::MockShoppingService* shopping_service,
    bookmarks::BookmarkModel* bookmark_model,
    BOOL unsubscribe_callback) {
  std::string_view title = kBookmarkTitle;
  const bookmarks::BookmarkNode* product =
      commerce::AddProductBookmark(bookmark_model, base::UTF8ToUTF16(title),
                                   GURL(kTestUrl), kClusterId, true);
  const bookmarks::BookmarkNode* default_folder =
      bookmark_model->account_mobile_node();
  bookmark_model->AddURL(default_folder, default_folder->children().size(),
                         base::UTF8ToUTF16(title), GURL(kTestUrl));
  shopping_service->SetSubscribeCallbackValue(true);
  shopping_service->SetIsSubscribedCallbackValue(true);
  shopping_service->SetUnsubscribeCallbackValue(unsubscribe_callback);
  TrackBookmark(shopping_service, bookmark_model, product);

  commerce::ProductInfo product_info;
  product_info.title = kBookmarkTitle;
  std::optional<commerce::ProductInfo> optional_product_info;
  optional_product_info.emplace(product_info);
  shopping_service->SetResponseForGetProductInfoForUrl(optional_product_info);

  std::vector<commerce::CommerceSubscription> subscriptions;
  std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
      power_bookmarks::GetNodePowerBookmarkMeta(bookmark_model, product);
  commerce::CommerceSubscription sub(
      commerce::SubscriptionType::kPriceTrack,
      commerce::IdentifierType::kProductClusterId,
      base::NumberToString(meta->shopping_specifics().product_cluster_id()),
      commerce::ManagementType::kUserManaged);
  subscriptions.push_back(sub);
  shopping_service->SetGetAllSubscriptionsCallbackValue(subscriptions);
  return product;
}

}  // namespace

class PriceNotificationsPriceTrackingMediatorTest : public PlatformTest {
 public:
  PriceNotificationsPriceTrackingMediatorTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(ios::BookmarkModelFactory::GetInstance(),
                              ios::BookmarkModelFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        commerce::ShoppingServiceFactory::GetInstance(),
        base::BindRepeating(
            [](web::BrowserState*) -> std::unique_ptr<KeyedService> {
              return commerce::MockShoppingService::Build();
            }));
    TestProfileIOS* test_profile =
        profile_manager_.AddProfileWithBuilder(std::move(builder));

    browser_list_ = BrowserListFactory::GetForProfile(test_profile);
    browser_ = std::make_unique<TestBrowser>(test_profile);
    browser_list_->AddBrowser(browser_.get());
    web_state_ = std::make_unique<web::FakeWebState>();
    std::unique_ptr<web::FakeNavigationManager> navigation_manager =
        std::make_unique<web::FakeNavigationManager>();
    navigation_manager->AddItem(GURL(kTestUrl), ui::PAGE_TRANSITION_LINK);
    navigation_manager->SetLastCommittedItem(
        navigation_manager->GetItemAtIndex(0));
    web_state_->SetNavigationManager(std::move(navigation_manager));
    web_state_->SetBrowserState(test_profile);
    web_state_->SetNavigationItemCount(1);
    web_state_->SetCurrentURL(GURL(kTestUrl));
    image_fetcher_ = std::make_unique<image_fetcher::ImageDataFetcher>(
        test_profile->GetSharedURLLoaderFactory());

    bookmark_model_ = ios::BookmarkModelFactory::GetForProfile(test_profile);
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model_);
    bookmark_model_->CreateAccountPermanentFolders();

    shopping_service_ = static_cast<commerce::MockShoppingService*>(
        commerce::ShoppingServiceFactory::GetForProfile(test_profile));
    shopping_service_->SetupPermissiveMock();
    push_notification_service_ = ios::provider::CreatePushNotificationService();
    mediator_ = [[PriceNotificationsPriceTrackingMediator alloc]
        initWithShoppingService:(commerce::ShoppingService*)shopping_service_
                  bookmarkModel:bookmark_model_
                   imageFetcher:std::move(image_fetcher_)
                       webState:web_state_.get()->GetWeakPtr()
        pushNotificationService:(PushNotificationService*)
                                    push_notification_service_.get()];
  }

 protected:
  // Sets up a mock notification center, so notification requests can be
  // tested.
  void SetupMockNotificationCenter() {
    mock_notification_center_ = OCMClassMock([UNUserNotificationCenter class]);
    // Swizzle in the mock notification center.
    UNUserNotificationCenter* (^swizzle_block)() =
        ^UNUserNotificationCenter*() {
          return mock_notification_center_;
        };
    notification_center_swizzler_ = std::make_unique<ScopedBlockSwizzler>(
        [UNUserNotificationCenter class], @selector(currentNotificationCenter),
        swizzle_block);
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  std::unique_ptr<Browser> browser_;
  PriceNotificationsPriceTrackingMediator* mediator_;
  std::unique_ptr<web::FakeWebState> web_state_;
  raw_ptr<commerce::MockShoppingService> shopping_service_;
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
  raw_ptr<BrowserList> browser_list_;
  std::unique_ptr<image_fetcher::ImageDataFetcher> image_fetcher_;
  std::unique_ptr<PushNotificationService> push_notification_service_;
  std::unique_ptr<ScopedBlockSwizzler> notification_center_swizzler_;
  id mock_notification_center_;
  TestPriceNotificationsConsumer* consumer_ =
      [[TestPriceNotificationsConsumer alloc] init];
  TestPriceInsightsConsumer* price_insights_consumer_ =
      [[TestPriceInsightsConsumer alloc] init];
};

TEST_F(PriceNotificationsPriceTrackingMediatorTest,
       TrackableItemIsEmptyWhenUserIsViewingProductWebpageAndProduct) {
  PrepareSubscription(shopping_service_, bookmark_model_, true);
  mediator_.consumer = consumer_;

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, ^bool {
        base::RunLoop().RunUntilIdle();
        return consumer_.didExecuteAction;
      }));

  EXPECT_EQ(consumer_.trackableItem, nil);
  EXPECT_EQ(consumer_.isCurrentlyTrackingVisibleProduct, YES);
}

TEST_F(
    PriceNotificationsPriceTrackingMediatorTest,
    TrackableItemExistsWhenUserUntracksProductFromWebpageIsCurrentlyViewing) {
  commerce::ProductInfo product_info;
  product_info.title = kBookmarkTitle;
  product_info.product_cluster_id.emplace(12345L);
  std::optional<commerce::ProductInfo> optional_product_info;
  optional_product_info.emplace(product_info);
  shopping_service_->SetResponseForGetProductInfoForUrl(optional_product_info);
  mediator_.consumer = consumer_;

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, ^bool {
        base::RunLoop().RunUntilIdle();
        return consumer_.didExecuteAction;
      }));

  consumer_.didExecuteAction = NO;
  const bookmarks::BookmarkNode* default_folder =
      bookmark_model_->account_mobile_node();
  bookmark_model_->AddURL(default_folder, default_folder->children().size(),
                          base::UTF8ToUTF16(product_info.title),
                          GURL(kTestUrl));
  shopping_service_->SetUnsubscribeCallbackValue(true);

  PriceNotificationsTableViewItem* product =
      [[PriceNotificationsTableViewItem alloc] init];
  product.title = base::SysUTF8ToNSString(kBookmarkTitle);
  product.entryURL = GURL(kTestUrl);
  id<PriceNotificationsMutator> mutator = mediator_;
  [mutator stopTrackingItem:product];
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, ^bool {
        base::RunLoop().RunUntilIdle();
        return consumer_.didExecuteAction;
      }));

  EXPECT_EQ(consumer_.trackableItem.title, product.title);
}

TEST_F(PriceNotificationsPriceTrackingMediatorTest,
       SuccessfullyTrackedProductURLFromPriceInsights) {
  SetupMockNotificationCenter();
  id settings = OCMClassMock([UNNotificationSettings class]);
  OCMStub([mock_notification_center_
      getNotificationSettingsWithCompletionHandler:
          ([OCMArg invokeBlockWithArgs:settings, nil])]);
  OCMStub([settings authorizationStatus])
      .andReturn(UNAuthorizationStatusAuthorized);

  commerce::ProductInfo product_info;
  product_info.title = kBookmarkTitle;
  product_info.product_cluster_id = std::make_optional(kClusterId);
  std::optional<commerce::ProductInfo> optional_product_info;
  optional_product_info.emplace(product_info);
  shopping_service_->SetResponseForGetProductInfoForUrl(optional_product_info);

  price_insights_consumer_.didPriceTrack = NO;
  mediator_.priceInsightsConsumer = price_insights_consumer_;
  [mediator_ tryPriceInsightsTrackItem:GetPriceInsightsItem()];

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, ^bool {
        base::RunLoop().RunUntilIdle();
        return price_insights_consumer_.didPriceTrack;
      }));

  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
}

TEST_F(PriceNotificationsPriceTrackingMediatorTest,
       PresentAlertWhenTrackingIsUnsuccessfulFromPriceInsights) {
  SetupMockNotificationCenter();
  id settings = OCMClassMock([UNNotificationSettings class]);
  OCMStub([mock_notification_center_
      getNotificationSettingsWithCompletionHandler:
          ([OCMArg invokeBlockWithArgs:settings, nil])]);
  OCMStub([settings authorizationStatus])
      .andReturn(UNAuthorizationStatusAuthorized);

  commerce::ProductInfo product_info;
  product_info.title = kBookmarkTitle;
  product_info.product_cluster_id = std::make_optional(kClusterId);
  std::optional<commerce::ProductInfo> optional_product_info;
  optional_product_info.emplace(product_info);
  shopping_service_->SetResponseForGetProductInfoForUrl(optional_product_info);
  shopping_service_->SetSubscribeCallbackValue(false);

  price_insights_consumer_.didPresentStartPriceTrackingErrorSnackbarForItem =
      NO;
  mediator_.priceInsightsConsumer = price_insights_consumer_;
  [mediator_ tryPriceInsightsTrackItem:GetPriceInsightsItem()];

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, ^bool {
        base::RunLoop().RunUntilIdle();
        return price_insights_consumer_
            .didPresentStartPriceTrackingErrorSnackbarForItem;
      }));

  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
}

TEST_F(PriceNotificationsPriceTrackingMediatorTest,
       SuccessfullyUntrackedProductURLFromPriceInsights) {
  commerce::ProductInfo product_info;
  product_info.title = kBookmarkTitle;
  product_info.product_cluster_id = std::make_optional(kClusterId);
  std::optional<commerce::ProductInfo> optional_product_info;
  optional_product_info.emplace(product_info);

  const bookmarks::BookmarkNode* default_folder =
      bookmark_model_->account_mobile_node();
  bookmark_model_->AddURL(default_folder, default_folder->children().size(),
                          base::UTF8ToUTF16(product_info.title),
                          GURL(kTestUrl));
  shopping_service_->SetUnsubscribeCallbackValue(true);
  shopping_service_->SetResponseForGetProductInfoForUrl(optional_product_info);

  price_insights_consumer_.didPriceUntrack = NO;
  mediator_.priceInsightsConsumer = price_insights_consumer_;
  [mediator_ priceInsightsStopTrackingItem:GetPriceInsightsItem()];

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, ^bool {
        base::RunLoop().RunUntilIdle();
        return price_insights_consumer_.didPriceUntrack;
      }));
}

TEST_F(PriceNotificationsPriceTrackingMediatorTest,
       PresentAlertWhenUntrackingIsUnsuccessfulFromPriceInsights) {
  commerce::ProductInfo product_info;
  product_info.title = kBookmarkTitle;
  product_info.product_cluster_id = std::make_optional(kClusterId);
  std::optional<commerce::ProductInfo> optional_product_info;
  optional_product_info.emplace(product_info);

  const bookmarks::BookmarkNode* default_folder =
      bookmark_model_->account_mobile_node();
  bookmark_model_->AddURL(default_folder, default_folder->children().size(),
                          base::UTF8ToUTF16(product_info.title),
                          GURL(kTestUrl));
  shopping_service_->SetUnsubscribeCallbackValue(false);
  shopping_service_->SetResponseForGetProductInfoForUrl(optional_product_info);

  price_insights_consumer_.didPresentStopPriceTrackingErrorSnackbarForItem = NO;
  mediator_.priceInsightsConsumer = price_insights_consumer_;
  [mediator_ priceInsightsStopTrackingItem:GetPriceInsightsItem()];

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, ^bool {
        base::RunLoop().RunUntilIdle();
        return price_insights_consumer_
            .didPresentStopPriceTrackingErrorSnackbarForItem;
      }));
}

TEST_F(PriceNotificationsPriceTrackingMediatorTest,
       NavigateToWebPageUponUserRequestsFromPriceInsights) {
  price_insights_consumer_.didNavigateToWebpage = NO;
  mediator_.priceInsightsConsumer = price_insights_consumer_;
  [mediator_ priceInsightsNavigateToWebpageForItem:GetPriceInsightsItem()];

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, ^bool {
        base::RunLoop().RunUntilIdle();
        return price_insights_consumer_.didNavigateToWebpage;
      }));
}

TEST_F(PriceNotificationsPriceTrackingMediatorTest,
       SuccessfullyUntrackedProductFromPriceInsightsThroughVariantURL) {
  commerce::ProductInfo product_info;
  product_info.title = kBookmarkTitle;
  product_info.product_cluster_id = std::make_optional(kClusterId);
  std::optional<commerce::ProductInfo> optional_product_info;
  optional_product_info.emplace(product_info);

  const bookmarks::BookmarkNode* product = commerce::AddProductBookmark(
      bookmark_model_, base::UTF8ToUTF16(product_info.title), GURL(kTestUrl),
      kClusterId, true);
  const bookmarks::BookmarkNode* default_folder =
      bookmark_model_->account_mobile_node();
  bookmark_model_->AddURL(default_folder, default_folder->children().size(),
                          base::UTF8ToUTF16(product_info.title),
                          GURL(kTestUrl));
  shopping_service_->SetSubscribeCallbackValue(true);
  shopping_service_->SetIsSubscribedCallbackValue(true);
  TrackBookmark(shopping_service_, bookmark_model_, product);

  // Call to ensure backend calls to Unsubscribe are successful.
  shopping_service_->SetUnsubscribeCallbackValue(true);
  shopping_service_->SetResponseForGetProductInfoForUrl(optional_product_info);

  price_insights_consumer_.didPriceUntrack = NO;
  mediator_.priceInsightsConsumer = price_insights_consumer_;
  PriceInsightsItem* price_insights = GetPriceInsightsItem();
  price_insights.productURL = GURL(kTestUrlVariant);
  price_insights.clusterId = kClusterId;
  [mediator_ priceInsightsStopTrackingItem:price_insights];

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, ^bool {
        base::RunLoop().RunUntilIdle();
        return price_insights_consumer_.didPriceUntrack;
      }));
}

TEST_F(PriceNotificationsPriceTrackingMediatorTest,
       PresentNotificationAlertWhenNotificationAuthorizationDenied) {
  SetupMockNotificationCenter();
  id settings = OCMClassMock([UNNotificationSettings class]);
  OCMStub([mock_notification_center_
      getNotificationSettingsWithCompletionHandler:
          ([OCMArg invokeBlockWithArgs:settings, nil])]);
  OCMStub([settings authorizationStatus])
      .andReturn(UNAuthorizationStatusDenied);

  commerce::ProductInfo product_info;
  product_info.title = kBookmarkTitle;
  product_info.product_cluster_id = std::make_optional(kClusterId);
  std::optional<commerce::ProductInfo> optional_product_info;
  optional_product_info.emplace(product_info);
  shopping_service_->SetResponseForGetProductInfoForUrl(optional_product_info);

  price_insights_consumer_.didPriceTrack = NO;
  price_insights_consumer_.didPresentPushNotificationPermissionAlertForItem =
      NO;
  mediator_.priceInsightsConsumer = price_insights_consumer_;
  [mediator_ tryPriceInsightsTrackItem:GetPriceInsightsItem()];

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, ^bool {
        base::RunLoop().RunUntilIdle();
        return price_insights_consumer_
            .didPresentPushNotificationPermissionAlertForItem;
      }));

  ASSERT_FALSE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, ^bool {
        base::RunLoop().RunUntilIdle();
        return price_insights_consumer_.didPriceTrack;
      }));

  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
}

TEST_F(PriceNotificationsPriceTrackingMediatorTest,
       NoTrackWhenNotificationAuthorizationUndetermined) {
  SetupMockNotificationCenter();
  id settings = OCMClassMock([UNNotificationSettings class]);
  OCMStub([mock_notification_center_
      getNotificationSettingsWithCompletionHandler:
          ([OCMArg invokeBlockWithArgs:settings, nil])]);
  OCMStub([settings authorizationStatus])
      .andReturn(UNAuthorizationStatusNotDetermined);

  commerce::ProductInfo product_info;
  product_info.title = kBookmarkTitle;
  product_info.product_cluster_id = std::make_optional(kClusterId);
  std::optional<commerce::ProductInfo> optional_product_info;
  optional_product_info.emplace(product_info);
  shopping_service_->SetResponseForGetProductInfoForUrl(optional_product_info);

  price_insights_consumer_.didPriceTrack = NO;
  price_insights_consumer_.didPresentPushNotificationPermissionAlertForItem =
      NO;
  mediator_.priceInsightsConsumer = price_insights_consumer_;
  [mediator_ tryPriceInsightsTrackItem:GetPriceInsightsItem()];

  ASSERT_FALSE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, ^bool {
        base::RunLoop().RunUntilIdle();
        return price_insights_consumer_.didPriceTrack;
      }));

  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
}
