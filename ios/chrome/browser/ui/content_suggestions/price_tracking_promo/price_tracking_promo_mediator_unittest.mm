// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_mediator.h"

#import <MaterialComponents/MaterialSnackbar.h>
#import <UserNotifications/UserNotifications.h>

#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/test/test_bookmark_client.h"
#import "components/commerce/core/mock_shopping_service.h"
#import "components/commerce/core/pref_names.h"
#import "components/image_fetcher/core/image_data_fetcher.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_item.h"
#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_mediator+testing.h"
#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_prefs.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/public/provider/chrome/browser/push_notification/push_notification_api.h"
#import "ios/testing/scoped_block_swizzler.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class PriceTrackingPromoMediatorTest : public PlatformTest {
 public:
  PriceTrackingPromoMediatorTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();
    AuthenticationServiceFactory::CreateAndInitializeForProfile(
        profile_.get(), std::make_unique<FakeAuthenticationServiceDelegate>());
    auth_service_ = static_cast<AuthenticationService*>(
        AuthenticationServiceFactory::GetInstance()->GetForProfile(
            profile_.get()));
    identity_ = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(identity_);

    auth_service_->SignIn(identity_,
                          signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);

    shopping_service_ = std::make_unique<commerce::MockShoppingService>();
    bookmark_model_ = bookmarks::TestBookmarkClient::CreateModel();
    push_notification_service_ = ios::provider::CreatePushNotificationService();
    pref_service_.registry()->RegisterDictionaryPref(
        prefs::kFeaturePushNotificationPermissions);
    local_state_.registry()->RegisterDictionaryPref(
        prefs::kAppLevelPushNotificationPermissions);

    mediator_ = [[PriceTrackingPromoMediator alloc]
        initWithShoppingService:shopping_service_.get()
                  bookmarkModel:bookmark_model_.get()
                   imageFetcher:std::make_unique<
                                    image_fetcher::ImageDataFetcher>(
                                    profile_->GetSharedURLLoaderFactory())
                    prefService:pref_service()
                     localState:&local_state_
        pushNotificationService:push_notification_service_.get()
          authenticationService:auth_service_];
    // Mock notifications settings response.
    mock_notification_center_ = OCMClassMock([UNUserNotificationCenter class]);
    UNUserNotificationCenter* (^swizzle_block)() =
        ^UNUserNotificationCenter*() {
          return mock_notification_center_;
        };
    notification_center_swizzler_ = std::make_unique<ScopedBlockSwizzler>(
        [UNUserNotificationCenter class], @selector(currentNotificationCenter),
        swizzle_block);
    pref_service_.registry()->RegisterBooleanPref(
        commerce::kPriceEmailNotificationsEnabled, false);
    pref_service_.registry()->RegisterBooleanPref(kPriceTrackingPromoDisabled,
                                                  false);
  }

  ~PriceTrackingPromoMediatorTest() override {}

  void TearDown() override {
    pref_service_.ClearPref(prefs::kFeaturePushNotificationPermissions);
    pref_service_.ClearPref(commerce::kPriceEmailNotificationsEnabled);
    pref_service_.ClearPref(kPriceTrackingPromoDisabled);
    local_state_.ClearPref(prefs::kAppLevelPushNotificationPermissions);
    [mediator_ disconnect];
  }

  PriceTrackingPromoMediator* mediator() { return mediator_; }

  PrefService* pref_service() { return &pref_service_; }

  PushNotificationService* push_notification_service() {
    return push_notification_service_.get();
  }

  NSString* gaia_id() { return identity_.gaiaID; }

  bookmarks::BookmarkModel* bookmark_model() { return bookmark_model_.get(); }

 protected:
  std::unique_ptr<PushNotificationService> push_notification_service_;
  TestingPrefServiceSimple pref_service_;
  TestingPrefServiceSimple local_state_;
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<AuthenticationService> auth_service_;
  id<SystemIdentity> identity_;
  std::unique_ptr<commerce::MockShoppingService> shopping_service_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
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
  EXPECT_NE(nil, mediator().bookmarkModelForTesting);
  EXPECT_NE(nil, mediator().prefServiceForTesting);
  EXPECT_NE(nil, mediator().pushNotificationServiceForTesting);
  EXPECT_NE(nil, mediator().authenticationServiceForTesting);
  EXPECT_NE(nil, mediator().imageFetcherForTesting);
  EXPECT_NE(nil, mediator().notificationsSettingsObserverForTesting);
  [mediator() disconnect];
  EXPECT_EQ(nil, mediator().shoppingServiceForTesting);
  EXPECT_EQ(nil, mediator().bookmarkModelForTesting);
  EXPECT_EQ(nil, mediator().prefServiceForTesting);
  EXPECT_EQ(nil, mediator().pushNotificationServiceForTesting);
  EXPECT_EQ(nil, mediator().authenticationServiceForTesting);
  EXPECT_EQ(nil, mediator().imageFetcherForTesting);
  EXPECT_EQ(nil, mediator().notificationsSettingsObserverForTesting);
}

// Resets card and fetches most recent subscription, if available.
TEST_F(PriceTrackingPromoMediatorTest, TestReset) {
  PriceTrackingPromoItem* item = [[PriceTrackingPromoItem alloc] init];
  [mediator() setPriceTrackingPromoItemForTesting:item];
  EXPECT_NE(nil, mediator().priceTrackingPromoItemForTesting);
  [mediator() reset];
  EXPECT_EQ(nil, mediator().priceTrackingPromoItemForTesting);
}

TEST_F(PriceTrackingPromoMediatorTest, TestGetSnackbarMessage) {
  MDCSnackbarMessage* snackbarMessage = [mediator() snackbarMessageForTesting];
  EXPECT_NSEQ(@"Price tracking notifications turned on", snackbarMessage.text);
  EXPECT_NSEQ(@"Manage", snackbarMessage.action.title);
}

TEST_F(PriceTrackingPromoMediatorTest, TestPriceTrackingSettings) {
  // TODO(crbug.com/367801170) Make test infrastructure for
  // PushNotificationService work for SetPreference and update test to include
  // that as well.
  EXPECT_FALSE(
      pref_service()->GetBoolean(commerce::kPriceEmailNotificationsEnabled));
  [mediator() enablePriceTrackingNotificationsSettingsForTesting];
  EXPECT_TRUE(
      pref_service()->GetBoolean(commerce::kPriceEmailNotificationsEnabled));
}

TEST_F(PriceTrackingPromoMediatorTest, TestRemovePriceTrackingPromo) {
  id mockDelegate =
      OCMStrictProtocolMock(@protocol(PriceTrackingPromoMediatorDelegate));
  OCMExpect([mockDelegate removePriceTrackingPromo]);
  mediator().delegate = mockDelegate;
  [mediator() removePriceTrackingPromo];
}

TEST_F(PriceTrackingPromoMediatorTest,
       TestEnablePriceTrackingSettingsAndShowSnackbar) {
  EXPECT_FALSE(
      pref_service()->GetBoolean(commerce::kPriceEmailNotificationsEnabled));
  [mediator() enablePriceTrackingSettingsAndShowSnackbar];
  EXPECT_TRUE(
      pref_service()->GetBoolean(commerce::kPriceEmailNotificationsEnabled));
  id mockDispatcher = OCMStrictProtocolMock(@protocol(SnackbarCommands));
  mediator().dispatcher = mockDispatcher;
  OCMExpect([mockDispatcher showSnackbarMessage:[OCMArg isNotNil]]);
  [mediator() enablePriceTrackingSettingsAndShowSnackbar];
}

TEST_F(PriceTrackingPromoMediatorTest, TestDenyPriceTrackingNotifications) {
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
  EXPECT_FALSE(pref_service()->GetBoolean(kPriceTrackingPromoDisabled));
  [mediator() requestPushNotificationDoneWithGrantedForTesting:NO
                                                   promptShown:YES
                                                         error:nil];
  EXPECT_TRUE(pref_service()->GetBoolean(kPriceTrackingPromoDisabled));
}

TEST_F(PriceTrackingPromoMediatorTest, TestPriceTrackingPromoDisabled) {
  id mockDelegate =
      OCMStrictProtocolMock(@protocol(PriceTrackingPromoMediatorDelegate));
  OCMExpect([mockDelegate removePriceTrackingPromo]);
  mediator().delegate = mockDelegate;
  pref_service()->SetBoolean(kPriceTrackingPromoDisabled, true);
}
