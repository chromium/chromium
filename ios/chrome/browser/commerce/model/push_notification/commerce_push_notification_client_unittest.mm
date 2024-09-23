// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/commerce/model/push_notification/commerce_push_notification_client.h"

#import "base/base64.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
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
#import "components/optimization_guide/core/hints_manager.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "components/optimization_guide/proto/push_notification.pb.h"
#import "components/session_proto_db/session_proto_db.h"
#import "components/sync_bookmarks/bookmark_sync_service.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/bookmarks/model/account_bookmark_sync_service_factory.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/commerce/model/session_proto_db_factory.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {

constexpr char kHintKey[] = "https://www.merchant.com/price_drop_product";
constexpr char kBookmarkFoundHistogramName[] =
    "Commerce.PriceTracking.Untrack.BookmarkFound";
std::string kBookmarkTitle = "My product title";
uint64_t kClusterId = 12345L;
constexpr char kPayloadValue[] = "value";
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
  std::string serialized_any_escaped = base::Base64Encode(serialized_any);

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
  std::optional<commerce::ProductInfo> optional_product_info;
  optional_product_info.emplace(product_info);
  shopping_service->SetResponseForGetProductInfoForUrl(optional_product_info);
  return product;
}

}  // namespace

class MockDelegate
    : public optimization_guide::PushNotificationManager::Delegate {
 public:
  MOCK_METHOD(void,
              RemoveFetchedEntriesByHintKeys,
              (base::OnceClosure,
               optimization_guide::proto::KeyRepresentation,
               (const base::flat_set<std::string>&)),
              (override));
};

class CommercePushNotificationClientTest : public PlatformTest {
 public:
  CommercePushNotificationClientTest() = default;
  ~CommercePushNotificationClientTest() override = default;

  void SetUp() override {
    PlatformTest::SetUp();
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(ios::BookmarkModelFactory::GetInstance(),
                              ios::BookmarkModelFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        commerce::ShoppingServiceFactory::GetInstance(),
        base::BindRepeating(
            [](web::BrowserState*) -> std::unique_ptr<KeyedService> {
              return std::make_unique<
                  testing::NiceMock<commerce::MockShoppingService>>();
            }));
    builder.AddTestingFactory(
        SessionProtoDBFactory<
            commerce_subscription_db::CommerceSubscriptionContentProto>::
            GetInstance(),
        SessionProtoDBFactory<
            commerce_subscription_db::CommerceSubscriptionContentProto>::
            GetDefaultFactory());
    builder.AddTestingFactory(
        OptimizationGuideServiceFactory::GetInstance(),
        OptimizationGuideServiceFactory::GetDefaultFactory());
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));
    browser_list_ = BrowserListFactory::GetForProfile(profile_.get());
    app_state_ = [[AppState alloc] initWithStartupInformation:nil];
    scene_state_foreground_ = [[SceneState alloc] initWithAppState:app_state_];
    scene_state_foreground_.activationLevel =
        SceneActivationLevelForegroundActive;
    browser_ =
        std::make_unique<TestBrowser>(profile_.get(), scene_state_foreground_);
    scene_state_background_ = [[SceneState alloc] initWithAppState:app_state_];
    scene_state_background_.activationLevel = SceneActivationLevelBackground;
    background_browser_ =
        std::make_unique<TestBrowser>(profile_.get(), scene_state_background_);
    browser_list_->AddBrowser(browser_.get());
    bookmark_model_ = ios::BookmarkModelFactory::GetForProfile(profile_.get());
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model_);
    // Pretend account bookmark sync is on and bookmarks have been downloaded
    // from the server, required for price tracking.
    bookmark_model_->CreateAccountPermanentFolders();
    ios::AccountBookmarkSyncServiceFactory::GetForProfile(profile_.get())
        ->SetIsTrackingMetadataForTesting();
    shopping_service_ = static_cast<commerce::MockShoppingService*>(
        commerce::ShoppingServiceFactory::GetForBrowserState(profile_.get()));
    application_handler_ = OCMProtocolMock(@protocol(ApplicationCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:application_handler_
                     forProtocol:@protocol(ApplicationCommands)];
  }

  CommercePushNotificationClient* GetCommercePushNotificationClient() {
    return &commerce_push_notification_client_;
  }

  Browser* GetBrowser() { return browser_.get(); }

  ProfileIOS* GetProfile() { return profile_.get(); }

  Browser* GetBackgroundBrowser() { return background_browser_.get(); }

  void HandleNotificationInteraction(NSString* action_identifier,
                                     NSDictionary* user_info,
                                     base::OnceClosure completion) {
    commerce_push_notification_client_.HandleNotificationInteraction(
        action_identifier, user_info, std::move(completion));
  }

  std::vector<std::pair<GURL, base::OnceCallback<void(Browser*)>>>&
  GetUrlsDelayedForLoading() {
    return commerce_push_notification_client_.urls_delayed_for_loading_;
  }

  void OnSceneActiveForegroundBrowserReady() {
    commerce_push_notification_client_.OnSceneActiveForegroundBrowserReady();
  }

  Browser* GetSceneLevelForegroundActiveBrowser() {
    return commerce_push_notification_client_
        .GetSceneLevelForegroundActiveBrowser();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  CommercePushNotificationClient commerce_push_notification_client_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<Browser> background_browser_;
  raw_ptr<TestProfileIOS> profile_;
  raw_ptr<BrowserList> browser_list_;
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
  raw_ptr<commerce::MockShoppingService> shopping_service_;
  SceneState* scene_state_foreground_;
  SceneState* scene_state_background_;
  id<ApplicationCommands> application_handler_;
  AppState* app_state_;
};

TEST_F(CommercePushNotificationClientTest, TestParsing) {
  optimization_guide::proto::Any any;
  optimization_guide::proto::HintNotificationPayload hint_notification_payload;
  hint_notification_payload.set_hint_key(kHintKey);
  hint_notification_payload.set_optimization_type(
      optimization_guide::proto::NOSCRIPT);
  hint_notification_payload.set_key_representation(
      optimization_guide::proto::HOST);
  optimization_guide::proto::Any* payload =
      hint_notification_payload.mutable_payload();
  payload->set_type_url(kHintKey);
  payload->set_value(kPayloadValue);

  std::string serialized_hint_notification_payload;
  hint_notification_payload.SerializeToString(
      &serialized_hint_notification_payload);
  any.set_value(serialized_hint_notification_payload.c_str());

  std::string serialized_any;
  any.SerializeToString(&serialized_any);
  std::string serialized_any_escaped = base::Base64Encode(serialized_any);

  std::unique_ptr<optimization_guide::proto::HintNotificationPayload> parsed =
      CommercePushNotificationClient::ParseHintNotificationPayload(
          base::SysUTF8ToNSString(serialized_any_escaped));
  EXPECT_EQ(kHintKey, parsed->hint_key());
  EXPECT_EQ(optimization_guide::proto::NOSCRIPT, parsed->optimization_type());
  EXPECT_EQ(optimization_guide::proto::HOST, parsed->key_representation());
  EXPECT_EQ(kHintKey, parsed->payload().type_url());
  EXPECT_EQ(kPayloadValue, parsed->payload().value());
}

TEST_F(CommercePushNotificationClientTest, TestHintKeyRemovedUponNotification) {
  MockDelegate mock_delegate;
  OptimizationGuideService* optimization_guide_service =
      OptimizationGuideServiceFactory::GetForProfile(GetProfile());
  optimization_guide_service->GetHintsManager()
      ->push_notification_manager()
      ->SetDelegate(&mock_delegate);

  optimization_guide::proto::Any any;
  optimization_guide::proto::HintNotificationPayload hint_notification_payload;
  hint_notification_payload.set_hint_key(kHintKey);
  hint_notification_payload.set_key_representation(
      optimization_guide::proto::HOST);
  hint_notification_payload.set_optimization_type(
      optimization_guide::proto::NOSCRIPT);

  optimization_guide::proto::Any* payload =
      hint_notification_payload.mutable_payload();
  payload->set_type_url(kHintKey);
  payload->set_value(kPayloadValue);

  std::string serialized_hint_notification_payload;
  hint_notification_payload.SerializeToString(
      &serialized_hint_notification_payload);
  any.set_value(serialized_hint_notification_payload.c_str());

  std::string serialized_any;
  any.SerializeToString(&serialized_any);
  std::string serialized_any_escaped = base::Base64Encode(serialized_any);

  NSDictionary* dict = @{
    kSerializedPayloadKey : base::SysUTF8ToNSString(serialized_any_escaped)
  };

  CommercePushNotificationClient push_notification_client;

  EXPECT_CALL(mock_delegate,
              RemoveFetchedEntriesByHintKeys(
                  testing::_, testing::Eq(optimization_guide::proto::HOST),
                  testing::ElementsAreArray({kHintKey})));
  push_notification_client.HandleNotificationReception(dict);
}

TEST_F(CommercePushNotificationClientTest, TestNotificationInteraction) {
  NSDictionary* user_info = SerializeOptGuideCommercePayload();

  // Simulate user clicking 'visit site'.
  HandleNotificationInteraction(kVisitSiteActionId, user_info,
                                base::DoNothing());

  // Check PriceDropNotification Destination URL loaded.
  OCMExpect([application_handler_
      openURLInNewTab:[OCMArg checkWithBlock:^(OpenNewTabCommand* command) {
        return kHintKey == command.URL;
      }]]);
}

TEST_F(CommercePushNotificationClientTest, TestActionableNotifications) {
  NSArray<UNNotificationCategory*>* actionable_notifications =
      GetCommercePushNotificationClient()->RegisterActionableNotifications();
  EXPECT_EQ(1u, [actionable_notifications count]);
  UNNotificationCategory* notification_category = actionable_notifications[0];
  EXPECT_EQ(2u, [notification_category.actions count]);
  EXPECT_NSEQ(notification_category.actions[0].identifier, kVisitSiteActionId);
  EXPECT_NSEQ(notification_category.actions[0].title, kVisitSiteTitle);
  EXPECT_NSEQ(notification_category.actions[1].identifier,
              kUntrackPriceActionId);
  EXPECT_NSEQ(notification_category.actions[1].title, kUntrackPriceTitle);
}

TEST_F(CommercePushNotificationClientTest, TestUntrackPrice) {
  PrepareSubscription(shopping_service_, bookmark_model_, true);
  NSDictionary* user_info = SerializeOptGuideCommercePayload();
  base::RunLoop run_loop;
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*shopping_service_, Unsubscribe(testing::_, testing::_)).Times(1);

  // Simulate user clicking 'visit site'.
  HandleNotificationInteraction(kUntrackPriceActionId, user_info,
                                run_loop.QuitClosure());
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
  HandleNotificationInteraction(kUntrackPriceActionId, user_info,
                                run_loop.QuitClosure());
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
  HandleNotificationInteraction(kUntrackPriceActionId, user_info,
                                run_loop.QuitClosure());
  run_loop.Run();
  histogram_tester.ExpectBucketCount(kUntrackSuccessHistogramName,
                                     /*sample=*/false, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(kBookmarkFoundHistogramName,
                                     /*sample=*/true, /*expected_count=*/1);
}

TEST_F(CommercePushNotificationClientTest, TestBrowserInitialization) {
  browser_list_->RemoveBrowser(GetBrowser());
  NSDictionary* user_info = SerializeOptGuideCommercePayload();

  // Simulate user clicking 'visit site'.
  HandleNotificationInteraction(kVisitSiteActionId, user_info,
                                base::DoNothing());
  EXPECT_EQ(1u, GetUrlsDelayedForLoading().size());
  CommercePushNotificationClient* commerce_push_notification_client =
      GetCommercePushNotificationClient();
  browser_list_->AddBrowser(GetBrowser());
  commerce_push_notification_client->OnSceneActiveForegroundBrowserReady();
  EXPECT_EQ(0u, GetUrlsDelayedForLoading().size());

  // Check PriceDropNotification Destination URL loaded.
  OCMExpect([application_handler_
      openURLInNewTab:[OCMArg checkWithBlock:^(OpenNewTabCommand* command) {
        return kHintKey == command.URL;
      }]]);
}

TEST_F(CommercePushNotificationClientTest,
       TestBackgroundBrowserNotUsedWhenForegroundAvailable) {
  browser_list_->AddBrowser(GetBackgroundBrowser());
  Browser* browser = GetSceneLevelForegroundActiveBrowser();
  // When active foregrounded and active backgrounded browser is availalbe,
  // should choose foregrounded browser.
  EXPECT_EQ(SceneActivationLevelForegroundActive,
            browser->GetSceneState().activationLevel);
}

TEST_F(CommercePushNotificationClientTest, TestBackgroundFallback) {
  // Remove foregrounded browser
  browser_list_->RemoveBrowser(GetBrowser());
  // Add backgrounded browser
  browser_list_->AddBrowser(GetBackgroundBrowser());
  // Background browser not used.
  EXPECT_EQ(nullptr, GetSceneLevelForegroundActiveBrowser());
}
