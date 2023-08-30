// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin_promo_view_mediator.h"

#import "base/functional/callback_helpers.h"
#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "build/branding_buildflags.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/sync/test/mock_sync_service.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/signin/fake_system_identity_manager.h"
#import "ios/chrome/browser/sync/mock_sync_service_utils.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service_mock.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_configurator.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_constants.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_consumer.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "third_party/ocmock/ocmock_extensions.h"
#import "ui/base/l10n/l10n_util.h"

using base::SysNSStringToUTF16;
using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;
using l10n_util::GetNSString;
using l10n_util::GetNSStringF;
using sync_preferences::PrefServiceMockFactory;
using sync_preferences::PrefServiceSyncable;
using user_prefs::PrefRegistrySyncable;
using web::WebTaskEnvironment;

namespace {

class SigninPromoViewMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    identity_ = [FakeSystemIdentity fakeIdentity1];
    close_button_hidden_ = YES;

    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));
    builder.AddTestingFactory(
        SyncSetupServiceFactory::GetInstance(),
        base::BindRepeating(&SyncSetupServiceMock::CreateKeyedService));
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    chrome_browser_state_ = builder.Build();
    // Set up the test browser and attach the browser agents.
    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        chrome_browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());
  }

  void TearDown() override {
    // All callbacks should be triggered to make sure tests are working
    // correctly.

    fake_system_identity_manager()->WaitForServiceCallbacksToComplete();
    if (mediator_) {
      [mediator_ disconnect];
      EXPECT_EQ(SigninPromoViewState::kInvalid, mediator_.signinPromoViewState);
      EXPECT_EQ(nil, mediator_.consumer);
      mediator_ = nil;
    }
    EXPECT_OCMOCK_VERIFY((id)consumer_);
    EXPECT_OCMOCK_VERIFY((id)signin_promo_view_);
    EXPECT_OCMOCK_VERIFY((id)primary_button_);
    EXPECT_OCMOCK_VERIFY((id)secondary_button_);
    EXPECT_OCMOCK_VERIFY((id)close_button_);
  }

  void CreateMediator(signin_metrics::AccessPoint access_point) {
    consumer_ = OCMStrictProtocolMock(@protocol(SigninPromoViewConsumer));
    mediator_ = [[SigninPromoViewMediator alloc]
        initWithAccountManagerService:ChromeAccountManagerServiceFactory::
                                          GetForBrowserState(
                                              chrome_browser_state_.get())
                          authService:GetAuthenticationService()
                          prefService:chrome_browser_state_.get()->GetPrefs()
                          syncService:GetSyncService()
                          accessPoint:access_point
                            presenter:nil
                   baseViewController:nil];
    mediator_.consumer = consumer_;

    signin_promo_view_ = OCMStrictClassMock([SigninPromoView class]);
    primary_button_ = OCMStrictClassMock([UIButton class]);
    OCMStub([signin_promo_view_ primaryButton]).andReturn(primary_button_);
    secondary_button_ = OCMStrictClassMock([UIButton class]);
    OCMStub([signin_promo_view_ secondaryButton]).andReturn(secondary_button_);
    close_button_ = OCMStrictClassMock([UIButton class]);
    OCMStub([signin_promo_view_ closeButton]).andReturn(close_button_);
  }

  std::unique_ptr<PrefServiceSyncable> CreatePrefService() {
    PrefServiceMockFactory factory;
    scoped_refptr<PrefRegistrySyncable> registry(new PrefRegistrySyncable);
    std::unique_ptr<PrefServiceSyncable> prefs =
        factory.CreateSyncable(registry.get());
    RegisterBrowserStatePrefs(registry.get());
    return prefs;
  }

  AuthenticationService* GetAuthenticationService() {
    return AuthenticationServiceFactory::GetForBrowserState(
        chrome_browser_state_.get());
  }

  syncer::SyncService* GetSyncService() {
    return SyncServiceFactory::GetForBrowserState(chrome_browser_state_.get());
  }

  // Creates the default identity and adds it into the ChromeIdentityService.
  void AddDefaultIdentity() {
    fake_system_identity_manager()->AddIdentity(identity_);
  }

  PrefService* GetLocalState() { return scoped_testing_local_state_.Get(); }

  // Tests the mediator with a new created configurator when no accounts are on
  // the device.
  void TestSigninPromoWithNoAccounts(SigninPromoViewStyle style) {
    EXPECT_EQ(nil, mediator_.identity);
    CheckNoAccountsConfigurator([mediator_ createConfigurator], style);
  }

  // Adds an identity and tests the mediator.
  void TestSigninPromoWithAccount(SigninPromoViewStyle style) {
    // Expect to receive an update to the consumer with a configurator.
    ExpectConfiguratorNotification(YES /* identity changed */);
    AddDefaultIdentity();
    // Check the configurator received by the consumer.
    CheckSigninWithAccountConfigurator(configurator_, style);
    // Check a new created configurator.
    CheckSigninWithAccountConfigurator([mediator_ createConfigurator], style);
    // The consumer should receive a notification related to the image.
    CheckForImageNotification(style);
  }

  // Expects a notification on the consumer for an identity update, and stores
  // the received configurator into configurator_.
  void ExpectConfiguratorNotification(BOOL identity_changed) {
    configurator_ = nil;
    SigninPromoViewConfigurator* configurator_arg =
        [OCMArg checkWithBlock:^BOOL(id value) {
          configurator_ = value;
          return YES;
        }];
    OCMExpect([consumer_
        configureSigninPromoWithConfigurator:configurator_arg
                             identityChanged:identity_changed]);
  }

  // Expects the signin promo view to be configured with no accounts on the
  // device.
  void ExpectNoAccountsConfiguration(SigninPromoViewStyle style) {
    OCMExpect([signin_promo_view_ setMode:SigninPromoViewModeNoAccounts]);
    NSString* title = nil;
    switch (style) {
      case SigninPromoViewStyleStandard:
        title = GetNSString(IDS_IOS_SYNC_PROMO_TURN_ON_SYNC);
        break;
      case SigninPromoViewStyleCompactHorizontal:
      case SigninPromoViewStyleCompactVertical:
        title = GetNSString(IDS_IOS_NTP_FEED_SIGNIN_PROMO_CONTINUE);
        break;
      case SigninPromoViewStyleOnlyButton:
        title = GetNSString(IDS_IOS_SIGNIN_PROMO_TURN_ON);
        break;
    }
    OCMExpect([signin_promo_view_ configurePrimaryButtonWithTitle:title]);
    image_view_profile_image_ = nil;
  }

  // Checks a configurator with no accounts on the device.
  void CheckNoAccountsConfigurator(SigninPromoViewConfigurator* configurator,
                                   SigninPromoViewStyle style) {
    EXPECT_NE(nil, configurator);
    ExpectNoAccountsConfiguration(style);
    OCMExpect([close_button_ setHidden:close_button_hidden_]);
    OCMExpect([signin_promo_view_ setPromoViewStyle:style]);
    OCMExpect([signin_promo_view_ stopSignInSpinner]);
    if (style == SigninPromoViewStyleCompactVertical ||
        style == SigninPromoViewStyleCompactHorizontal) {
      UIImage* logo;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      logo = [UIImage imageNamed:@"signin_promo_logo_chrome_color"];
#else
      logo = [UIImage imageNamed:@"signin_promo_logo_chromium_color"];
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
      OCMExpect([signin_promo_view_ setNonProfileImage:logo]);
    }
    [configurator configureSigninPromoView:signin_promo_view_ withStyle:style];
    EXPECT_EQ(nil, image_view_profile_image_);
  }

  // Expects the signin promo view to be configured when accounts are on the
  // device.
  void ExpectSigninWithAccountConfiguration(SigninPromoViewStyle style) {
    EXPECT_EQ(identity_, mediator_.identity);
    OCMExpect(
        [signin_promo_view_ setMode:SigninPromoViewModeSigninWithAccount]);
    switch (style) {
      case SigninPromoViewStyleStandard: {
        NSString* name = identity_.userGivenName.length
                             ? identity_.userGivenName
                             : identity_.userEmail;
        std::u16string name16 = SysNSStringToUTF16(name);
        OCMExpect([signin_promo_view_
            configurePrimaryButtonWithTitle:
                GetNSStringF(IDS_IOS_SIGNIN_PROMO_CONTINUE_AS, name16)]);
        OCMExpect([secondary_button_
            setTitle:GetNSString(IDS_IOS_SIGNIN_PROMO_CHANGE_ACCOUNT)
            forState:UIControlStateNormal]);
        OCMExpect([signin_promo_view_
            setProfileImage:[OCMArg checkWithBlock:^BOOL(id value) {
              image_view_profile_image_ = value;
              return YES;
            }]]);

        break;
      }
      case SigninPromoViewStyleCompactHorizontal:
      case SigninPromoViewStyleCompactVertical: {
        OCMExpect([signin_promo_view_
            configurePrimaryButtonWithTitle:
                GetNSString(IDS_IOS_NTP_FEED_SIGNIN_PROMO_CONTINUE)]);
        OCMExpect([signin_promo_view_
            setProfileImage:[OCMArg checkWithBlock:^BOOL(id value) {
              image_view_profile_image_ = value;
              return YES;
            }]]);
        break;
      }
      case SigninPromoViewStyleOnlyButton:
        OCMExpect([signin_promo_view_
            configurePrimaryButtonWithTitle:GetNSString(
                                                IDS_IOS_SIGNIN_PROMO_TURN_ON)]);
        break;
    }
  }

  // Checks a configurator with accounts on the device.
  void CheckSigninWithAccountConfigurator(
      SigninPromoViewConfigurator* configurator,
      SigninPromoViewStyle style) {
    EXPECT_NE(nil, configurator);
    ExpectSigninWithAccountConfiguration(style);
    OCMExpect([close_button_ setHidden:close_button_hidden_]);
    OCMExpect([signin_promo_view_ setPromoViewStyle:style]);
    OCMExpect([signin_promo_view_ stopSignInSpinner]);
    [configurator configureSigninPromoView:signin_promo_view_ withStyle:style];
    switch (style) {
      case SigninPromoViewStyleStandard:
      case SigninPromoViewStyleCompactHorizontal:
      case SigninPromoViewStyleCompactVertical:
        EXPECT_NE(nil, image_view_profile_image_);
        break;
      case SigninPromoViewStyleOnlyButton:
        EXPECT_EQ(nil, image_view_profile_image_);
        break;
    }
  }

  // Expects the sync promo view to be configured
  void ExpectSyncPromoConfiguration() {
    OCMExpect(
        [signin_promo_view_ setMode:SigninPromoViewModeSyncWithPrimaryAccount]);
    OCMExpect([signin_promo_view_
        setProfileImage:[OCMArg checkWithBlock:^BOOL(id value) {
          image_view_profile_image_ = value;
          return YES;
        }]]);
    NSString* name = identity_.userGivenName.length ? identity_.userGivenName
                                                    : identity_.userEmail;
    std::u16string name16 = SysNSStringToUTF16(name);
    OCMExpect([signin_promo_view_
        configurePrimaryButtonWithTitle:GetNSString(
                                            IDS_IOS_SYNC_PROMO_TURN_ON_SYNC)]);
    image_view_profile_image_ = nil;
  }

  // Checks a configurator with accounts on the device.
  void CheckSyncPromoWithAccountConfigurator(
      SigninPromoViewConfigurator* configurator,
      SigninPromoViewStyle style) {
    EXPECT_NE(nil, configurator);
    ExpectSyncPromoConfiguration();
    OCMExpect([close_button_ setHidden:close_button_hidden_]);
    OCMExpect([signin_promo_view_ setPromoViewStyle:style]);
    OCMExpect([signin_promo_view_ stopSignInSpinner]);
    [configurator configureSigninPromoView:signin_promo_view_ withStyle:style];
    EXPECT_NE(nil, image_view_profile_image_);
  }

  // Checks to receive a notification for the image upate of the current
  // identity.
  void CheckForImageNotification(SigninPromoViewStyle style) {
    configurator_ = nil;
    ExpectConfiguratorNotification(NO /* identity changed */);

    fake_system_identity_manager()->WaitForServiceCallbacksToComplete();
    // Check the configurator received by the consumer.
    CheckSigninWithAccountConfigurator(configurator_, style);
  }

  FakeSystemIdentityManager* fake_system_identity_manager() {
    return FakeSystemIdentityManager::FromSystemIdentityManager(
        GetApplicationContext()->GetSystemIdentityManager());
  }

  // Task environment.
  WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;

  // Mediator used for the tests.
  SigninPromoViewMediator* mediator_;

  // Identity used for sign-in.
  id<SystemIdentity> identity_;

  // Configurator received from the consumer.
  SigninPromoViewConfigurator* configurator_;

  // Mocks.
  id<SigninPromoViewConsumer> consumer_;
  SigninPromoView* signin_promo_view_;
  UIButton* primary_button_;
  UIButton* secondary_button_;
  UIButton* close_button_;

  // Value set by -[SigninPromoView setProfileImage:].
  UIImage* image_view_profile_image_;
  // Value set by -[close_button_ setHidden:].
  BOOL close_button_hidden_;
};

// Tests signin promo view and its configurator with no accounts on the device.
TEST_F(SigninPromoViewMediatorTest, NoAccountsConfigureSigninPromoView) {
  CreateMediator(signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS);
  TestSigninPromoWithNoAccounts(SigninPromoViewStyleStandard);
}

// Tests signin promo view and its configurator settings view with no accounts
// on the device.
TEST_F(SigninPromoViewMediatorTest,
       NoAccountsConfigureSigninPromoViewFromSettings) {
  close_button_hidden_ = NO;
  CreateMediator(signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS);
  TestSigninPromoWithNoAccounts(SigninPromoViewStyleStandard);
}

// Tests signin promo view and its configurator with accounts on the device.
TEST_F(SigninPromoViewMediatorTest, SigninWithAccountConfigureSigninPromoView) {
  CreateMediator(signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS);
  TestSigninPromoWithAccount(SigninPromoViewStyleStandard);
}

// Tests signin promo view and its configurator with an identity
// without full name.
TEST_F(SigninPromoViewMediatorTest,
       SigninWithAccountConfigureSigninPromoViewWithoutName) {
  CreateMediator(signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS);
  TestSigninPromoWithAccount(SigninPromoViewStyleStandard);
}

// Tests the scenario with the sign-in promo when no accounts on the device, and
// then add an identity to update the view.
TEST_F(SigninPromoViewMediatorTest, ConfigureSigninPromoViewWithColdAndWarm) {
  CreateMediator(signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS);
  TestSigninPromoWithNoAccounts(SigninPromoViewStyleStandard);
  TestSigninPromoWithAccount(SigninPromoViewStyleStandard);
}

// Tests the sign-in promo with and without account when the promo style is
// compact horizontal.
TEST_F(SigninPromoViewMediatorTest,
       ConfigureCompactHorizontalSigninPromoViewWithColdAndWarm) {
  CreateMediator(signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS);
  TestSigninPromoWithNoAccounts(SigninPromoViewStyleCompactHorizontal);
  TestSigninPromoWithAccount(SigninPromoViewStyleCompactHorizontal);
}

// Tests the sign-in promo with and without account when the promo style is
// SigninPromoViewStyleOnlyButton.
TEST_F(SigninPromoViewMediatorTest,
       ConfigureOnlyButtonSigninPromoViewWithColdAndWarm) {
  CreateMediator(signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS);
  TestSigninPromoWithNoAccounts(SigninPromoViewStyleOnlyButton);
  TestSigninPromoWithAccount(SigninPromoViewStyleOnlyButton);
}

// Tests the sign-in promo with and without account when the promo style is
// compact vertical.
TEST_F(SigninPromoViewMediatorTest,
       ConfigureCompactVerticalSigninPromoViewWithColdAndWarm) {
  CreateMediator(signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS);
  TestSigninPromoWithNoAccounts(SigninPromoViewStyleCompactVertical);
  TestSigninPromoWithAccount(SigninPromoViewStyleCompactVertical);
}

// Tests the scenario with the sign-in promo with accounts on the device, and
// then removing the identity to update the view.
TEST_F(SigninPromoViewMediatorTest, ConfigureSigninPromoViewWithWarmAndCold) {
  CreateMediator(signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS);
  TestSigninPromoWithAccount(SigninPromoViewStyleStandard);
  // Expect to receive a new configuration from -[Consumer
  // configureSigninPromoWithConfigurator:identityChanged:].
  ExpectConfiguratorNotification(YES /* identity changed */);

  // Forgetting an identity is an asynchronous operation, so we need to wait
  // before the notification is sent.
  {
    base::RunLoop run_loop;
    fake_system_identity_manager()->ForgetIdentity(
        identity_, base::IgnoreArgs<NSError*>(run_loop.QuitClosure()));
    run_loop.Run();
  }
  identity_ = nil;

  // Check the received configurator.
  CheckNoAccountsConfigurator(configurator_, SigninPromoViewStyleStandard);
}

// Tests the view state before and after calling -[SigninPromoViewMediator
// signinPromoViewIsVisible].
TEST_F(SigninPromoViewMediatorTest, SigninPromoViewStateVisible) {
  CreateMediator(signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS);
  // Test initial state.
  EXPECT_EQ(SigninPromoViewState::kNeverVisible,
            mediator_.signinPromoViewState);
  [mediator_ signinPromoViewIsVisible];
  // Test state once the sign-in promo view is visible.
  EXPECT_EQ(SigninPromoViewState::kUnused, mediator_.signinPromoViewState);
}

// Tests the view state while signing in.
TEST_F(SigninPromoViewMediatorTest, SigninPromoViewStateSignedin) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(kHideSettingsSyncPromo);

  CreateMediator(signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS);
  [mediator_ signinPromoViewIsVisible];
  __block ShowSigninCommandCompletionCallback completion;
  ShowSigninCommandCompletionCallback completion_arg =
      [OCMArg checkWithBlock:^BOOL(ShowSigninCommandCompletionCallback value) {
        completion = value;
        return YES;
      }];
  // Start sign-in.
  OCMExpect([consumer_
           signinPromoViewMediator:mediator_
      shouldOpenSigninWithIdentity:nil
                       promoAction:
                           signin_metrics::PromoAction::
                               PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT
                        completion:completion_arg]);
  OCMExpect([consumer_ promoProgressStateDidChange]);
  ExpectConfiguratorNotification(NO /* identity changed */);
  [mediator_ signinPromoViewDidTapSigninWithNewAccount:signin_promo_view_];
  EXPECT_TRUE(mediator_.showSpinner);
  EXPECT_EQ(SigninPromoViewState::kUsedAtLeastOnce,
            mediator_.signinPromoViewState);
  EXPECT_NE(nil, (id)completion);
  // Stop sign-in.
  OCMExpect([consumer_ promoProgressStateDidChange]);
  OCMExpect([consumer_ signinDidFinish]);
  ExpectConfiguratorNotification(NO /* identity changed */);
  completion(SigninCoordinatorResultSuccess, nil);
  EXPECT_FALSE(mediator_.showSpinner);
  EXPECT_EQ(SigninPromoViewState::kUsedAtLeastOnce,
            mediator_.signinPromoViewState);
}

// Tests that no update notification is sent by the mediator to its consumer,
// while the sign-in is in progress, when an identity is added.
TEST_F(SigninPromoViewMediatorTest,
       SigninPromoViewNoUpdateNotificationWhileSignin) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(kHideSettingsSyncPromo);

  CreateMediator(signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS);
  [mediator_ signinPromoViewIsVisible];
  __block ShowSigninCommandCompletionCallback completion;
  ShowSigninCommandCompletionCallback completion_arg =
      [OCMArg checkWithBlock:^BOOL(ShowSigninCommandCompletionCallback value) {
        completion = value;
        return YES;
      }];
  OCMExpect([consumer_
           signinPromoViewMediator:mediator_
      shouldOpenSigninWithIdentity:nil
                       promoAction:
                           signin_metrics::PromoAction::
                               PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT
                        completion:completion_arg]);
  OCMExpect([consumer_ promoProgressStateDidChange]);
  ExpectConfiguratorNotification(NO /* identity changed */);
  // Starts sign-in without identity.
  [mediator_ signinPromoViewDidTapSigninWithNewAccount:signin_promo_view_];
  // Adds an identity while doing sign-in.
  AddDefaultIdentity();
  // No consumer notification should be expected.
  fake_system_identity_manager()->WaitForServiceCallbacksToComplete();
  // Finishs the sign-in.
  OCMExpect([consumer_ promoProgressStateDidChange]);
  OCMExpect([consumer_ signinDidFinish]);
  ExpectConfiguratorNotification(NO /* identity changed */);
  completion(SigninCoordinatorResultSuccess, nil);
}

// Tests that no update notification is sent by the mediator to its consumer,
// while the sign-in is in progress.
TEST_F(SigninPromoViewMediatorTest,
       SigninPromoViewNoUpdateNotificationWhileSignin2) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(kHideSettingsSyncPromo);

  AddDefaultIdentity();
  CreateMediator(signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS);
  [mediator_ signinPromoViewIsVisible];
  __block ShowSigninCommandCompletionCallback completion;
  ShowSigninCommandCompletionCallback completion_arg =
      [OCMArg checkWithBlock:^BOOL(ShowSigninCommandCompletionCallback value) {
        completion = value;
        return YES;
      }];
  OCMExpect([consumer_ signinPromoViewMediator:mediator_
                  shouldOpenSigninWithIdentity:identity_
                                   promoAction:signin_metrics::PromoAction::
                                                   PROMO_ACTION_WITH_DEFAULT
                                    completion:completion_arg]);
  OCMExpect([consumer_ promoProgressStateDidChange]);
  ExpectConfiguratorNotification(NO /* identity changed */);
  // Starts sign-in with an identity.
  [mediator_ signinPromoViewDidTapSigninWithDefaultAccount:signin_promo_view_];
  EXPECT_TRUE([mediator_
      conformsToProtocol:@protocol(ChromeAccountManagerServiceObserver)]);
  id<ChromeAccountManagerServiceObserver> accountManagerServiceObserver =
      (id<ChromeAccountManagerServiceObserver>)mediator_;
  // Simulates an identity update.
  [accountManagerServiceObserver identityUpdated:identity_];
  // Spins the run loop to wait for the profile image update.
  fake_system_identity_manager()->WaitForServiceCallbacksToComplete();
  // Finishs the sign-in.
  OCMExpect([consumer_ promoProgressStateDidChange]);
  OCMExpect([consumer_ signinDidFinish]);
  ExpectConfiguratorNotification(NO /* identity changed */);
  completion(SigninCoordinatorResultSuccess, nil);
}

// Tests that promos aren't shown if browser sign-in is disabled by policy
TEST_F(SigninPromoViewMediatorTest,
       ShouldNotDisplaySigninPromoViewIfDisabledByPolicy) {
  CreateMediator(signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS);
  TestChromeBrowserState::Builder builder;
  builder.SetPrefService(CreatePrefService());
  std::unique_ptr<TestChromeBrowserState> browser_state = builder.Build();
  GetLocalState()->SetInteger(prefs::kBrowserSigninPolicy,
                              static_cast<int>(BrowserSigninMode::kDisabled));
  EXPECT_FALSE([SigninPromoViewMediator
      shouldDisplaySigninPromoViewWithAccessPoint:signin_metrics::AccessPoint::
                                                      ACCESS_POINT_RECENT_TABS
                            authenticationService:GetAuthenticationService()
                                      prefService:browser_state->GetPrefs()]);
}

// Tests that the default identity is the primary account, when the user is
// signed in.
TEST_F(SigninPromoViewMediatorTest, SigninPromoWhileSignedIn) {
  AddDefaultIdentity();
  identity_ = [FakeSystemIdentity fakeIdentity2];
  fake_system_identity_manager()->AddIdentity(identity_);
  GetAuthenticationService()->SignIn(
      identity_, signin_metrics::AccessPoint::ACCESS_POINT_SIGNIN_PROMO);
  CreateMediator(signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS);
  ExpectConfiguratorNotification(NO /* identity changed */);
  [mediator_ signinPromoViewIsVisible];
  EXPECT_EQ(identity_, mediator_.identity);
  fake_system_identity_manager()->WaitForServiceCallbacksToComplete();
  CheckSyncPromoWithAccountConfigurator(configurator_,
                                        SigninPromoViewStyleStandard);
}

// Tests that the sign-in promo view being removed and the mediator being
// deallocated while the sign-in is in progress, and tests the consumer is still
// called at the end of the sign-in.
TEST_F(SigninPromoViewMediatorTest,
       RemoveSigninPromoAndDeallocMediatorWhileSignedIn) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(kHideSettingsSyncPromo);

  // Setup.
  AddDefaultIdentity();
  CreateMediator(signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS);
  __weak __typeof(mediator_) weak_mediator = mediator_;
  __block ShowSigninCommandCompletionCallback completion;
  // This test wants to verify behavior when `mediator_` gets deallocated.
  // OCMock uses autorelease in places, which could result in `mediator_`
  // staying allocated longer than we want without this @autoreleasepool.
  @autoreleasepool {
    [mediator_ signinPromoViewIsVisible];
    id completion_arg = [OCMArg
        checkWithBlock:^BOOL(ShowSigninCommandCompletionCallback value) {
          completion = value;
          return YES;
        }];
    OCMExpect([consumer_ signinPromoViewMediator:mediator_
                    shouldOpenSigninWithIdentity:identity_
                                     promoAction:signin_metrics::PromoAction::
                                                     PROMO_ACTION_WITH_DEFAULT
                                      completion:completion_arg]);
    OCMExpect([consumer_ promoProgressStateDidChange]);
    ExpectConfiguratorNotification(NO /* identity changed */);
    // Start sign-in with an identity.
    [mediator_
        signinPromoViewDidTapSigninWithDefaultAccount:signin_promo_view_];
    // Remove the sign-in promo.
    [mediator_ disconnect];
    EXPECT_EQ(SigninPromoViewState::kInvalid, mediator_.signinPromoViewState);
    // Dealloc the mediator.
    mediator_ = nil;
    // Also clear all invocations from `consumer_` after verifying them, as the
    // invocations also contain a reference to `mediator_`. Generally this is
    // what `stopMocking` is meant for, but we still want to verify that
    // `signinDidfinish` is called below, so we can't just stop mocking yet.
    EXPECT_OCMOCK_VERIFY(consumer_);
    [(OCMockObject*)consumer_ clearInvocations];
  }
  EXPECT_EQ(weak_mediator, nil);
  // Finish the sign-in.
  OCMExpect([consumer_ signinDidFinish]);
  completion(SigninCoordinatorResultSuccess, nil);
}

// Tests that the sign-in promo view being removed, and tests the consumer is
// still called at the end of the sign-in.
TEST_F(SigninPromoViewMediatorTest, RemoveSigninPromoWhileSignedIn) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(kHideSettingsSyncPromo);

  // Setup.
  AddDefaultIdentity();
  CreateMediator(signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS);
  [mediator_ signinPromoViewIsVisible];
  __block ShowSigninCommandCompletionCallback completion;
  id completion_arg =
      [OCMArg checkWithBlock:^BOOL(ShowSigninCommandCompletionCallback value) {
        completion = value;
        return YES;
      }];
  OCMExpect([consumer_ signinPromoViewMediator:mediator_
                  shouldOpenSigninWithIdentity:identity_
                                   promoAction:signin_metrics::PromoAction::
                                                   PROMO_ACTION_WITH_DEFAULT
                                    completion:completion_arg]);
  OCMExpect([consumer_ promoProgressStateDidChange]);
  ExpectConfiguratorNotification(NO /* identity changed */);
  // Start sign-in with an identity.
  [mediator_ signinPromoViewDidTapSigninWithDefaultAccount:signin_promo_view_];
  // Remove the sign-in promo.
  [mediator_ disconnect];
  EXPECT_EQ(SigninPromoViewState::kInvalid, mediator_.signinPromoViewState);
  // Finish the sign-in.
  OCMExpect([consumer_ signinDidFinish]);
  completion(SigninCoordinatorResultSuccess, nil);
  // Set mediator_ to nil to avoid the TearDown doesn't call
  // -[mediator_ disconnect] again.
  mediator_ = nil;
}

}  // namespace
