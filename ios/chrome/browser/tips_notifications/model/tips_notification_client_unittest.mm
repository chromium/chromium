// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tips_notifications/model/tips_notification_client.h"

#import <UserNotifications/UserNotifications.h>

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/scoped_mock_clock_override.h"
#import "base/threading/thread_restrictions.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_presenter.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_commands.h"
#import "ios/chrome/browser/default_browser/model/promo_source.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/default_browser/model/utils_test_support.h"
#import "ios/chrome/browser/first_run/model/first_run.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/credential_provider_promo_commands.h"
#import "ios/chrome/browser/shared/public/commands/docking_promo_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/whats_new_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/tips_notifications/model/utils.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/testing/scoped_block_swizzler.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/device_form_factor.h"

using startup_metric_utils::FirstRunSentinelCreationResult;

// A simple class that stubs `PrepareToPresentModal:` by immediately calling
// the provided `completion` callback.
@interface PrepareToPresentModalStub : NSObject
@end

@implementation PrepareToPresentModalStub
- (void)prepareToPresentModalWithSnackbarDismissal:(BOOL)dismissSnackbars
                                        completion:(ProceduralBlock)completion {
  completion();
}
@end

class TipsNotificationClientTest : public PlatformTest {
 protected:
  TipsNotificationClientTest() {
    [[NSUserDefaults standardUserDefaults]
        removeObjectForKey:@"TipsNotificationTrigger"];
    SetupMockNotificationCenter();
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));
    BrowserList* list = BrowserListFactory::GetForProfile(profile_);
    mock_scene_state_ = OCMClassMock([SceneState class]);
    OCMStub([mock_scene_state_ activationLevel])
        .andReturn(SceneActivationLevelForegroundActive);
    browser_ = std::make_unique<TestBrowser>(profile_, mock_scene_state_);
    list->AddBrowser(browser_.get());
    client_ = std::make_unique<TipsNotificationClient>();
    ScopedDictPrefUpdate update(GetApplicationContext()->GetLocalState(),
                                prefs::kAppLevelPushNotificationPermissions);
    update->Set(kTipsNotificationKey, true);
  }

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

  std::string_view GetProfileName() {
    return browser_->GetProfile()->GetProfileName();
  }

  // Writes the first run sentinel file, to allow notifications to be
  // registered.
  void WriteFirstRunSentinel() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    FirstRun::RemoveSentinel();
    base::File::Error file_error = base::File::FILE_OK;
    FirstRunSentinelCreationResult sentinel_created =
        FirstRun::CreateSentinel(&file_error);
    ASSERT_EQ(sentinel_created, FirstRunSentinelCreationResult::kSuccess)
        << "Error creating FirstRun sentinel: "
        << base::File::ErrorToString(file_error);
    FirstRun::LoadSentinelInfo();
    FirstRun::ClearStateForTesting();
  }

  // Returns an OCMArg that verifies a UNNotificationRequest was passed for the
  // given notification `type`.
  id NotificationRequestArg(TipsNotificationType type) {
    return [OCMArg checkWithBlock:^BOOL(UNNotificationRequest* request) {
      TipsNotificationType requested_type =
          ParseTipsNotificationType(request).value();
      EXPECT_TRUE(IsTipsNotification(request));
      EXPECT_EQ(requested_type, type);
      return YES;
    }];
  }

  // Returns a mock UNNotificationResponse for the given notification `type`.
  id MockRequestResponse(TipsNotificationType type,
                         bool for_reactivation = false) {
    id mock_response = OCMClassMock([UNNotificationResponse class]);
    OCMStub([mock_response notification])
        .andReturn(MockNotification(type, for_reactivation));
    return mock_response;
  }

  // Returns a mock UNNotification for the given notification `type`.
  id MockNotification(TipsNotificationType type, bool for_reactivation) {
    UNNotificationRequest* request = [UNNotificationRequest
        requestWithIdentifier:kTipsNotificationId
                      content:ContentForTipsNotificationType(
                                  type, for_reactivation, GetProfileName())
                      trigger:[UNTimeIntervalNotificationTrigger
                                  triggerWithTimeInterval:
                                      TipsNotificationTriggerDelta(
                                          for_reactivation,
                                          TipsNotificationUserType::kUnknown)
                                          .InSecondsF()
                                                  repeats:NO]];
    id mock_notification = OCMClassMock([UNNotification class]);
    OCMStub([mock_notification request]).andReturn(request);
    return mock_notification;
  }

  // Stubs the notification center's completion callback for
  // getPendingNotificationRequestsWithCompletionHandler.
  void StubGetPendingRequests(NSArray<UNNotificationRequest*>* requests) {
    auto completionCaller =
        ^BOOL(void (^completion)(NSArray<UNNotificationRequest*>* requests)) {
          completion(requests);
          return YES;
        };
    OCMStub([mock_notification_center_
        getPendingNotificationRequestsWithCompletionHandler:
            [OCMArg checkWithBlock:completionCaller]]);
  }

  // Stubs the notification center's completion callback for
  // getPendingNotificationRequestsWithCompletionHandler.
  void StubGetDeliveredNotifications(NSArray<UNNotification*>* notifications) {
    auto completionCaller =
        ^BOOL(void (^completion)(NSArray<UNNotification*>* notifications)) {
          completion(notifications);
          return YES;
        };
    OCMStub([mock_notification_center_
        getDeliveredNotificationsWithCompletionHandler:
            [OCMArg checkWithBlock:completionCaller]]);
  }

  // Clears the pref used to store which notification types have already been
  // sent.
  void ClearSentNotifications() {
    GetApplicationContext()->GetLocalState()->SetInteger(
        kTipsNotificationsSentPref, 0);
  }

  // Sets the pref used to store which notification types have been sent.
  void SetSentNotifications(std::vector<TipsNotificationType> types) {
    int bits = 0;
    for (TipsNotificationType type : types) {
      bits |= 1 << int(type);
    }
    GetApplicationContext()->GetLocalState()->SetInteger(
        kTipsNotificationsSentPref, bits);
  }

  // Stubs the `-prepareToPresentModalWithSnackbarDismissal:` method from
  // `ApplicationCommands` so that it immediately calls the completion block.
  void StubPrepareToPresentModal() {
    prepare_to_present_modal_stub_ = [[PrepareToPresentModalStub alloc] init];
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:prepare_to_present_modal_stub_
                     forProtocol:@protocol(ApplicationCommands)];
  }

  // Sets up an OCMock expectation that a notification will be requested.
  void ExpectNotificationRequest(TipsNotificationType type) {
    ExpectNotificationRequest(NotificationRequestArg(type));
  }
  void ExpectNotificationRequest(id request) {
    auto completionCaller = ^BOOL(void (^completion)(NSError* error)) {
      completion(nil);
      return YES;
    };
    OCMExpect([mock_notification_center_
        addNotificationRequest:request
         withCompletionHandler:[OCMArg checkWithBlock:completionCaller]]);
  }

  // Ensures that Chrome is considered as default browser.
  void SetTrueChromeLikelyDefaultBrowser() { LogOpenHTTPURLFromExternalURL(); }

  // Ensures that Chrome is not considered as default browser.
  void SetFalseChromeLikelyDefaultBrowser() { ClearDefaultBrowserPromoData(); }

  // Clears the pref that stores the last action the user took with a Default
  // Browser promo.
  void ClearDefaultBrowserPromoLastAction() {
    GetApplicationContext()->GetLocalState()->ClearPref(
        prefs::kIosDefaultBrowserPromoLastAction);
  }

  // Creates a mock command handler and starts dispatching to it.
  id MockHandler(Protocol* protocol) {
    id mock_handler = OCMProtocolMock(protocol);
    [browser_->GetCommandDispatcher() startDispatchingToTarget:mock_handler
                                                   forProtocol:protocol];
    return mock_handler;
  }

  // Simulates foregrounding the app by calling the client's
  // OnSceneActiveForegroundBrowserReady method.
  void SimulateForegroundingApp() {
    base::RunLoop run_loop;
    client_->OnSceneActiveForegroundBrowserReady(run_loop.QuitClosure());
    run_loop.Run();
  }

  // Returns the user's type stored in local state prefs.
  TipsNotificationUserType GetUserType() {
    PrefService* local_state = GetApplicationContext()->GetLocalState();
    return static_cast<TipsNotificationUserType>(
        local_state->GetInteger(kTipsNotificationsUserType));
  }

  // Signs in with `fakeIdentity1`.
  void SigninWithFakeIdentity() {
    FakeSystemIdentityManager* identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    AuthenticationService* authentication_service =
        AuthenticationServiceFactory::GetForProfile(profile_.get());
    identity_manager->AddIdentity([FakeSystemIdentity fakeIdentity1]);
    authentication_service->SignIn([FakeSystemIdentity fakeIdentity1],
                                   signin_metrics::AccessPoint::kUnknown);
  }

  web::WebTaskEnvironment task_environment_;
  const base::HistogramTester histogram_tester_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  id mock_scene_state_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<TipsNotificationClient> client_;
  id mock_notification_center_;
  std::unique_ptr<ScopedBlockSwizzler> notification_center_swizzler_;
  raw_ptr<ProfileIOS> profile_;
  PrepareToPresentModalStub* prepare_to_present_modal_stub_;
};

#pragma mark - Test cases

// Tests that HandleNotificationReception does nothing and returns "NoData".
TEST_F(TipsNotificationClientTest, HandleNotificationReception) {
  EXPECT_EQ(client_->HandleNotificationReception(nil), std::nullopt);
  NSDictionary* user_info = UserInfoForTipsNotificationType(
      TipsNotificationType::kWhatsNew, false, GetProfileName());
  EXPECT_EQ(client_->HandleNotificationReception(user_info),
            UIBackgroundFetchResultNoData);
}

// Tests that RegisterActionalableNotifications returns an empty array.
TEST_F(TipsNotificationClientTest, RegisterActionableNotifications) {
  EXPECT_EQ(client_->RegisterActionableNotifications().count, 0u);
}

// Tests that the client can request a Default Browser notification.
TEST_F(TipsNotificationClientTest, DefaultBrowserRequest) {
  WriteFirstRunSentinel();
  SetFalseChromeLikelyDefaultBrowser();
  ClearDefaultBrowserPromoLastAction();
  StubGetPendingRequests(nil);
  SetSentNotifications(
      {TipsNotificationType::kSetUpListContinuation,
       TipsNotificationType::kWhatsNew, TipsNotificationType::kLens,
       TipsNotificationType::kOmniboxPosition,
       TipsNotificationType::kEnhancedSafeBrowsing,
       TipsNotificationType::kDocking, TipsNotificationType::kSignin});

  ExpectNotificationRequest(TipsNotificationType::kDefaultBrowser);
  base::RunLoop run_loop;
  client_->OnSceneActiveForegroundBrowserReady(run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  histogram_tester_.ExpectUniqueSample(
      "IOS.Notifications.Tips.Sent", TipsNotificationType::kDefaultBrowser, 1);

  // Run again, but this time simulating a delivered notification.
  NSMutableArray<UNNotification*>* delivered_notifications = [NSMutableArray
      arrayWithObject:MockNotification(TipsNotificationType::kDefaultBrowser,
                                       false)];
  StubGetDeliveredNotifications(delivered_notifications);
  base::RunLoop run_loop_2;
  client_->OnSceneActiveForegroundBrowserReady(run_loop_2.QuitClosure());
  run_loop_2.Run();
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  histogram_tester_.ExpectUniqueSample("IOS.Notifications.Tips.Triggered",
                                       TipsNotificationType::kDefaultBrowser,
                                       1);

  // Run again, but this time the delivered notification is gone.
  [delivered_notifications removeAllObjects];
  base::RunLoop run_loop_3;
  client_->OnSceneActiveForegroundBrowserReady(run_loop_3.QuitClosure());
  run_loop_3.Run();
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  histogram_tester_.ExpectUniqueSample("IOS.Notifications.Tips.Dismissed",
                                       TipsNotificationType::kDefaultBrowser,
                                       1);
}

// Tests that the client handles a Default Browser notification response.
TEST_F(TipsNotificationClientTest, DefaultBrowserHandle) {
  StubPrepareToPresentModal();
  id mock_handler = MockHandler(@protocol(SettingsCommands));
  OCMExpect([mock_handler
      showDefaultBrowserSettingsFromViewController:nil
                                      sourceForUMA:
                                          DefaultBrowserSettingsPageSource::
                                              kTipsNotification]);

  id mock_response = MockRequestResponse(TipsNotificationType::kDefaultBrowser);
  client_->HandleNotificationInteraction(mock_response);

  EXPECT_OCMOCK_VERIFY(mock_handler);
  histogram_tester_.ExpectUniqueSample("IOS.Notifications.Tips.Interaction",
                                       TipsNotificationType::kDefaultBrowser,
                                       1);
}

// Tests that the client can request a Sign-in notification.
TEST_F(TipsNotificationClientTest, SigninRequest) {
  WriteFirstRunSentinel();
  StubGetPendingRequests(nil);
  SetSentNotifications(
      {TipsNotificationType::kSetUpListContinuation,
       TipsNotificationType::kWhatsNew, TipsNotificationType::kLens,
       TipsNotificationType::kOmniboxPosition,
       TipsNotificationType::kEnhancedSafeBrowsing,
       TipsNotificationType::kDocking, TipsNotificationType::kDefaultBrowser});

  ExpectNotificationRequest(TipsNotificationType::kSignin);
  base::RunLoop run_loop;
  client_->OnSceneActiveForegroundBrowserReady(run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  histogram_tester_.ExpectUniqueSample("IOS.Notifications.Tips.Sent",
                                       TipsNotificationType::kSignin, 1);

  // Run again, but this time simulating that the user is signed in, so no
  // notification will be requested.
  SigninWithFakeIdentity();
  SetSentNotifications(
      {TipsNotificationType::kSetUpListContinuation,
       TipsNotificationType::kWhatsNew, TipsNotificationType::kLens,
       TipsNotificationType::kOmniboxPosition,
       TipsNotificationType::kEnhancedSafeBrowsing,
       TipsNotificationType::kDocking, TipsNotificationType::kDefaultBrowser});
  base::RunLoop run_loop_2;
  client_->OnSceneActiveForegroundBrowserReady(run_loop_2.QuitClosure());
  run_loop_2.Run();
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  histogram_tester_.ExpectUniqueSample("IOS.Notifications.Tips.Sent",
                                       TipsNotificationType::kSignin, 1);
}

// Tests that the client handles a SignIn notification response.
TEST_F(TipsNotificationClientTest, SigninHandle) {
  StubPrepareToPresentModal();
  id mock_handler = MockHandler(@protocol(SigninPresenter));
  OCMExpect([mock_handler showSignin:[OCMArg any]]);

  id mock_response = MockRequestResponse(TipsNotificationType::kSignin);
  client_->HandleNotificationInteraction(mock_response);

  EXPECT_OCMOCK_VERIFY(mock_handler);
  histogram_tester_.ExpectUniqueSample("IOS.Notifications.Tips.Interaction",
                                       TipsNotificationType::kSignin, 1);

  SigninWithFakeIdentity();
  mock_handler = MockHandler(@protocol(SettingsCommands));
  OCMExpect([mock_handler showAccountsSettingsFromViewController:nil
                                            skipIfUINotAvailable:NO]);
  client_->HandleNotificationInteraction(mock_response);

  EXPECT_OCMOCK_VERIFY(mock_handler);
  histogram_tester_.ExpectUniqueSample("IOS.Notifications.Tips.Interaction",
                                       TipsNotificationType::kSignin, 2);
}

// Tests that the client can register a Whats New notification.
TEST_F(TipsNotificationClientTest, WhatsNewRequest) {
  WriteFirstRunSentinel();
  SetTrueChromeLikelyDefaultBrowser();
  SetSentNotifications({TipsNotificationType::kEnhancedSafeBrowsing});

  StubGetPendingRequests(nil);
  ExpectNotificationRequest(TipsNotificationType::kWhatsNew);

  base::RunLoop run_loop;
  client_->OnSceneActiveForegroundBrowserReady(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  histogram_tester_.ExpectUniqueSample("IOS.Notifications.Tips.Sent",
                                       TipsNotificationType::kWhatsNew, 1);
}

// Tests that the client can request a Proactive Whats New notification.
TEST_F(TipsNotificationClientTest, WhatsNewProactiveRequest) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kIOSReactivationNotifications);
  SetSentNotifications({TipsNotificationType::kLens,
                        TipsNotificationType::kEnhancedSafeBrowsing});

  [PushNotificationUtil
      updateAuthorizationStatusPref:UNAuthorizationStatusProvisional];
  // Simulate that the user has not opted-in.
  {
    ScopedDictPrefUpdate update(GetApplicationContext()->GetLocalState(),
                                prefs::kAppLevelPushNotificationPermissions);
    update->Remove(kTipsNotificationKey);
  }

  WriteFirstRunSentinel();
  SetTrueChromeLikelyDefaultBrowser();

  StubGetPendingRequests(nil);
  ExpectNotificationRequest(TipsNotificationType::kWhatsNew);

  base::RunLoop run_loop;
  client_->OnSceneActiveForegroundBrowserReady(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  histogram_tester_.ExpectUniqueSample("IOS.Notifications.Tips.Proactive.Sent",
                                       TipsNotificationType::kWhatsNew, 1);

  // Run again, but this time simulating a delivered notification.
  NSMutableArray<UNNotification*>* delivered_notifications = [NSMutableArray
      arrayWithObject:MockNotification(TipsNotificationType::kWhatsNew, true)];
  StubGetDeliveredNotifications(delivered_notifications);
  base::RunLoop run_loop_2;
  client_->OnSceneActiveForegroundBrowserReady(run_loop_2.QuitClosure());
  run_loop_2.Run();
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  histogram_tester_.ExpectUniqueSample(
      "IOS.Notifications.Tips.Proactive.Triggered",
      TipsNotificationType::kWhatsNew, 1);

  // Run again, but this time the delivered notification is gone.
  [delivered_notifications removeAllObjects];
  base::RunLoop run_loop_3;
  client_->OnSceneActiveForegroundBrowserReady(run_loop_3.QuitClosure());
  run_loop_3.Run();
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  histogram_tester_.ExpectUniqueSample(
      "IOS.Notifications.Tips.Proactive.Dismissed",
      TipsNotificationType::kWhatsNew, 1);
}

// Tests that the client will not request a Proactive Whats New notification if
// provisional notifications are disallowed by policy.
TEST_F(TipsNotificationClientTest, ProvisionalDisallowedByPolicy) {
  profile_->GetPrefs()->SetBoolean(
      prefs::kProvisionalNotificationsAllowedByPolicy, false);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kIOSReactivationNotifications);
  SetSentNotifications({TipsNotificationType::kLens,
                        TipsNotificationType::kEnhancedSafeBrowsing});
  [PushNotificationUtil
      updateAuthorizationStatusPref:UNAuthorizationStatusProvisional];
  // Simulate that the user has not opted-in.
  GetApplicationContext()->GetLocalState()->ClearPref(
      prefs::kAppLevelPushNotificationPermissions);

  WriteFirstRunSentinel();
  StubGetPendingRequests(nil);
  OCMReject([mock_notification_center_ addNotificationRequest:[OCMArg any]
                                        withCompletionHandler:[OCMArg any]]);

  base::RunLoop run_loop;
  client_->OnSceneActiveForegroundBrowserReady(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  histogram_tester_.ExpectUniqueSample("IOS.Notifications.Tips.Proactive.Sent",
                                       TipsNotificationType::kWhatsNew, 0);
}

// Tests that the client handles a Whats New notification response.
TEST_F(TipsNotificationClientTest, WhatsNewHandle) {
  StubPrepareToPresentModal();
  id mock_handler = MockHandler(@protocol(WhatsNewCommands));
  OCMExpect([mock_handler showWhatsNew]);

  id mock_response = MockRequestResponse(TipsNotificationType::kWhatsNew);
  client_->HandleNotificationInteraction(mock_response);

  EXPECT_OCMOCK_VERIFY(mock_handler);
  histogram_tester_.ExpectUniqueSample("IOS.Notifications.Tips.Interaction",
                                       TipsNotificationType::kWhatsNew, 1);
}

// Tests that the client can register a SetUpList Continuation notification.
TEST_F(TipsNotificationClientTest, SetUpListContinuationRequest) {
  WriteFirstRunSentinel();
  StubGetPendingRequests(nil);
  SetSentNotifications({TipsNotificationType::kEnhancedSafeBrowsing,
                        TipsNotificationType::kWhatsNew,
                        TipsNotificationType::kLens,
                        TipsNotificationType::kOmniboxPosition});
  ExpectNotificationRequest(TipsNotificationType::kSetUpListContinuation);

  base::RunLoop run_loop;
  client_->OnSceneActiveForegroundBrowserReady(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  histogram_tester_.ExpectUniqueSample(
      "IOS.Notifications.Tips.Sent",
      TipsNotificationType::kSetUpListContinuation, 1);
}

// Tests that the client handles a SetUpList Continuation notification response.
TEST_F(TipsNotificationClientTest, SetUpListContinuationHandle) {
  StubPrepareToPresentModal();
  id mock_handler = MockHandler(@protocol(ContentSuggestionsCommands));
  OCMExpect([mock_handler showSetUpListSeeMoreMenuExpanded:YES]);

  id mock_response =
      MockRequestResponse(TipsNotificationType::kSetUpListContinuation);
  client_->HandleNotificationInteraction(mock_response);

  EXPECT_OCMOCK_VERIFY(mock_handler);
  histogram_tester_.ExpectUniqueSample(
      "IOS.Notifications.Tips.Interaction",
      TipsNotificationType::kSetUpListContinuation, 1);
}

// Tests that the client can register a Docking promo notification.
TEST_F(TipsNotificationClientTest, DockingRequest) {
  WriteFirstRunSentinel();
  SetSentNotifications({TipsNotificationType::kEnhancedSafeBrowsing,
                        TipsNotificationType::kWhatsNew,
                        TipsNotificationType::kLens,
                        TipsNotificationType::kOmniboxPosition,
                        TipsNotificationType::kSetUpListContinuation,
                        TipsNotificationType::kDefaultBrowser});
  StubGetPendingRequests(nil);
  ExpectNotificationRequest(TipsNotificationType::kDocking);

  base::RunLoop run_loop;
  client_->OnSceneActiveForegroundBrowserReady(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  histogram_tester_.ExpectUniqueSample("IOS.Notifications.Tips.Sent",
                                       TipsNotificationType::kDocking, 1);
}

// Tests that the client handles a Docking promo notification response.
TEST_F(TipsNotificationClientTest, DockingHandle) {
  StubPrepareToPresentModal();
  id mock_handler = MockHandler(@protocol(DockingPromoCommands));
  OCMExpect([mock_handler showDockingPromo:YES]);

  id mock_response = MockRequestResponse(TipsNotificationType::kDocking);
  client_->HandleNotificationInteraction(mock_response);

  EXPECT_OCMOCK_VERIFY(mock_handler);
  histogram_tester_.ExpectUniqueSample("IOS.Notifications.Tips.Interaction",
                                       TipsNotificationType::kDocking, 1);
}

// Tests that the client can register an Omnibox Position promo notification.
TEST_F(TipsNotificationClientTest, OmniboxPositionRequest) {
  // OmniboxPositionChoice is only available on phones.
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_PHONE) {
    return;
  }

  WriteFirstRunSentinel();
  SetSentNotifications({TipsNotificationType::kEnhancedSafeBrowsing,
                        TipsNotificationType::kWhatsNew,
                        TipsNotificationType::kLens});
  StubGetPendingRequests(nil);
  ExpectNotificationRequest(TipsNotificationType::kOmniboxPosition);

  base::RunLoop run_loop;
  client_->OnSceneActiveForegroundBrowserReady(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  histogram_tester_.ExpectUniqueSample(
      "IOS.Notifications.Tips.Sent", TipsNotificationType::kOmniboxPosition, 1);
}

// Tests that the client handles an Omnibox Position promo notification
// response.
TEST_F(TipsNotificationClientTest, OmniboxPositionHandle) {
  // OmniboxPositionChoice is only available on phones.
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_PHONE) {
    return;
  }

  StubPrepareToPresentModal();
  id mock_handler = MockHandler(@protocol(BrowserCoordinatorCommands));
  OCMExpect([mock_handler showOmniboxPositionChoice]);

  id mock_response =
      MockRequestResponse(TipsNotificationType::kOmniboxPosition);
  client_->HandleNotificationInteraction(mock_response);

  EXPECT_OCMOCK_VERIFY(mock_handler);
  histogram_tester_.ExpectUniqueSample("IOS.Notifications.Tips.Interaction",
                                       TipsNotificationType::kOmniboxPosition,
                                       1);
}

// Tests that the client can register an Enhanced Safe Browsing promo
// notification.
TEST_F(TipsNotificationClientTest, EnhancedSafeBrowsingRequest) {
  WriteFirstRunSentinel();
  StubGetPendingRequests(nil);
  ExpectNotificationRequest(TipsNotificationType::kEnhancedSafeBrowsing);

  base::RunLoop run_loop;
  client_->OnSceneActiveForegroundBrowserReady(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  histogram_tester_.ExpectUniqueSample(
      "IOS.Notifications.Tips.Sent",
      TipsNotificationType::kEnhancedSafeBrowsing, 1);
}

// Tests that the client will not register an Enhanced Safe Browsing promo
// notification if ESB is disabled by policy.
TEST_F(TipsNotificationClientTest,
       EnhancedSafeBrowsingRequestWhenDisabledByPolicy) {
  // Disable pref that stores the policy setting.
  profile_->GetPrefs()->SetBoolean(prefs::kAdvancedProtectionAllowed, false);

  SetFalseChromeLikelyDefaultBrowser();
  ClearDefaultBrowserPromoLastAction();
  WriteFirstRunSentinel();
  StubGetPendingRequests(nil);
  // Expect to skip over ESB and send the next notification.
  ExpectNotificationRequest(TipsNotificationType::kWhatsNew);

  base::RunLoop run_loop;
  client_->OnSceneActiveForegroundBrowserReady(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
}

// Tests that the client handles an Enhanced Safe Browsing promo notification
// response.
TEST_F(TipsNotificationClientTest, EnhancedSafeBrowsingHandle) {
  StubPrepareToPresentModal();
  id mock_handler = MockHandler(@protocol(BrowserCoordinatorCommands));
  OCMExpect([mock_handler showEnhancedSafeBrowsingPromo]);

  id mock_response =
      MockRequestResponse(TipsNotificationType::kEnhancedSafeBrowsing);
  client_->HandleNotificationInteraction(mock_response);

  EXPECT_OCMOCK_VERIFY(mock_handler);
  histogram_tester_.ExpectUniqueSample(
      "IOS.Notifications.Tips.Interaction",
      TipsNotificationType::kEnhancedSafeBrowsing, 1);
}

// Tests that the client handles a Lens promo notification response.
TEST_F(TipsNotificationClientTest, LensHandle) {
  StubPrepareToPresentModal();
  id mock_handler = MockHandler(@protocol(BrowserCoordinatorCommands));
  OCMExpect([mock_handler showLensPromo]);

  id mock_response = MockRequestResponse(TipsNotificationType::kLens);
  client_->HandleNotificationInteraction(mock_response);

  EXPECT_OCMOCK_VERIFY(mock_handler);
  histogram_tester_.ExpectUniqueSample("IOS.Notifications.Tips.Interaction",
                                       TipsNotificationType::kLens, 1);
}

// Tests that the client handles a Lens promo proactive notification response.
TEST_F(TipsNotificationClientTest, LensProactiveHandle) {
  StubPrepareToPresentModal();
  id mock_handler = MockHandler(@protocol(BrowserCoordinatorCommands));
  OCMExpect([mock_handler showLensPromo]);

  id mock_response = MockRequestResponse(TipsNotificationType::kLens, true);
  client_->HandleNotificationInteraction(mock_response);

  EXPECT_OCMOCK_VERIFY(mock_handler);
  histogram_tester_.ExpectUniqueSample(
      "IOS.Notifications.Tips.Proactive.Interaction",
      TipsNotificationType::kLens, 1);
}

// Tests the the user can be classified as an "Active Seeker" of Tips.
TEST_F(TipsNotificationClientTest, ClassifyUserActiveSeeker) {
  base::ScopedMockClockOverride clock;
  WriteFirstRunSentinel();
  StubPrepareToPresentModal();
  EXPECT_EQ(GetUserType(), TipsNotificationUserType::kUnknown);

  StubGetPendingRequests(nil);
  ExpectNotificationRequest(TipsNotificationType::kEnhancedSafeBrowsing);
  base::RunLoop run_loop;
  client_->OnSceneActiveForegroundBrowserReady(run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);

  clock.Advance(base::Hours(1));
  SimulateForegroundingApp();
  EXPECT_EQ(GetUserType(), TipsNotificationUserType::kUnknown);

  clock.Advance(base::Hours(24));
  SimulateForegroundingApp();
  EXPECT_EQ(GetUserType(), TipsNotificationUserType::kActiveSeeker);
}

// Tests the the user can be classified as "Less Engaged".
TEST_F(TipsNotificationClientTest, ClassifyUserLessEngaged) {
  base::ScopedMockClockOverride clock;
  WriteFirstRunSentinel();
  StubPrepareToPresentModal();

  EXPECT_EQ(GetUserType(), TipsNotificationUserType::kUnknown);

  StubGetPendingRequests(nil);
  ExpectNotificationRequest(TipsNotificationType::kEnhancedSafeBrowsing);
  base::RunLoop run_loop;
  client_->OnSceneActiveForegroundBrowserReady(run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);

  clock.Advance(base::Hours(73));
  SimulateForegroundingApp();
  EXPECT_EQ(GetUserType(), TipsNotificationUserType::kLessEngaged);
}

// Tests that the correct trigger time is used, depending on the user's
// classification.
TEST_F(TipsNotificationClientTest, TestTriggerTimeDeltas) {
  EXPECT_EQ(
      TipsNotificationTriggerDelta(false, TipsNotificationUserType::kUnknown),
      base::Days(3));
  EXPECT_EQ(TipsNotificationTriggerDelta(
                false, TipsNotificationUserType::kLessEngaged),
            base::Days(21));
  EXPECT_EQ(TipsNotificationTriggerDelta(
                false, TipsNotificationUserType::kActiveSeeker),
            base::Days(7));

  // Verify that the experimental settings can override the trigger time.
  [[NSUserDefaults standardUserDefaults] setInteger:111
                                             forKey:@"TipsNotificationTrigger"];
  EXPECT_EQ(
      TipsNotificationTriggerDelta(false, TipsNotificationUserType::kUnknown),
      base::Seconds(111));
  [[NSUserDefaults standardUserDefaults]
      removeObjectForKey:@"TipsNotificationTrigger"];

  // Verify that the Reactivation feature param can set the trigger delta.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kIOSReactivationNotifications,
      {
          {kIOSReactivationNotificationsTriggerTimeParam, "30s"},
      });
  EXPECT_EQ(
      TipsNotificationTriggerDelta(true, TipsNotificationUserType::kUnknown),
      base::Seconds(30));
}

// Tests that the order of notification types changes correctly when the feature
// param is set.
TEST_F(TipsNotificationClientTest, TestOrderParam) {
  std::vector<TipsNotificationType> order = TipsNotificationsTypesOrder(false);
  EXPECT_EQ(order[0], TipsNotificationType::kEnhancedSafeBrowsing);
  EXPECT_EQ(order[1], TipsNotificationType::kWhatsNew);

  // Test Reactivation notifications order, default order.
  order = TipsNotificationsTypesOrder(true);
  EXPECT_EQ(order.size(), 3u);
  EXPECT_EQ(order[0], TipsNotificationType::kLens);
  EXPECT_EQ(order[1], TipsNotificationType::kEnhancedSafeBrowsing);
  EXPECT_EQ(order[2], TipsNotificationType::kWhatsNew);

  // Test Reactivation notifications order, alternate order.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kIOSReactivationNotifications,
      {
          // The alternate order: ESB, Lens, What's New.
          {kIOSReactivationNotificationsOrderParam, "8,7,1"},
      });
  order = TipsNotificationsTypesOrder(true);
  EXPECT_EQ(order.size(), 3u);
  EXPECT_EQ(order[0], TipsNotificationType::kEnhancedSafeBrowsing);
  EXPECT_EQ(order[1], TipsNotificationType::kLens);
  EXPECT_EQ(order[2], TipsNotificationType::kWhatsNew);

  // Test Expanded Tips order param.
  feature_list.Reset();
  feature_list.InitAndEnableFeatureWithParameters(
      kIOSExpandedTips, {
                            {kIOSExpandedTipsOrderParam, "4,5,6"},
                        });
  order = TipsNotificationsTypesOrder(false);
  EXPECT_EQ(order.size(), 3u);
  EXPECT_EQ(order[0], TipsNotificationType::kSetUpListContinuation);
  EXPECT_EQ(order[1], TipsNotificationType::kDocking);
  EXPECT_EQ(order[2], TipsNotificationType::kOmniboxPosition);
}

// Tests that the client can register a CPE Promo notification, only when the
// CPE promo was displayed more than 7 days ago.
TEST_F(TipsNotificationClientTest, CPERequest) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kIOSExpandedTips);
  WriteFirstRunSentinel();
  StubGetPendingRequests(nil);
  SetSentNotifications({
      TipsNotificationType::kEnhancedSafeBrowsing,
      TipsNotificationType::kWhatsNew,
      TipsNotificationType::kLens,
      TipsNotificationType::kOmniboxPosition,
      TipsNotificationType::kSetUpListContinuation,
      TipsNotificationType::kDefaultBrowser,
      TipsNotificationType::kDocking,
      TipsNotificationType::kSignin,
  });
  PrefService* local_state = GetApplicationContext()->GetLocalState();
  local_state->SetTime(prefs::kIosCredentialProviderPromoDisplayTime,
                       base::Time::Now() - base::Days(6));
  local_state->SetTime(prefs::kIosSuccessfulLoginWithExistingPassword,
                       base::Time::Now() - base::Days(29));

  // A notification should not be requested yet because promo display time is
  // less than 30 days ago.
  OCMReject([mock_notification_center_ addNotificationRequest:[OCMArg any]
                                        withCompletionHandler:[OCMArg any]]);
  base::RunLoop run_loop;
  client_->OnSceneActiveForegroundBrowserReady(run_loop.QuitClosure());
  run_loop.Run();

  // Simulate that the CPE promo was displayed more than 30 days ago.
  local_state->SetTime(prefs::kIosCredentialProviderPromoDisplayTime,
                       base::Time::Now() - base::Days(8));
  SetupMockNotificationCenter();
  StubGetPendingRequests(nil);
  ExpectNotificationRequest(TipsNotificationType::kCPE);
  base::RunLoop run_loop2;
  client_->OnSceneActiveForegroundBrowserReady(run_loop2.QuitClosure());
  run_loop2.Run();

  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  histogram_tester_.ExpectUniqueSample("IOS.Notifications.Tips.Sent",
                                       TipsNotificationType::kCPE, 1);
}

// Tests that the client handles a CPE Promo notification response.
TEST_F(TipsNotificationClientTest, CPEHandle) {
  StubPrepareToPresentModal();
  id mock_handler = MockHandler(@protocol(CredentialProviderPromoCommands));
  OCMExpect(
      [mock_handler showCredentialProviderPromoWithTrigger:
                        CredentialProviderPromoTrigger::TipsNotification]);

  id mock_response = MockRequestResponse(TipsNotificationType::kCPE);
  client_->HandleNotificationInteraction(mock_response);

  EXPECT_OCMOCK_VERIFY(mock_handler);
  histogram_tester_.ExpectUniqueSample("IOS.Notifications.Tips.Interaction",
                                       TipsNotificationType::kCPE, 1);
}
