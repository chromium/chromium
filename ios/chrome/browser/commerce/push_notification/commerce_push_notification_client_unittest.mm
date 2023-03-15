// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/commerce/push_notification/commerce_push_notification_client.h"

#import "base/base64.h"
#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/test/bookmark_test_helpers.h"
#import "components/bookmarks/test/test_bookmark_client.h"
#import "components/commerce/core/mock_shopping_service.h"
#import "components/commerce/core/price_tracking_utils.h"
#import "components/commerce/core/proto/commerce_subscription_db_content.pb.h"
#import "components/commerce/core/proto/price_tracking.pb.h"
#import "components/commerce/core/test_utils.h"
#import "components/optimization_guide/proto/push_notification.pb.h"
#import "components/session_proto_db/session_proto_db.h"
#import "ios/chrome/browser/bookmarks/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/commerce/session_proto_db_factory.h"
#import "ios/chrome/browser/commerce/shopping_service_factory.h"
#import "ios/chrome/browser/main/browser_list.h"
#import "ios/chrome/browser/main/browser_list_factory.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/url_loading/fake_url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_notifier_browser_agent.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

constexpr char kHintKey[] = "https://www.merchant.com/price_drop_product";
constexpr char kBookmarkFoundHistogramName[] =
    "Commerce.PriceTracking.Untrack.BookmarkFound";
std::string kBookmarkTitle = "My product title";
uint64_t kClusterId = 12345L;
NSString* kSerializedPayloadKey = @"op";
NSString* kVisitSiteActionId = @"visit_site";
NSString* kVisitSiteTitle = @"Visit site";
NSString* kUntrackPriceActionId = @"untrack_price";
NSString* kUntrackPriceTitle = @"Untrack price";
constexpr char kUntrackSuccessHistogramName[] =
    "Commerce.PriceTracking.Untrack.Success";

NSDictionary* SerializeOptGuideCommercePayload() {
  // Serialized PriceDropNotificationPayload
  commerce::PriceDropNotificationPayload price_drop_notification;
  price_drop_notification.set_destination_url(kHintKey);
  std::string serialized_price_drop_notification;
  price_drop_notification.SerializeToString(
      &serialized_price_drop_notification);

  // Serialized HintNotificationPayload with PriceDropNotificationPayload
  // injected.
  optimization_guide::proto::HintNotificationPayload hint_notification_payload;
  hint_notification_payload.set_hint_key(kHintKey);
  hint_notification_payload.set_optimization_type(
      optimization_guide::proto::PRICE_TRACKING);
  hint_notification_payload.set_key_representation(
      optimization_guide::proto::HOST);
  optimization_guide::proto::Any* payload =
      hint_notification_payload.mutable_payload();
  payload->set_type_url(kHintKey);
  payload->set_value(serialized_price_drop_notification.c_str());
  std::string serialized_hint_notification_payload;
  hint_notification_payload.SerializeToString(
      &serialized_hint_notification_payload);

  // Serialized Any with HintNotificationPayload injected
  optimization_guide::proto::Any any;
  any.set_value(serialized_hint_notification_payload.c_str());
  std::string serialized_any;
  any.SerializeToString(&serialized_any);

  // Base 64 encoding
  std::string serialized_any_escaped;
  base::Base64Encode(serialized_any, &serialized_any_escaped);

  NSDictionary* user_info = @{
    kSerializedPayloadKey : base::SysUTF8ToNSString(serialized_any_escaped)
  };
  return user_info;
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
  const bookmarks::BookmarkNode* product = commerce::AddProductBookmark(
      bookmark_model, base::UTF8ToUTF16(kBookmarkTitle), GURL(kHintKey),
      kClusterId, true);
  shopping_service->SetSubscribeCallbackValue(true);
  shopping_service->SetUnsubscribeCallbackValue(unsubscribe_callback);
  TrackBookmark(shopping_service, bookmark_model, product);

  commerce::ProductInfo product_info;
  product_info.title = kBookmarkTitle;
  absl::optional<commerce::ProductInfo> optional_product_info;
  optional_product_info.emplace(product_info);
  shopping_service->SetResponseForGetProductInfoForUrl(optional_product_info);
  return product;
}

}  // namespace

class CommercePushNotificationClientTest : public PlatformTest {
 public:
  CommercePushNotificationClientTest() {}
  ~CommercePushNotificationClientTest() override = default;

  void SetUp() override {
    PlatformTest::SetUp();
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        ios::LocalOrSyncableBookmarkModelFactory::GetInstance(),
        ios::LocalOrSyncableBookmarkModelFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        commerce::ShoppingServiceFactory::GetInstance(),
        base::BindRepeating(
            [](web::BrowserState*) -> std::unique_ptr<KeyedService> {
              return std::make_unique<commerce::MockShoppingService>();
            }));
    builder.AddTestingFactory(
        SessionProtoDBFactory<
            commerce_subscription_db::CommerceSubscriptionContentProto>::
            GetInstance(),
        SessionProtoDBFactory<
            commerce_subscription_db::CommerceSubscriptionContentProto>::
            GetDefaultFactory());
    chrome_browser_state_ = builder.Build();
    browser_list_ =
        BrowserListFactory::GetForBrowserState(chrome_browser_state_.get());
    browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get());
    browser_list_->AddBrowser(browser_.get());
    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    FakeUrlLoadingBrowserAgent::InjectForBrowser(browser_.get());
    commerce_push_notification_client_.SetLastUsedChromeBrowserStateForTesting(
        chrome_browser_state_.get());
    bookmark_model_ =
        ios::LocalOrSyncableBookmarkModelFactory::GetForBrowserState(
            chrome_browser_state_.get());
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model_);
    shopping_service_ = static_cast<commerce::MockShoppingService*>(
        commerce::ShoppingServiceFactory::GetForBrowserState(
            chrome_browser_state_.get()));
  }

  CommercePushNotificationClient* GetCommercePushNotificationClient() {
    return &commerce_push_notification_client_;
  }

  Browser* GetBrowser() { return browser_.get(); }

  void HandleNotificationInteraction(
      NSString* action_identifier,
      NSDictionary* user_info,
      base::RunLoop* on_complete_for_testing = nil) {
    commerce_push_notification_client_.HandleNotificationInteraction(
        action_identifier, user_info, on_complete_for_testing);
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  CommercePushNotificationClient commerce_push_notification_client_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  BrowserList* browser_list_;
  bookmarks::BookmarkModel* bookmark_model_;
  commerce::MockShoppingService* shopping_service_;
};

TEST_F(CommercePushNotificationClientTest, TestNotificationInteraction) {
  NSDictionary* user_info = SerializeOptGuideCommercePayload();

  // Simulate user clicking 'visit site'.
  HandleNotificationInteraction(kVisitSiteActionId, user_info);

  // Check PriceDropNotification Destination URL loaded.
  FakeUrlLoadingBrowserAgent* url_loader =
      FakeUrlLoadingBrowserAgent::FromUrlLoadingBrowserAgent(
          UrlLoadingBrowserAgent::FromBrowser(GetBrowser()));
  EXPECT_EQ(kHintKey, url_loader->last_params.web_params.url);
}

TEST_F(CommercePushNotificationClientTest, TestActionableNotifications) {
  NSArray<UNNotificationCategory*>* actionable_notifications =
      GetCommercePushNotificationClient()->RegisterActionableNotifications();
  EXPECT_EQ(1u, [actionable_notifications count]);
  UNNotificationCategory* notification_category = actionable_notifications[0];
  EXPECT_EQ(2u, [notification_category.actions count]);
  EXPECT_TRUE([notification_category.actions[0].identifier
      isEqualToString:kVisitSiteActionId]);
  EXPECT_TRUE(
      [notification_category.actions[0].title isEqualToString:kVisitSiteTitle]);
  EXPECT_TRUE([notification_category.actions[1].identifier
      isEqualToString:kUntrackPriceActionId]);
  EXPECT_TRUE([notification_category.actions[1].title
      isEqualToString:kUntrackPriceTitle]);
}

TEST_F(CommercePushNotificationClientTest, TestUntrackPrice) {
  PrepareSubscription(shopping_service_, bookmark_model_, true);
  NSDictionary* user_info = SerializeOptGuideCommercePayload();
  base::RunLoop run_loop;
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*shopping_service_, Unsubscribe(testing::_, testing::_)).Times(1);

  // Simulate user clicking 'visit site'.
  HandleNotificationInteraction(kUntrackPriceActionId, user_info, &run_loop);
  run_loop.Run();
  histogram_tester.ExpectBucketCount(kBookmarkFoundHistogramName,
                                     /*sample=*/true, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(kUntrackSuccessHistogramName,
                                     /*sample=*/true, /*expected_count=*/1);
}

TEST_F(CommercePushNotificationClientTest, TestNoBookmarkFound) {
  // No bookmark added so
  // GetBookmarkModel()->GetMostRecentlyAddedUserNodeForURL() returns nil.
  NSDictionary* user_info = SerializeOptGuideCommercePayload();
  base::RunLoop run_loop;
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*shopping_service_, Unsubscribe(testing::_, testing::_)).Times(0);

  // Simulate user clicking 'visit site'.
  HandleNotificationInteraction(kUntrackPriceActionId, user_info, &run_loop);
  run_loop.Run();
  histogram_tester.ExpectBucketCount(kBookmarkFoundHistogramName,
                                     /*sample=*/false, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(kUntrackSuccessHistogramName,
                                     /*sample=*/false, /*expected_count=*/0);
}

TEST_F(CommercePushNotificationClientTest, TestUntrackPriceFailed) {
  PrepareSubscription(shopping_service_, bookmark_model_, false);
  NSDictionary* user_info = SerializeOptGuideCommercePayload();
  base::RunLoop run_loop;
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*shopping_service_, Unsubscribe(testing::_, testing::_)).Times(1);

  // Simulate user clicking 'visit site'.
  HandleNotificationInteraction(kUntrackPriceActionId, user_info, &run_loop);
  run_loop.Run();
  histogram_tester.ExpectBucketCount(kUntrackSuccessHistogramName,
                                     /*sample=*/false, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(kBookmarkFoundHistogramName,
                                     /*sample=*/true, /*expected_count=*/1);
}
