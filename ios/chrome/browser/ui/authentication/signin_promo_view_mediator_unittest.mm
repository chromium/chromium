// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin_promo_view_mediator.h"

#import "base/functional/callback_helpers.h"
#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "build/branding_buildflags.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/sync/test/mock_sync_service.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/sync/model/mock_sync_service_utils.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/account_settings_presenter.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_configurator.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_constants.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_consumer.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_presenter.h"
#import "ios/chrome/grit/ios_branded_strings.h"
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

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();
    // Set up the test browser and attach the browser agents.
    AuthenticationServiceFactory::CreateAndInitializeForProfile(
        profile_.get(), std::make_unique<FakeAuthenticationServiceDelegate>());
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
    signin_presenter_ = OCMStrictProtocolMock(@protocol(SigninPresenter));
    account_settings_presenter_ =
        OCMStrictProtocolMock(@protocol(AccountSettingsPresenter));
    mediator_ = [[SigninPromoViewMediator alloc]
        initWithAccountManagerService:ChromeAccountManagerServiceFactory::
                                          GetForProfile(profile_.get())
                          authService:GetAuthenticationService()
                          prefService:profile_.get()->GetPrefs()
                          syncService:GetSyncService()
                          accessPoint:access_point
                      signinPresenter:signin_presenter_
             accountSettingsPresenter:account_settings_presenter_];
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
    RegisterProfilePrefs(registry.get());
    return prefs;
  }

  AuthenticationService* GetAuthenticationService() {
    return AuthenticationServiceFactory::GetForProfile(profile_.get());
  }

  syncer::SyncService* GetSyncService() {
    return SyncServiceFactory::GetForProfile(profile_.get());
  }

  // Creates the default identity and adds it into the ChromeIdentityService.
  void AddDefaultIdentity() {
    fake_system_identity_manager()->AddIdentity(identity_);
  }

  PrefService* GetLocalState() {
    return GetApplicationContext()->GetLocalState();
  }

  // Tests the mediator with a new created configurator when no accounts are on
  // the device.
  void TestSigninPromoWithNoAccounts(SigninPromoViewStyle style) {
    EXPECT_EQ(nil, mediator_.displayedIdentity);
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
        title = GetNSString(IDS_IOS_CONSISTENCY_PROMO_SIGN_IN);
        break;
      case SigninPromoViewStyleCompact:
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
    OCMExpect([close_button_ setHidden:YES]);
    OCMExpect([signin_promo_view_ setPromoViewStyle:style]);
    OCMExpect([signin_promo_view_ stopSignInSpinner]);
    if (style == SigninPromoViewStyleCompact) {
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
      UIImage* logo = [UIImage imageNamed:kChromeSigninPromoLogoImage];
#else
      UIImage* logo = [UIImage imageNamed:kChromiumSigninPromoLogoImage];
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
      OCMExpect([signin_promo_view_ setNonProfileImage:logo]);
    }
    [configurator configureSigninPromoView:signin_promo_view_ withStyle:style];
    EXPECT_EQ(nil, image_view_profile_image_);
  }

  // Expects the signin promo view to be configured when accounts are on the
  // device.
  void ExpectSigninWithAccountConfiguration(SigninPromoViewStyle style) {
    EXPECT_EQ(identity_, mediator_.displayedIdentity);
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
      case SigninPromoViewStyleCompact: {
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
    OCMExpect([close_button_ setHidden:YES]);
    OCMExpect([signin_promo_view_ setPromoViewStyle:style]);
    OCMExpect([signin_promo_view_ stopSignInSpinner]);
    [configurator configureSigninPromoView:signin_promo_view_ withStyle:style];
    switch (style) {
      case SigninPromoViewStyleStandard:
      case SigninPromoViewStyleCompact:
        EXPECT_NE(nil, image_view_profile_image_);
        break;
      case SigninPromoViewStyleOnlyButton:
        EXPECT_EQ(nil, image_view_profile_image_);
        break;
    }
  }

  // Expects the sync promo view to be configured
  void ExpectSyncPromoConfiguration() {
    OCMExpect([signin_promo_view_
        setMode:SigninPromoViewModeSignedInWithPrimaryAccount]);
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

  // Expects the review account settings promo view to be configured.
  void ExpectReviewAccountSettingsPromoConfiguration() {
    OCMExpect([signin_promo_view_
        setMode:SigninPromoViewModeSignedInWithPrimaryAccount]);
    OCMExpect([signin_promo_view_
        setProfileImage:[OCMArg checkWithBlock:^BOOL(id value) {
          image_view_profile_image_ = value;
          return YES;
        }]]);
    OCMExpect([signin_promo_view_
        configurePrimaryButtonWithTitle:
            GetNSString(IDS_IOS_SIGNIN_PROMO_REVIEW_SETTINGS_BUTTON)]);
    image_view_profile_image_ = nil;
  }

  // Checks a configurator with accounts on the device.
  void CheckSyncPromoWithAccountConfigurator(
      SigninPromoViewConfigurator* configurator,
      SigninPromoViewStyle style) {
    EXPECT_NE(nil, configurator);
    ExpectSyncPromoConfiguration();
    OCMExpect([close_button_ setHidden:YES]);
    OCMExpect([signin_promo_view_ setPromoViewStyle:style]);
    OCMExpect([signin_promo_view_ stopSignInSpinner]);
    [configurator configureSigninPromoView:signin_promo_view_ withStyle:style];
    EXPECT_NE(nil, image_view_profile_image_);
  }

  // Checks a configurator with a signed-in account and review account settings
  // action.
  void CheckPromoWithReviewAccountSettingsAction(
      SigninPromoViewConfigurator* configurator,
      SigninPromoViewStyle style) {
    EXPECT_NE(nil, configurator);
    ExpectReviewAccountSettingsPromoConfiguration();
    // The close button should exist on the promo when shown on the bookmarks
    // manager UI.
    OCMExpect([close_button_ setHidden:NO]);
    OCMExpect([signin_promo_view_ setPromoViewStyle:style]);
    OCMExpect([signin_promo_view_ stopSignInSpinner]);
    [configurator configureSigninPromoView:signin_promo_view_ withStyle:style];
    EXPECT_NE(nil, image_view_profile_image_);
  }

  // Checks a configurator for recent tabs.
  void CheckPromoForRecentTabs(SigninPromoViewConfigurator* configurator,
                               SigninPromoViewStyle style) {
    EXPECT_NE(nil, configurator);
    // ExpectReviewAccountSettingsPromoConfiguration();
    //  The close button should exist on the promo when shown on the bookmarks
    //  manager UI.
    OCMExpect([close_button_ setHidden:YES]);
    OCMExpect([signin_promo_view_
        setProfileImage:[OCMArg checkWithBlock:^BOOL(id value) {
          image_view_profile_image_ = value;
          return YES;
        }]]);
    OCMExpect(
        [signin_promo_view_ setMode:SigninPromoViewModeSigninWithAccount]);
    OCMExpect([signin_promo_view_
        configurePrimaryButtonWithTitle:GetNSStringF(
                                            IDS_IOS_SIGNIN_PROMO_CONTINUE_AS,
                                            SysNSStringToUTF16(
                                                identity_.userGivenName))]);
    OCMExpect([secondary_button_
        setTitle:GetNSString(IDS_IOS_SIGNIN_PROMO_CHANGE_ACCOUNT)
        forState:UIControlStateNormal]);
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
  std::unique_ptr<TestProfileIOS> profile_;

  // Mediator used for the tests.
  SigninPromoViewMediator* mediator_;

  // Identity used for sign-in.
  id<SystemIdentity> identity_;

  // Configurator received from the consumer.
  SigninPromoViewConfigurator* configurator_;

  // Mocks.
  id<SigninPresenter> signin_presenter_;
  id<AccountSettingsPresenter> account_settings_presenter_;
  id<SigninPromoViewConsumer> consumer_;
  SigninPromoView* signin_promo_view_;
  UIButton* primary_button_;
  UIButton* secondary_button_;
  UIButton* close_button_;

  // Value set by -[SigninPromoView setProfileImage:].
  UIImage* image_view_profile_image_;
};

// Tests signin promo view and its configurator with no accounts on the device.
TEST_F(SigninPromoViewMediatorTest, NoAccountsConfigureSigninPromoView) {
  CreateMediator(signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS);
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
       ConfigureCompactSigninPromoViewWithColdAndWarm) {
  CreateMediator(signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS);
  TestSigninPromoWithNoAccounts(SigninPromoViewStyleCompact);
  TestSigninPromoWithAccount(SigninPromoViewStyleCompact);
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
  CreateMediator(signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS);
  [mediator_ signinPromoViewIsVisible];
  __block ShowSigninCommand* command;
  ShowSigninCommand* command_arg =
      [OCMArg checkWithBlock:^BOOL(ShowSigninCommand* value) {
        command = value;
        return YES;
      }];
  // Start sign-in.
  OCMExpect([signin_presenter_ showSignin:command_arg]);
  OCMExpect([consumer_ promoProgressStateDidChange]);
  ExpectConfiguratorNotification(NO /* identity changed */);
  [mediator_ signinPromoViewDidTapSigninWithNewAccount:signin_promo_view_];
  EXPECT_TRUE(mediator_.showSpinner);
  EXPECT_EQ(SigninPromoViewState::kUsedAtLeastOnce,
            mediator_.signinPromoViewState);
  EXPECT_NE(nil, command.callback);
  // Stop sign-in.
  OCMExpect([consumer_ promoProgressStateDidChange]);
  OCMExpect([consumer_ signinDidFinish]);
  ExpectConfiguratorNotification(NO /* identity changed */);
  command.callback(SigninCoordinatorResultSuccess, nil);
  EXPECT_FALSE(mediator_.showSpinner);
  EXPECT_EQ(SigninPromoViewState::kUsedAtLeastOnce,
            mediator_.signinPromoViewState);
}

// Tests that no update notification is sent by the mediator to its consumer,
// while the sign-in is in progress, when an identity is added.
TEST_F(SigninPromoViewMediatorTest,
       SigninPromoViewNoUpdateNotificationWhileSignin) {
  CreateMediator(signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS);
  [mediator_ signinPromoViewIsVisible];
  __block ShowSigninCommand* command;
  ShowSigninCommand* command_arg =
      [OCMArg checkWithBlock:^BOOL(ShowSigninCommand* value) {
        command = value;
        return YES;
      }];
  OCMExpect([signin_presenter_ showSignin:command_arg]);
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
  command.callback(SigninCoordinatorResultSuccess, nil);
}

// Tests that no update notification is sent by the mediator to its consumer,
// while the sign-in is in progress.
TEST_F(SigninPromoViewMediatorTest,
       SigninPromoViewNoUpdateNotificationWhileSignin2) {
  AddDefaultIdentity();
  CreateMediator(signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS);
  [mediator_ signinPromoViewIsVisible];
  __block ShowSigninCommand* command;
  ShowSigninCommand* command_arg =
      [OCMArg checkWithBlock:^BOOL(ShowSigninCommand* value) {
        command = value;
        return YES;
      }];
  OCMExpect([signin_presenter_ showSignin:command_arg]);
  OCMExpect([consumer_ promoProgressStateDidChange]);
  ExpectConfiguratorNotification(NO /* identity changed */);
  // Starts sign-in with an identity.
  [mediator_
      signinPromoViewDidTapPrimaryButtonWithDefaultAccount:signin_promo_view_];
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
  command.callback(SigninCoordinatorResultSuccess, nil);
}

// Tests that promos aren't shown if browser sign-in is disabled by policy
TEST_F(SigninPromoViewMediatorTest,
       ShouldNotDisplaySigninPromoViewIfDisabledByPolicy) {
  CreateMediator(signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS);
  TestProfileIOS::Builder builder;
  builder.SetPrefService(CreatePrefService());
  std::unique_ptr<TestProfileIOS> profile = std::move(builder).Build();
  GetLocalState()->SetInteger(prefs::kBrowserSigninPolicy,
                              static_cast<int>(BrowserSigninMode::kDisabled));
  EXPECT_FALSE([SigninPromoViewMediator
      shouldDisplaySigninPromoViewWithAccessPoint:signin_metrics::AccessPoint::
                                                      ACCESS_POINT_RECENT_TABS
                                signinPromoAction:SigninPromoAction::
                                                      kInstantSignin
                            authenticationService:GetAuthenticationService()
                                      prefService:profile->GetPrefs()]);
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
  EXPECT_EQ(identity_, mediator_.displayedIdentity);
  fake_system_identity_manager()->WaitForServiceCallbacksToComplete();
  CheckSyncPromoWithAccountConfigurator(configurator_,
                                        SigninPromoViewStyleStandard);
}

// Tests that the sign-in promo view being removed and the mediator being
// deallocated while the sign-in is in progress, and tests the consumer is still
// called at the end of the sign-in.
TEST_F(SigninPromoViewMediatorTest,
       RemoveSigninPromoAndDeallocMediatorWhileSignedIn) {
  // Setup.
  AddDefaultIdentity();
  CreateMediator(signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS);
  __weak __typeof(mediator_) weak_mediator = mediator_;
  __block ShowSigninCommand* command;
  // This test wants to verify behavior when `mediator_` gets deallocated.
  // OCMock uses autorelease in places, which could result in `mediator_`
  // staying allocated longer than we want without this @autoreleasepool.
  @autoreleasepool {
    [mediator_ signinPromoViewIsVisible];
    ShowSigninCommand* command_arg =
        [OCMArg checkWithBlock:^BOOL(ShowSigninCommand* value) {
          command = value;
          return YES;
        }];
    OCMExpect([signin_presenter_ showSignin:command_arg]);
    OCMExpect([consumer_ promoProgressStateDidChange]);
    ExpectConfiguratorNotification(NO /* identity changed */);
    // Start sign-in with an identity.
    [mediator_ signinPromoViewDidTapPrimaryButtonWithDefaultAccount:
                   signin_promo_view_];
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
  command.callback(SigninCoordinatorResultSuccess, nil);
}

// Tests that the sign-in promo view being removed, and tests the consumer is
// still called at the end of the sign-in.
TEST_F(SigninPromoViewMediatorTest, RemoveSigninPromoWhileSignedIn) {
  // Setup.
  AddDefaultIdentity();
  CreateMediator(signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS);
  [mediator_ signinPromoViewIsVisible];
  __block ShowSigninCommand* command;
  ShowSigninCommand* command_arg =
      [OCMArg checkWithBlock:^BOOL(ShowSigninCommand* value) {
        command = value;
        return YES;
      }];
  OCMExpect([signin_presenter_ showSignin:command_arg]);
  OCMExpect([consumer_ promoProgressStateDidChange]);
  ExpectConfiguratorNotification(NO /* identity changed */);
  // Start sign-in with an identity.
  [mediator_
      signinPromoViewDidTapPrimaryButtonWithDefaultAccount:signin_promo_view_];
  // Remove the sign-in promo.
  [mediator_ disconnect];
  EXPECT_EQ(SigninPromoViewState::kInvalid, mediator_.signinPromoViewState);
  // Finish the sign-in.
  OCMExpect([consumer_ signinDidFinish]);
  command.callback(SigninCoordinatorResultSuccess, nil);
  // Set mediator_ to nil to avoid the TearDown doesn't call
  // -[mediator_ disconnect] again.
  mediator_ = nil;
}

// Tests that promo setup with kSigninWithNoDefaultIdentity creates the expected
// configurator and promo.
TEST_F(SigninPromoViewMediatorTest, SigninPromoWithSigninWithNoDefaultIdentity) {
  AddDefaultIdentity();
  CreateMediator(signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS);
  ExpectConfiguratorNotification(NO /* identity changed */);
  [mediator_ signinPromoViewIsVisible];
  ExpectConfiguratorNotification(NO /* identity changed */);
  [mediator_
      setSigninPromoAction:SigninPromoAction::kSigninWithNoDefaultIdentity];
  EXPECT_EQ(identity_, mediator_.displayedIdentity);
  fake_system_identity_manager()->WaitForServiceCallbacksToComplete();
  CheckPromoForRecentTabs(configurator_, SigninPromoViewStyleStandard);
}

// Tests that promo setup with review account settings promo action.
TEST_F(SigninPromoViewMediatorTest,
       SigninPromoWithReviewAccountSettingsAction) {
  AddDefaultIdentity();
  identity_ = [FakeSystemIdentity fakeIdentity2];
  fake_system_identity_manager()->AddIdentity(identity_);
  GetAuthenticationService()->SignIn(
      identity_, signin_metrics::AccessPoint::ACCESS_POINT_SIGNIN_PROMO);

  CreateMediator(signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER);
  ExpectConfiguratorNotification(NO /* identity changed */);
  [mediator_ signinPromoViewIsVisible];
  ExpectConfiguratorNotification(NO /* identity changed */);
  [mediator_ setSigninPromoAction:SigninPromoAction::kReviewAccountSettings];
  EXPECT_EQ(identity_, mediator_.displayedIdentity);
  fake_system_identity_manager()->WaitForServiceCallbacksToComplete();
  CheckPromoWithReviewAccountSettingsAction(configurator_,
                                            SigninPromoViewStyleStandard);

  OCMExpect([account_settings_presenter_ showAccountSettings]);
  [mediator_
      signinPromoViewDidTapPrimaryButtonWithDefaultAccount:signin_promo_view_];
  EXPECT_EQ(SigninPromoViewState::kUsedAtLeastOnce,
            mediator_.signinPromoViewState);
}

// Tests that review settings promo is not shown if the user has already
// dismissed it, but the signin promo should not be affected.
TEST_F(SigninPromoViewMediatorTest,
       ShouldNotDisplaySigninPromoViewIfAlreadySeen) {
  CreateMediator(signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER);
  TestProfileIOS::Builder builder;
  builder.SetPrefService(CreatePrefService());
  std::unique_ptr<TestProfileIOS> profile = std::move(builder).Build();
  profile->GetPrefs()->SetBoolean(prefs::kIosBookmarkSettingsPromoAlreadySeen,
                                  true);
  EXPECT_FALSE([SigninPromoViewMediator
      shouldDisplaySigninPromoViewWithAccessPoint:
          signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER
                                signinPromoAction:SigninPromoAction::
                                                      kReviewAccountSettings
                            authenticationService:GetAuthenticationService()
                                      prefService:profile->GetPrefs()]);
  EXPECT_TRUE([SigninPromoViewMediator
      shouldDisplaySigninPromoViewWithAccessPoint:
          signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER
                                signinPromoAction:SigninPromoAction::
                                                      kInstantSignin
                            authenticationService:GetAuthenticationService()
                                      prefService:profile->GetPrefs()]);
}

}  // namespace
