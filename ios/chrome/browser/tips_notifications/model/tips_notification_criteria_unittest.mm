// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tips_notifications/model/tips_notification_criteria.h"

#import "base/test/scoped_feature_list.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "components/omnibox/browser/omnibox_pref_names.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/prefs/pref_service.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/search_engines/template_url_service.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/sync/base/features.h"
#import "components/sync/test/mock_sync_service.h"
#import "ios/chrome/browser/default_browser/model/promo_source.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/default_browser/model/utils_test_support.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/ntp/model/set_up_list_prefs.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/utils/first_run_test_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/sync/model/mock_sync_service_utils.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/tips_notifications/model/utils.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/device_form_factor.h"

namespace {

using ::testing::Return;

// Creates the Feature Engagement Mock Tracker.
std::unique_ptr<KeyedService> BuildFeatureEngagementMockTracker(
    ProfileIOS* profile) {
  return std::make_unique<feature_engagement::test::MockTracker>();
}

}  // namespace
class TipsNotificationCriteriaTest : public PlatformTest {
 protected:
  TipsNotificationCriteriaTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    builder.AddTestingFactory(
        feature_engagement::TrackerFactory::GetInstance(),
        base::BindRepeating(&BuildFeatureEngagementMockTracker));
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));
    builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();
    mock_tracker_ = static_cast<feature_engagement::test::MockTracker*>(
        feature_engagement::TrackerFactory::GetForProfile(profile_.get()));
    TemplateURLService* template_url_service =
        ios::TemplateURLServiceFactory::GetForProfile(profile_.get());
    template_url_service->Load();
    criteria_ = std::make_unique<TipsNotificationCriteria>(profile_.get(),
                                                           GetLocalState());
    sync_service_mock_ = static_cast<syncer::MockSyncService*>(
        SyncServiceFactory::GetForProfile(profile_.get()));
  }

  PrefService* GetLocalState() {
    return GetApplicationContext()->GetLocalState();
  }

  AuthenticationService* GetAuthService() {
    return AuthenticationServiceFactory::GetForProfile(profile_.get());
  }

  // Ensures that Chrome is considered as default browser.
  void SetTrueChromeLikelyDefaultBrowser() { LogOpenHTTPURLFromExternalURL(); }

  // Ensures that Chrome is not considered as default browser.
  void SetFalseChromeLikelyDefaultBrowser() { ClearDefaultBrowserPromoData(); }

  void SignIn() {
    FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(fake_identity);
    GetAuthService()->SignIn(fake_identity,
                             signin_metrics::AccessPoint::kUnknown);
  }

  void SetIsGoogleDefaultSearchEngine(bool is_google) {
    TemplateURLService* template_url_service =
        ios::TemplateURLServiceFactory::GetForProfile(profile_.get());
    if (is_google) {
      template_url_service->SetUserSelectedDefaultSearchProvider(
          template_url_service->GetTemplateURLForKeyword(u"google.com"));
    } else {
      template_url_service->SetUserSelectedDefaultSearchProvider(
          template_url_service->GetTemplateURLForKeyword(u"duckduckgo.com"));
    }
  }

  base::test::ScopedFeatureList feature_list_;
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  raw_ptr<feature_engagement::test::MockTracker, DanglingUntriaged>
      mock_tracker_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TipsNotificationCriteria> criteria_;
  raw_ptr<syncer::MockSyncService> sync_service_mock_ = nullptr;
};

#pragma mark - ShouldSendDefaultBrowser

// Tests that the Default Browser notification should not be sent when Chrome is
// already the default.
TEST_F(TipsNotificationCriteriaTest, TestShouldSendDefaultBrowser_Default) {
  SetTrueChromeLikelyDefaultBrowser();
  EXPECT_FALSE(
      criteria_->ShouldSendNotification(TipsNotificationType::kDefaultBrowser));
}

// Tests that the Default Browser notification should not be sent when the user
// has previously canceled the promo.
TEST_F(TipsNotificationCriteriaTest,
       TestShouldSendDefaultBrowser_PromoCanceled) {
  feature_list_.InitAndDisableFeature(kIOSOneTimeDefaultBrowserNotification);
  SetFalseChromeLikelyDefaultBrowser();
  RecordDefaultBrowserPromoLastAction(IOSDefaultBrowserPromoAction::kCancel);
  EXPECT_FALSE(
      criteria_->ShouldSendNotification(TipsNotificationType::kDefaultBrowser));
}

// Tests that the Default Browser notification should be sent when the user has
// previously dismissed the promo.
TEST_F(TipsNotificationCriteriaTest,
       TestShouldSendDefaultBrowser_PromoDismissed) {
  feature_list_.InitAndDisableFeature(kIOSOneTimeDefaultBrowserNotification);
  SetFalseChromeLikelyDefaultBrowser();
  RecordDefaultBrowserPromoLastAction(IOSDefaultBrowserPromoAction::kDismiss);
  EXPECT_TRUE(
      criteria_->ShouldSendNotification(TipsNotificationType::kDefaultBrowser));
}

// Tests that the Default Browser notification should be sent when the user has
// previously tapped "Remind Me Later".
TEST_F(TipsNotificationCriteriaTest,
       TestShouldSendDefaultBrowser_PromoRemindMeLater) {
  feature_list_.InitAndDisableFeature(kIOSOneTimeDefaultBrowserNotification);
  SetFalseChromeLikelyDefaultBrowser();
  RecordDefaultBrowserPromoLastAction(
      IOSDefaultBrowserPromoAction::kRemindMeLater);
  EXPECT_TRUE(
      criteria_->ShouldSendNotification(TipsNotificationType::kDefaultBrowser));
}

// Tests that the Default Browser notification should be sent when the user has
// not interacted with the promo and Chrome is not the default.
TEST_F(TipsNotificationCriteriaTest, TestShouldSendDefaultBrowser_NoAction) {
  feature_list_.InitAndDisableFeature(kIOSOneTimeDefaultBrowserNotification);
  SetFalseChromeLikelyDefaultBrowser();
  EXPECT_TRUE(
      criteria_->ShouldSendNotification(TipsNotificationType::kDefaultBrowser));
}

// Tests that the Default Browser notification should not be sent if a default
// browser promo has been seen in the last 2 weeks.
TEST_F(TipsNotificationCriteriaTest,
       TestShouldSendDefaultBrowser_ShouldNotTriggerOneTime) {
  feature_list_.InitAndEnableFeature(kIOSOneTimeDefaultBrowserNotification);
  SetFalseChromeLikelyDefaultBrowser();
  RecordDefaultBrowserPromoLastAction(IOSDefaultBrowserPromoAction::kCancel);
  EXPECT_CALL(
      *mock_tracker_,
      WouldTriggerHelpUI(testing::Ref(
          feature_engagement::kIPHiOSOneTimeDefaultBrowserNotificationFeature)))
      .WillOnce(testing::Return(false));
  EXPECT_FALSE(
      criteria_->ShouldSendNotification(TipsNotificationType::kDefaultBrowser));
}

// Tests that the Default Browser notification should be sent if a default
// browser promo has not been seen in the last 2 weeks.
TEST_F(TipsNotificationCriteriaTest,
       TestShouldSendDefaultBrowser_ShouldTriggerOneTime) {
  feature_list_.InitAndEnableFeature(kIOSOneTimeDefaultBrowserNotification);
  SetFalseChromeLikelyDefaultBrowser();
  RecordDefaultBrowserPromoLastAction(IOSDefaultBrowserPromoAction::kCancel);
  EXPECT_CALL(
      *mock_tracker_,
      WouldTriggerHelpUI(testing::Ref(
          feature_engagement::kIPHiOSOneTimeDefaultBrowserNotificationFeature)))
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(
      criteria_->ShouldSendNotification(TipsNotificationType::kDefaultBrowser));
}

#pragma mark - ShouldSendSignin

// Tests that the Sign-in notification should be sent when sign-in is allowed
// and the user is not signed in.
TEST_F(TipsNotificationCriteriaTest, TestShouldSendSignin_Allowed) {
  GetLocalState()->SetBoolean(prefs::kSigninAllowedOnDevice, true);
  EXPECT_TRUE(criteria_->ShouldSendNotification(TipsNotificationType::kSignin));
}

// Tests that the Sign-in notification should not be sent when sign-in is
// allowed but the user is already signed in.
TEST_F(TipsNotificationCriteriaTest, TestShouldSendSignin_Allowed_SignedIn) {
  GetLocalState()->SetBoolean(prefs::kSigninAllowedOnDevice, true);
  EXPECT_EQ(GetAuthService()->GetServiceStatus(),
            AuthenticationService::ServiceStatus::SigninAllowed);
  SignIn();
  EXPECT_FALSE(
      criteria_->ShouldSendNotification(TipsNotificationType::kSignin));
}

// Tests that the Sign-in notification should be sent when sign-in is forced by
// policy and the user is not signed in.
TEST_F(TipsNotificationCriteriaTest, TestShouldSendSignin_Forced) {
  // Set sign-in forced by policy.
  GetLocalState()->SetInteger(prefs::kBrowserSigninPolicy,
                              static_cast<int>(BrowserSigninMode::kForced));
  EXPECT_EQ(GetAuthService()->GetServiceStatus(),
            AuthenticationService::ServiceStatus::SigninForcedByPolicy);
  EXPECT_TRUE(criteria_->ShouldSendNotification(TipsNotificationType::kSignin));
}

// Tests that the Sign-in notification should not be sent when sign-in is
// forced by policy but the user is already signed in.
TEST_F(TipsNotificationCriteriaTest, TestShouldSendSignin_Forced_SignedIn) {
  // Set sign-in forced by policy.
  GetLocalState()->SetInteger(prefs::kBrowserSigninPolicy,
                              static_cast<int>(BrowserSigninMode::kForced));
  EXPECT_EQ(GetAuthService()->GetServiceStatus(),
            AuthenticationService::ServiceStatus::SigninForcedByPolicy);
  SignIn();
  EXPECT_FALSE(
      criteria_->ShouldSendNotification(TipsNotificationType::kSignin));
}

// Tests that the Sign-in notification should not be sent when sign-in is
// disabled by the user.
TEST_F(TipsNotificationCriteriaTest, TestShouldSendSignin_DisabledByUser) {
  GetLocalState()->SetBoolean(prefs::kSigninAllowedOnDevice, false);
  EXPECT_FALSE(
      criteria_->ShouldSendNotification(TipsNotificationType::kSignin));
}

#pragma mark - ShouldSendWhatsNew

// Tests that the "What's New" notification should not be sent if the Feature
// Engagement Tracker has already been triggered for it.
TEST_F(TipsNotificationCriteriaTest, TestShouldSendWhatsNew_Triggered) {
  EXPECT_CALL(
      *mock_tracker_,
      HasEverTriggered(
          testing::Ref(feature_engagement::kIPHWhatsNewUpdatedFeature), true))
      .WillOnce(testing::Return(true));
  EXPECT_FALSE(
      criteria_->ShouldSendNotification(TipsNotificationType::kWhatsNew));
}

// Tests that the "What's New" notification should be sent if the Feature
// Engagement Tracker has not been triggered for it.
TEST_F(TipsNotificationCriteriaTest, TestShouldSendWhatsNew_NotTriggered) {
  EXPECT_CALL(
      *mock_tracker_,
      HasEverTriggered(
          testing::Ref(feature_engagement::kIPHWhatsNewUpdatedFeature), true))
      .WillOnce(testing::Return(false));
  EXPECT_TRUE(
      criteria_->ShouldSendNotification(TipsNotificationType::kWhatsNew));
}

#pragma mark - ShouldSendSetUpListContinuation

// Tests that the "Set Up List Continuation" notification should not be sent if
// the first run was not recent enough.
TEST_F(TipsNotificationCriteriaTest,
       TestShouldSendSetUpListContinuation_FirstRunNotRecent) {
  ForceFirstRunRecency(30);
  EXPECT_FALSE(criteria_->ShouldSendNotification(
      TipsNotificationType::kSetUpListContinuation));
}

// Tests that the "Set Up List Continuation" notification should not be sent if
// all items in the Set Up List are already complete.
TEST_F(TipsNotificationCriteriaTest,
       TestShouldSendSetUpListContinuation_AllItemsComplete) {
  ForceFirstRunRecency(0);
  set_up_list_prefs::MarkAllItemsComplete(GetLocalState());
  EXPECT_FALSE(criteria_->ShouldSendNotification(
      TipsNotificationType::kSetUpListContinuation));
}

// Tests that the "Set Up List Continuation" notification should be sent when
// all conditions are met.
TEST_F(TipsNotificationCriteriaTest, TestShouldSendSetUpListContinuation_OK) {
  ForceFirstRunRecency(0);
  EXPECT_TRUE(criteria_->ShouldSendNotification(
      TipsNotificationType::kSetUpListContinuation));
}

#pragma mark - ShouldSendDocking

// Tests that the Docking notification should not be sent if the Feature
// Engagement Tracker has already been triggered for the main promo feature.
TEST_F(TipsNotificationCriteriaTest, TestShouldSendDocking_Triggered) {
  EXPECT_CALL(
      *mock_tracker_,
      HasEverTriggered(
          testing::Ref(feature_engagement::kIPHiOSDockingPromoFeature), true))
      .WillOnce(testing::Return(true));
  EXPECT_FALSE(
      criteria_->ShouldSendNotification(TipsNotificationType::kDocking));
}

// Tests that the Docking notification should be sent if the Feature Engagement
// Tracker has not been triggered for either the main promo or the "Remind Me
// Later" feature.
TEST_F(TipsNotificationCriteriaTest, TestShouldSendDocking_NotTriggered) {
  EXPECT_CALL(
      *mock_tracker_,
      HasEverTriggered(
          testing::Ref(feature_engagement::kIPHiOSDockingPromoFeature), true))
      .WillOnce(testing::Return(false));
  EXPECT_CALL(
      *mock_tracker_,
      HasEverTriggered(
          testing::Ref(
              feature_engagement::kIPHiOSDockingPromoRemindMeLaterFeature),
          true))
      .WillOnce(testing::Return(false));
  EXPECT_TRUE(
      criteria_->ShouldSendNotification(TipsNotificationType::kDocking));
}

// Tests that the Docking notification should not be sent if the Feature
// Engagement Tracker has been triggered for the "Remind Me Later" feature.
TEST_F(TipsNotificationCriteriaTest,
       TestShouldSendDocking_RemindMeLaterTriggered) {
  EXPECT_CALL(
      *mock_tracker_,
      HasEverTriggered(
          testing::Ref(feature_engagement::kIPHiOSDockingPromoFeature), true))
      .WillOnce(testing::Return(false));
  EXPECT_CALL(
      *mock_tracker_,
      HasEverTriggered(
          testing::Ref(
              feature_engagement::kIPHiOSDockingPromoRemindMeLaterFeature),
          true))
      .WillOnce(testing::Return(true));
  EXPECT_FALSE(
      criteria_->ShouldSendNotification(TipsNotificationType::kDocking));
}

#pragma mark - ShouldSendOmniboxPosition

// Tests that the Omnibox Position notification should not be sent when the user
// has already made a choice.
TEST_F(TipsNotificationCriteriaTest,
       TestShouldSendOmniboxPosition_ShouldNotSend) {
  GetLocalState()->SetBoolean(omnibox::kIsOmniboxInBottomPosition, true);
  EXPECT_FALSE(criteria_->ShouldSendNotification(
      TipsNotificationType::kOmniboxPosition));
}

// Tests that the Omnibox Position notification should be sent on a phone when
// the user has not yet made a choice.
TEST_F(TipsNotificationCriteriaTest, TestShouldSendOmniboxPosition_ShouldSend) {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    GTEST_SKIP() << "Test is running on a tablet, skipping.";
  }
  // Not setting the kBottomOmnibox pref should cause the criteria to be met.
  EXPECT_TRUE(criteria_->ShouldSendNotification(
      TipsNotificationType::kOmniboxPosition));
}

// Tests that the Omnibox Position notification should not be sent on a tablet,
// regardless of the preference.
TEST_F(TipsNotificationCriteriaTest,
       TestShouldSendOmniboxPosition_ShouldNotSendOnTablet) {
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET) {
    GTEST_SKIP() << "Test is running on a phone, skipping.";
  }
  GetLocalState()->SetBoolean(omnibox::kIsOmniboxInBottomPosition, false);
  EXPECT_FALSE(criteria_->ShouldSendNotification(
      TipsNotificationType::kOmniboxPosition));
}

#pragma mark - ShouldSendLens

// Tests that the Lens notification is not sent in unit tests, as Lens is not
// supported in this environment.
TEST_F(TipsNotificationCriteriaTest,
       TestShouldSendLens_ReturnsFalseInUnitTests) {
  // Lens is not supported in unit tests, so this should always be false.
  SetIsGoogleDefaultSearchEngine(true);
  GetLocalState()->SetTime(prefs::kLensLastOpened,
                           base::Time::Now() - base::Days(31));
  EXPECT_FALSE(criteria_->ShouldSendNotification(TipsNotificationType::kLens));
}

#pragma mark - ShouldSendEnhancedSafeBrowsing

// Tests that the Enhanced Safe Browsing notification should be sent when
// Advanced Protection is allowed but ESB is not enabled.
TEST_F(TipsNotificationCriteriaTest,
       TestShouldSendEnhancedSafeBrowsing_ShouldSend) {
  profile_->GetPrefs()->SetBoolean(prefs::kAdvancedProtectionAllowed, true);
  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, false);
  EXPECT_TRUE(criteria_->ShouldSendNotification(
      TipsNotificationType::kEnhancedSafeBrowsing));
}

// Tests that the Enhanced Safe Browsing notification should not be sent when it
// is already enabled.
TEST_F(TipsNotificationCriteriaTest,
       TestShouldSendEnhancedSafeBrowsing_AlreadyEnabled) {
  profile_->GetPrefs()->SetBoolean(prefs::kAdvancedProtectionAllowed, true);
  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, true);
  EXPECT_FALSE(criteria_->ShouldSendNotification(
      TipsNotificationType::kEnhancedSafeBrowsing));
}

// Tests that the Enhanced Safe Browsing notification should not be sent when
// Advanced Protection is not allowed.
TEST_F(TipsNotificationCriteriaTest,
       TestShouldSendEnhancedSafeBrowsing_NotAllowed) {
  profile_->GetPrefs()->SetBoolean(prefs::kAdvancedProtectionAllowed, false);
  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, false);
  EXPECT_FALSE(criteria_->ShouldSendNotification(
      TipsNotificationType::kEnhancedSafeBrowsing));
}

#pragma mark - ShouldSendCPE

// Tests that the CPE notification should be sent when all conditions are met.
TEST_F(TipsNotificationCriteriaTest, TestShouldSendCPE_ShouldSend) {
  GetLocalState()->SetBoolean(prefs::kIosCredentialProviderPromoPolicyEnabled,
                              true);
  GetLocalState()->SetBoolean(
      password_manager::prefs::kCredentialProviderEnabledOnStartup, false);
  GetLocalState()->SetTime(prefs::kIosCredentialProviderPromoDisplayTime,
                           base::Time::Now() - base::Days(8));
  GetLocalState()->SetTime(prefs::kIosSuccessfulLoginWithExistingPassword,
                           base::Time::Now() - base::Days(1));
  EXPECT_TRUE(criteria_->ShouldSendNotification(TipsNotificationType::kCPE));
}

// Tests that the CPE notification should not be sent when the policy is
// disabled.
TEST_F(TipsNotificationCriteriaTest, TestShouldSendCPE_PolicyDisabled) {
  GetLocalState()->SetBoolean(prefs::kIosCredentialProviderPromoPolicyEnabled,
                              false);
  EXPECT_FALSE(criteria_->ShouldSendNotification(TipsNotificationType::kCPE));
}

// Tests that the CPE notification should not be sent when the CPE is already
// enabled.
TEST_F(TipsNotificationCriteriaTest, TestShouldSendCPE_CPEEnabled) {
  GetLocalState()->SetBoolean(prefs::kIosCredentialProviderPromoPolicyEnabled,
                              true);
  GetLocalState()->SetBoolean(
      password_manager::prefs::kCredentialProviderEnabledOnStartup, true);
  EXPECT_FALSE(criteria_->ShouldSendNotification(TipsNotificationType::kCPE));
}

// Tests that the CPE notification should not be sent when the promo was shown
// too recently.
TEST_F(TipsNotificationCriteriaTest, TestShouldSendCPE_PromoTooRecent) {
  GetLocalState()->SetBoolean(prefs::kIosCredentialProviderPromoPolicyEnabled,
                              true);
  GetLocalState()->SetBoolean(
      password_manager::prefs::kCredentialProviderEnabledOnStartup, false);
  GetLocalState()->SetTime(prefs::kIosCredentialProviderPromoDisplayTime,
                           base::Time::Now());
  EXPECT_FALSE(criteria_->ShouldSendNotification(TipsNotificationType::kCPE));
}

// Tests that the CPE notification should not be sent when there has been no
// recent successful login.
TEST_F(TipsNotificationCriteriaTest, TestShouldSendCPE_LoginNotRecent) {
  GetLocalState()->SetBoolean(prefs::kIosCredentialProviderPromoPolicyEnabled,
                              true);
  GetLocalState()->SetBoolean(
      password_manager::prefs::kCredentialProviderEnabledOnStartup, false);
  GetLocalState()->SetTime(prefs::kIosCredentialProviderPromoDisplayTime,
                           base::Time::Now() - base::Days(8));
  GetLocalState()->SetTime(prefs::kIosSuccessfulLoginWithExistingPassword,
                           base::Time::Now() - base::Days(31));
  EXPECT_FALSE(criteria_->ShouldSendNotification(TipsNotificationType::kCPE));
}

#pragma mark - ShouldSendLensOverlay

// Tests that the Lens Overlay notification should not be sent when it has been
// presented recently.
TEST_F(TipsNotificationCriteriaTest, TestShouldSendLensOverlay_ShouldNotSend) {
  GetLocalState()->SetTime(prefs::kLensOverlayLastPresented, base::Time::Now());
  EXPECT_FALSE(
      criteria_->ShouldSendNotification(TipsNotificationType::kLensOverlay));
}

// Tests that the Lens Overlay notification should be sent when it has not been
// presented recently.
TEST_F(TipsNotificationCriteriaTest, TestShouldSendLensOverlay_ShouldSend) {
  GetLocalState()->SetTime(prefs::kLensOverlayLastPresented,
                           base::Time::Now() - base::Days(31));
  EXPECT_TRUE(
      criteria_->ShouldSendNotification(TipsNotificationType::kLensOverlay));
}

#pragma mark - ShouldSendTrustedVaultKeyRetrieval

// Tests that the trusted vault notification should not be sent when the flag is
// disabled
TEST_F(TipsNotificationCriteriaTest,
       TestShouldSendTrustedVaultKeyRetrieval_NotSent_WhenFlagIsDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kIOSTrustedVaultNotification);

  EXPECT_FALSE(criteria_->ShouldSendNotification(
      TipsNotificationType::kTrustedVaultKeyRetrieval));
}

// Tests that the trusted vault notification should be sent when the flag is
// enabled and the trusted vault key is missing
TEST_F(TipsNotificationCriteriaTest,
       TestShouldSendTrustedVaultKeyRetrieval_Sent_WhenKeyIsMissing) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kIOSTrustedVaultNotification);
  ON_CALL(*(sync_service_mock_->GetMockUserSettings()),
          IsTrustedVaultKeyRequiredForPreferredDataTypes())
      .WillByDefault(Return(true));

  EXPECT_TRUE(criteria_->ShouldSendNotification(
      TipsNotificationType::kTrustedVaultKeyRetrieval));
}

// Tests that the trusted vault notification should be sent when the flag is
// enabled and the trusted vault key is available
TEST_F(TipsNotificationCriteriaTest,
       TestShouldSendTrustedVaultKeyRetrieval_NotSent_WhenKeyIsAvailable) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kIOSTrustedVaultNotification);
  ON_CALL(*(sync_service_mock_->GetMockUserSettings()),
          IsTrustedVaultKeyRequiredForPreferredDataTypes())
      .WillByDefault(Return(false));

  EXPECT_FALSE(criteria_->ShouldSendNotification(
      TipsNotificationType::kTrustedVaultKeyRetrieval));
}
