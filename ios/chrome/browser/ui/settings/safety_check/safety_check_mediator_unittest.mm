// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/safety_check/safety_check_mediator.h"

#import <memory>

#import "base/memory/ptr_util.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/bind.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/affiliations/core/browser/fake_affiliation_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/password_store/test_password_store.h"
#import "components/password_manager/core/browser/ui/password_check_referrer.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/testing_pref_service.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/safety_check/safety_check.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/test/mock_sync_service.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "ios/chrome/browser/affiliations/model/ios_chrome_affiliation_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/password_check_observer_bridge.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/settings/cells/settings_check_item.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_constants.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_consumer.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_mediator+Testing.h"
#import "ios/chrome/browser/upgrade/model/upgrade_constants.h"
#import "ios/chrome/browser/upgrade/model/upgrade_recommended_details.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/time_format.h"

namespace {

using l10n_util::GetNSString;
using password_manager::InsecureType;
using password_manager::TestPasswordStore;

typedef NS_ENUM(NSInteger, SafetyCheckItemType) {
  // CheckTypes section.
  UpdateItemType = kItemTypeEnumZero,
  PasswordItemType,
  SafeBrowsingItemType,
  HeaderItem,
  // CheckStart section.
  CheckStartItemType,
  TimestampFooterItem,
};

// The size of trailing symbol icons.
NSInteger kTrailingSymbolImagePointSize = 22;

// Registers account preference that will be used for Safe Browsing.
PrefService* SetPrefService() {
  TestingPrefServiceSimple* prefs = new TestingPrefServiceSimple();
  PrefRegistrySimple* registry = prefs->registry();
  registry->RegisterBooleanPref(prefs::kSafeBrowsingEnabled, true);
  registry->RegisterBooleanPref(prefs::kSafeBrowsingEnhanced, true);
  return prefs;
}

// The image when the state is safe.
UIImage* SafeImage() {
  return DefaultSymbolTemplateWithPointSize(kCheckmarkCircleFillSymbol,
                                            kTrailingSymbolImagePointSize);
}

// The image when the state is unsafe.
UIImage* UnsafeImage() {
  return DefaultSymbolTemplateWithPointSize(kErrorCircleFillSymbol,
                                            kTrailingSymbolImagePointSize);
}

// The color when the state is safe.
UIColor* GreenColor() {
  return [UIColor colorNamed:kGreen500Color];
}

// The color when the state is unsafe.
UIColor* RedColor() {
  return [UIColor colorNamed:kRed500Color];
}

}  // namespace

class SafetyCheckMediatorTest : public PlatformTest {
 public:
  SafetyCheckMediatorTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        SyncServiceFactory::GetInstance(),
        base::BindRepeating(
            [](web::BrowserState*) -> std::unique_ptr<KeyedService> {
              return std::make_unique<syncer::MockSyncService>();
            }));
    builder.AddTestingFactory(
        IOSChromeProfilePasswordStoreFactory::GetInstance(),
        base::BindRepeating(
            &password_manager::BuildPasswordStore<web::BrowserState,
                                                  TestPasswordStore>));
    builder.AddTestingFactory(
        IOSChromeAffiliationServiceFactory::GetInstance(),
        base::BindRepeating(base::BindLambdaForTesting([](web::BrowserState*) {
          return std::unique_ptr<KeyedService>(
              std::make_unique<affiliations::FakeAffiliationService>());
        })));

    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));

    AuthenticationServiceFactory::CreateAndInitializeForProfile(
        profile_.get(), std::make_unique<FakeAuthenticationServiceDelegate>());
    auth_service_ = static_cast<AuthenticationService*>(
        AuthenticationServiceFactory::GetInstance()->GetForProfile(
            profile_.get()));

    store_ =
        base::WrapRefCounted(static_cast<password_manager::TestPasswordStore*>(
            IOSChromeProfilePasswordStoreFactory::GetForProfile(
                profile_.get(), ServiceAccessType::EXPLICIT_ACCESS)
                .get()));

    password_check_ =
        IOSChromePasswordCheckManagerFactory::GetForProfile(profile_.get());

    pref_service_ = SetPrefService();
    local_pref_service_ =
        TestingApplicationContext::GetGlobal()->GetLocalState();

    mediator_ = [[SafetyCheckMediator alloc]
        initWithUserPrefService:pref_service_
               localPrefService:local_pref_service_
           passwordCheckManager:password_check_
                    authService:auth_service_
                    syncService:syncService()
                       referrer:password_manager::PasswordCheckReferrer::
                                    kSafetyCheck];
  }

  syncer::SyncService* syncService() {
    return SyncServiceFactory::GetForProfile(profile_.get());
  }

  void RunUntilIdle() { environment_.RunUntilIdle(); }

  void AddPasswordForm(std::unique_ptr<password_manager::PasswordForm> form) {
    GetTestStore().AddLogin(*form);
    RunUntilIdle();
  }

  void ResetNSUserDefaultsForTesting() {
    NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
    [defaults removeObjectForKey:kTimestampOfLastIssueFoundKey];
    [defaults removeObjectForKey:kIOSChromeUpToDateKey];
  }

  void ResetLocalPrefsForTesting() {
    local_pref_service_->ClearPref(prefs::kIosSettingsSafetyCheckLastRunTime);
    local_pref_service_->ClearPref(kIOSChromeNextVersionKey);
    local_pref_service_->ClearPref(kIOSChromeUpgradeURLKey);
  }

  // Creates a form.
  std::unique_ptr<password_manager::PasswordForm> CreateForm(
      std::string signon_realm = "http://www.example.com/") {
    auto form = std::make_unique<password_manager::PasswordForm>();
    form->url = GURL(signon_realm + "accounts/LoginAuth");
    form->action = GURL(signon_realm + "accounts/Login");
    form->username_element = u"Email";
    form->username_value = u"test@egmail.com";
    form->password_element = u"Passwd";
    form->password_value = u"fnlsr4@cm^mdls@fkspnsg3d";
    form->submit_element = u"signIn";
    form->signon_realm = signon_realm;
    form->scheme = password_manager::PasswordForm::Scheme::kHtml;
    form->blocked_by_user = false;
    return form;
  }

  // Creates and adds a saved password form. If `is_leaked` is true it marks the
  // credential as leaked.
  void AddSavedForm() {
    auto form = CreateForm();
    AddPasswordForm(std::move(form));
  }

  // Creates and adds a saved insecure password form.
  void AddSavedInsecureForm(
      InsecureType insecure_type,
      bool is_muted = false,
      std::string signon_realm = "http://www.example.com/") {
    auto form = CreateForm(signon_realm);
    form->password_issues = {
        {insecure_type,
         password_manager::InsecurityMetadata(
             base::Time::Now(), password_manager::IsMuted(is_muted),
             password_manager::TriggerBackendNotification(false))}};
    AddPasswordForm(std::move(form));
  }

  TestPasswordStore& GetTestStore() {
    return *static_cast<TestPasswordStore*>(
        IOSChromeProfilePasswordStoreFactory::GetForProfile(
            profile_.get(), ServiceAccessType::EXPLICIT_ACCESS)
            .get());
  }

  // Tests that only having a leaked password results in an umuted compromised
  // password row state.
  void CheckCompromisedRowState() {
    base::HistogramTester histogram_tester;

    AddSavedInsecureForm(InsecureType::kLeaked);
    mediator_.currentPasswordCheckState = PasswordCheckState::kRunning;
    [mediator_ passwordCheckStateDidChange:PasswordCheckState::kIdle];

    histogram_tester.ExpectUniqueSample(
        kSafetyCheckMetricsPasswords,
        safety_check::PasswordsStatus::kCompromisedExist, 1);

    EXPECT_EQ(mediator_.passwordCheckRowState,
              PasswordCheckRowStateUnmutedCompromisedPasswords);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  web::WebTaskEnvironment environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  raw_ptr<ProfileIOS> profile_;
  scoped_refptr<TestPasswordStore> store_;
  raw_ptr<AuthenticationService> auth_service_;
  scoped_refptr<IOSChromePasswordCheckManager> password_check_;
  TestProfileManagerIOS profile_manager_;
  SafetyCheckMediator* mediator_;
  raw_ptr<PrefService> pref_service_;
  raw_ptr<PrefService> local_pref_service_;
  PrefBackedBoolean* safe_browsing_preference_;
};

#pragma mark - Check start button tests

TEST_F(SafetyCheckMediatorTest, StartingCheckPutsChecksInRunningState) {
  TableViewItem* start =
      [[TableViewItem alloc] initWithType:CheckStartItemType];
  [mediator_ didSelectItem:start];
  EXPECT_EQ(mediator_.updateCheckRowState, UpdateCheckRowStateRunning);
  EXPECT_EQ(mediator_.passwordCheckRowState, PasswordCheckRowStateRunning);
  EXPECT_EQ(mediator_.safeBrowsingCheckRowState,
            SafeBrowsingCheckRowStateRunning);
  EXPECT_EQ(mediator_.checkStartState, CheckStartStateCancel);
}

TEST_F(SafetyCheckMediatorTest, CheckStartButtonDefaultUI) {
  mediator_.checkStartState = CheckStartStateDefault;
  [mediator_ reconfigureCheckStartSection];
  EXPECT_NSEQ(mediator_.checkStartItem.text,
              GetNSString(IDS_IOS_CHECK_PASSWORDS_NOW_BUTTON));
}

TEST_F(SafetyCheckMediatorTest, ClickingCancelTakesChecksToPrevious) {
  TableViewItem* start =
      [[TableViewItem alloc] initWithType:CheckStartItemType];
  [mediator_ didSelectItem:start];
  EXPECT_EQ(mediator_.updateCheckRowState, UpdateCheckRowStateRunning);
  EXPECT_EQ(mediator_.passwordCheckRowState, PasswordCheckRowStateRunning);
  EXPECT_EQ(mediator_.safeBrowsingCheckRowState,
            SafeBrowsingCheckRowStateRunning);
  EXPECT_EQ(mediator_.checkStartState, CheckStartStateCancel);
  [mediator_ didSelectItem:start];
  EXPECT_EQ(mediator_.updateCheckRowState,
            mediator_.previousUpdateCheckRowState);
  EXPECT_EQ(mediator_.passwordCheckRowState,
            mediator_.previousPasswordCheckRowState);
  EXPECT_EQ(mediator_.safeBrowsingCheckRowState,
            mediator_.previousSafeBrowsingCheckRowState);
  EXPECT_EQ(mediator_.checkStartState, CheckStartStateDefault);
}

TEST_F(SafetyCheckMediatorTest, CheckStartButtonCancelUI) {
  mediator_.checkStartState = CheckStartStateCancel;
  [mediator_ reconfigureCheckStartSection];
  EXPECT_NSEQ(mediator_.checkStartItem.text,
              GetNSString(IDS_IOS_CANCEL_PASSWORD_CHECK_BUTTON));
}

#pragma mark - Timestamp tests

TEST_F(SafetyCheckMediatorTest, TimestampSetIfIssueFound) {
  mediator_.checkDidRun = true;
  mediator_.passwordCheckRowState =
      PasswordCheckRowStateUnmutedCompromisedPasswords;
  [mediator_ resetsCheckStartItemIfNeeded];

  base::Time lastCompletedCheck = base::Time::FromSecondsSinceUnixEpoch(
      [[NSUserDefaults standardUserDefaults]
          doubleForKey:kTimestampOfLastIssueFoundKey]);
  EXPECT_GE(lastCompletedCheck, base::Time::Now() - base::Seconds(1));
  EXPECT_LE(lastCompletedCheck, base::Time::Now() + base::Seconds(1));

  ResetNSUserDefaultsForTesting();
}

TEST_F(SafetyCheckMediatorTest, TimestampResetIfNoIssuesInCheck) {
  mediator_.checkDidRun = true;
  mediator_.passwordCheckRowState =
      PasswordCheckRowStateUnmutedCompromisedPasswords;
  [mediator_ resetsCheckStartItemIfNeeded];

  base::Time lastCompletedCheck = base::Time::FromSecondsSinceUnixEpoch(
      [[NSUserDefaults standardUserDefaults]
          doubleForKey:kTimestampOfLastIssueFoundKey]);
  EXPECT_GE(lastCompletedCheck, base::Time::Now() - base::Seconds(1));
  EXPECT_LE(lastCompletedCheck, base::Time::Now() + base::Seconds(1));

  mediator_.checkDidRun = true;
  mediator_.passwordCheckRowState = PasswordCheckRowStateSafe;
  [mediator_ resetsCheckStartItemIfNeeded];

  lastCompletedCheck = base::Time::FromSecondsSinceUnixEpoch(
      [[NSUserDefaults standardUserDefaults]
          doubleForKey:kTimestampOfLastIssueFoundKey]);
  EXPECT_EQ(base::Time(), lastCompletedCheck);

  ResetNSUserDefaultsForTesting();
}

// Checks the timestamp of the latest run is set after the Safety Check
// completes its run.
TEST_F(SafetyCheckMediatorTest, TimestampSetForLatestRun) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kSafetyCheckMagicStack}, {});

  mediator_.checkDidRun = true;

  mediator_.passwordCheckRowState =
      PasswordCheckRowStateUnmutedCompromisedPasswords;

  [mediator_ resetsCheckStartItemIfNeeded];

  base::Time lastRunTime =
      local_pref_service_->GetTime(prefs::kIosSettingsSafetyCheckLastRunTime);

  EXPECT_GE(lastRunTime, base::Time::Now() - base::Seconds(1));
  EXPECT_LE(lastRunTime, base::Time::Now() + base::Seconds(1));

  ResetLocalPrefsForTesting();
}

#pragma mark - Safe Browsing check tests

TEST_F(SafetyCheckMediatorTest, SafeBrowsingEnabledReturnsSafeState) {
  mediator_.safeBrowsingPreference.value = true;
  RunUntilIdle();
  [mediator_ checkAndReconfigureSafeBrowsingState];
  RunUntilIdle();
  EXPECT_EQ(mediator_.safeBrowsingCheckRowState, SafeBrowsingCheckRowStateSafe);
}

TEST_F(SafetyCheckMediatorTest, SafeBrowsingSafeUI) {
  mediator_.safeBrowsingCheckRowState = SafeBrowsingCheckRowStateSafe;
  [mediator_ reconfigureSafeBrowsingCheckItem];
  EXPECT_NSEQ(
      mediator_.safeBrowsingCheckItem.detailText,
      GetNSString(
          IDS_IOS_SETTINGS_SAFETY_CHECK_SAFE_BROWSING_ENHANCED_PROTECTION_ENABLED_DESC));

  // Change from Enhanced Protection to Standard Protection.
  mediator_.enhancedSafeBrowsingPreference.value = false;
  [mediator_ reconfigureSafeBrowsingCheckItem];
  EXPECT_NSEQ(
      mediator_.safeBrowsingCheckItem.detailText,
      GetNSString(
          IDS_IOS_SETTINGS_SAFETY_CHECK_SAFE_BROWSING_STANDARD_PROTECTION_ENABLED_DESC_WITH_ENHANCED_PROTECTION));

  EXPECT_EQ(mediator_.safeBrowsingCheckItem.trailingImage, SafeImage());
  EXPECT_TRUE([mediator_.safeBrowsingCheckItem.trailingImageTintColor
      isEqual:GreenColor()]);
}

// Tests UI for Safe Browsing row in Safety Check settings.
TEST_F(SafetyCheckMediatorTest,
       SafeBrowsingSafeUIStandardAndEnhancedProtection) {
  // Check UI when Safe Browsing protection choice is "Enhanced Protection".
  mediator_.safeBrowsingCheckRowState = SafeBrowsingCheckRowStateSafe;
  [mediator_ reconfigureSafeBrowsingCheckItem];
  EXPECT_NSEQ(
      mediator_.safeBrowsingCheckItem.detailText,
      GetNSString(
          IDS_IOS_SETTINGS_SAFETY_CHECK_SAFE_BROWSING_ENHANCED_PROTECTION_ENABLED_DESC));
  EXPECT_EQ(mediator_.safeBrowsingCheckItem.accessoryType,
            UITableViewCellAccessoryNone);
  EXPECT_EQ(mediator_.safeBrowsingCheckItem.trailingImage, SafeImage());
  EXPECT_TRUE([mediator_.safeBrowsingCheckItem.trailingImageTintColor
      isEqual:GreenColor()]);

  // Check UI when Safe Browsing protection choice is "Standard Protection".
  mediator_.enhancedSafeBrowsingPreference.value = false;
  [mediator_ reconfigureSafeBrowsingCheckItem];
  EXPECT_NSEQ(
      mediator_.safeBrowsingCheckItem.detailText,
      GetNSString(
          IDS_IOS_SETTINGS_SAFETY_CHECK_SAFE_BROWSING_STANDARD_PROTECTION_ENABLED_DESC_WITH_ENHANCED_PROTECTION));
  EXPECT_EQ(mediator_.safeBrowsingCheckItem.accessoryType,
            UITableViewCellAccessoryDisclosureIndicator);
  EXPECT_EQ(mediator_.safeBrowsingCheckItem.trailingImage, SafeImage());
  EXPECT_TRUE([mediator_.safeBrowsingCheckItem.trailingImageTintColor
      isEqual:GreenColor()]);

  // Check UI when Safe Browsing protection choice is "No Protection".
  mediator_.safeBrowsingPreference.value = false;
  mediator_.safeBrowsingCheckRowState = SafeBrowsingCheckRowStateUnsafe;
  [mediator_ reconfigureSafeBrowsingCheckItem];
  EXPECT_EQ(mediator_.safeBrowsingCheckItem.accessoryType,
            UITableViewCellAccessoryDisclosureIndicator);
  EXPECT_EQ(mediator_.safeBrowsingCheckItem.trailingImage, UnsafeImage());
  EXPECT_TRUE([mediator_.safeBrowsingCheckItem.trailingImageTintColor
      isEqual:RedColor()]);
}

TEST_F(SafetyCheckMediatorTest, SafeBrowsingDisabledReturnsInfoState) {
  mediator_.safeBrowsingPreference.value = false;
  RunUntilIdle();
  [mediator_ checkAndReconfigureSafeBrowsingState];
  EXPECT_EQ(mediator_.safeBrowsingCheckRowState,
            SafeBrowsingCheckRowStateUnsafe);
}

TEST_F(SafetyCheckMediatorTest, SafeBrowsingUnSafeUI) {
  mediator_.safeBrowsingCheckRowState = SafeBrowsingCheckRowStateUnsafe;
  [mediator_ reconfigureSafeBrowsingCheckItem];
  EXPECT_NSEQ(
      mediator_.safeBrowsingCheckItem.detailText,
      GetNSString(IDS_IOS_SETTINGS_SAFETY_CHECK_SAFE_BROWSING_DISABLED_DESC));
  EXPECT_TRUE(mediator_.safeBrowsingCheckItem.infoButtonHidden);
}

TEST_F(SafetyCheckMediatorTest, SafeBrowsingManagedUI) {
  mediator_.safeBrowsingCheckRowState = SafeBrowsingCheckRowStateManaged;
  [mediator_ reconfigureSafeBrowsingCheckItem];
  EXPECT_NSEQ(
      mediator_.safeBrowsingCheckItem.detailText,
      GetNSString(IDS_IOS_SETTINGS_SAFETY_CHECK_SAFE_BROWSING_MANAGED_DESC));
  EXPECT_FALSE(mediator_.safeBrowsingCheckItem.infoButtonHidden);
}

#pragma mark - Password check tests

// Tests that only having a safe password results in a safe password row state.
TEST_F(SafetyCheckMediatorTest, PasswordCheckSafeCheck) {
  AddSavedForm();
  mediator_.currentPasswordCheckState = PasswordCheckState::kRunning;
  [mediator_ passwordCheckStateDidChange:PasswordCheckState::kIdle];
  EXPECT_EQ(mediator_.passwordCheckRowState, PasswordCheckRowStateSafe);
}

// Tests that the content of the `passwordCheckItem` is as expected when in safe
// state.
TEST_F(SafetyCheckMediatorTest, PasswordCheckSafeUI) {
  mediator_.passwordCheckRowState = PasswordCheckRowStateSafe;
  [mediator_ reconfigurePasswordCheckItem];
  EXPECT_NSEQ(mediator_.passwordCheckItem.detailText,
              base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
                  IDS_IOS_PASSWORD_CHECKUP_COMPROMISED_COUNT, 0)));
  EXPECT_EQ(mediator_.passwordCheckItem.trailingImage, SafeImage());
  EXPECT_TRUE([mediator_.passwordCheckItem.trailingImageTintColor
      isEqual:GreenColor()]);
  EXPECT_EQ(mediator_.passwordCheckItem.accessoryType,
            UITableViewCellAccessoryDisclosureIndicator);
}

// Tests that only having a leaked password results in an umuted compromised
// password row state.
TEST_F(SafetyCheckMediatorTest, PasswordCheckUnmutedCompromisedPasswordsCheck) {
  CheckCompromisedRowState();
}

// Tests that the content of the `passwordCheckItem` is as expected when in
// compromised state.
TEST_F(SafetyCheckMediatorTest, PasswordCheckUnmutedCompromisedPasswordsUI) {
  AddSavedInsecureForm(InsecureType::kLeaked);
  mediator_.passwordCheckRowState =
      PasswordCheckRowStateUnmutedCompromisedPasswords;
  [mediator_ reconfigurePasswordCheckItem];
  EXPECT_NSEQ(mediator_.passwordCheckItem.detailText,
              base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
                  IDS_IOS_PASSWORD_CHECKUP_COMPROMISED_COUNT, 1)));
  EXPECT_EQ(mediator_.passwordCheckItem.trailingImage, UnsafeImage());
  EXPECT_TRUE(
      [mediator_.passwordCheckItem.trailingImageTintColor isEqual:RedColor()]);
  EXPECT_EQ(mediator_.passwordCheckItem.accessoryType,
            UITableViewCellAccessoryDisclosureIndicator);
}

// Tests that only having a reused password results in a reused password row
// state.
TEST_F(SafetyCheckMediatorTest, PasswordCheckReusedPasswordsCheck) {
  base::HistogramTester histogram_tester;

  AddSavedInsecureForm(InsecureType::kReused);
  AddSavedInsecureForm(InsecureType::kReused, /*is_muted=*/false,
                       /*signon_realm=*/"http://www.example1.com/");
  mediator_.currentPasswordCheckState = PasswordCheckState::kRunning;
  [mediator_ passwordCheckStateDidChange:PasswordCheckState::kIdle];

  histogram_tester.ExpectUniqueSample(
      kSafetyCheckMetricsPasswords,
      safety_check::PasswordsStatus::kReusedPasswordsExist, 1);

  EXPECT_EQ(mediator_.passwordCheckRowState,
            PasswordCheckRowStateReusedPasswords);
}

// Tests that the content of the `passwordCheckItem` is as expected when in
// reused state.
TEST_F(SafetyCheckMediatorTest, PasswordCheckReusedPasswordsUI) {
  AddSavedInsecureForm(InsecureType::kReused);
  AddSavedInsecureForm(InsecureType::kReused, /*is_muted=*/false,
                       /*signon_realm=*/"http://www.example1.com/");
  mediator_.passwordCheckRowState = PasswordCheckRowStateReusedPasswords;
  [mediator_ reconfigurePasswordCheckItem];
  EXPECT_NSEQ(
      mediator_.passwordCheckItem.detailText,
      l10n_util::GetNSStringF(IDS_IOS_PASSWORD_CHECKUP_REUSED_COUNT, u"2"));
  EXPECT_EQ(mediator_.passwordCheckItem.trailingImage, nil);
  EXPECT_EQ(mediator_.passwordCheckItem.accessoryType,
            UITableViewCellAccessoryDisclosureIndicator);
}

// Tests that only having a weak password results in a weak password row state.
TEST_F(SafetyCheckMediatorTest, PasswordCheckWeakPasswordsCheck) {
  base::HistogramTester histogram_tester;

  AddSavedInsecureForm(InsecureType::kWeak);
  mediator_.currentPasswordCheckState = PasswordCheckState::kRunning;
  [mediator_ passwordCheckStateDidChange:PasswordCheckState::kIdle];

  histogram_tester.ExpectUniqueSample(
      kSafetyCheckMetricsPasswords,
      safety_check::PasswordsStatus::kWeakPasswordsExist, 1);

  EXPECT_EQ(mediator_.passwordCheckRowState,
            PasswordCheckRowStateWeakPasswords);
}

// Tests that the content of the `passwordCheckItem` is as expected when in weak
// state.
TEST_F(SafetyCheckMediatorTest, PasswordCheckWeakPasswordsUI) {
  AddSavedInsecureForm(InsecureType::kWeak);
  mediator_.passwordCheckRowState = PasswordCheckRowStateWeakPasswords;
  [mediator_ reconfigurePasswordCheckItem];
  EXPECT_NSEQ(mediator_.passwordCheckItem.detailText,
              base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
                  IDS_IOS_PASSWORD_CHECKUP_WEAK_COUNT, 1)));
  EXPECT_EQ(mediator_.passwordCheckItem.trailingImage, nil);
  EXPECT_EQ(mediator_.passwordCheckItem.accessoryType,
            UITableViewCellAccessoryDisclosureIndicator);
}

// Tests that only having a dismissed compromsied warning results in a dismissed
// warning row state.
TEST_F(SafetyCheckMediatorTest, PasswordCheckDismissedWarningsCheck) {
  base::HistogramTester histogram_tester;

  AddSavedInsecureForm(InsecureType::kLeaked, /*is_muted=*/true);
  mediator_.currentPasswordCheckState = PasswordCheckState::kRunning;
  [mediator_ passwordCheckStateDidChange:PasswordCheckState::kIdle];

  histogram_tester.ExpectUniqueSample(
      kSafetyCheckMetricsPasswords,
      safety_check::PasswordsStatus::kMutedCompromisedExist, 1);

  EXPECT_EQ(mediator_.passwordCheckRowState,
            PasswordCheckRowStateDismissedWarnings);
}

// Tests that the content of the `passwordCheckItem` is as expected when in
// dismissed warning state.
TEST_F(SafetyCheckMediatorTest, PasswordCheckDismissedWarningsUI) {
  AddSavedInsecureForm(InsecureType::kLeaked, /*is_muted=*/true);
  mediator_.passwordCheckRowState = PasswordCheckRowStateDismissedWarnings;
  [mediator_ reconfigurePasswordCheckItem];
  EXPECT_NSEQ(mediator_.passwordCheckItem.detailText,
              base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
                  IDS_IOS_PASSWORD_CHECKUP_DISMISSED_COUNT, 1)));
  EXPECT_EQ(mediator_.passwordCheckItem.trailingImage, nil);
  EXPECT_EQ(mediator_.passwordCheckItem.accessoryType,
            UITableViewCellAccessoryDisclosureIndicator);
}

// Tests that exceeding the quota limit results in an error row state.
TEST_F(SafetyCheckMediatorTest, PasswordCheckErrorCheck) {
  AddSavedForm();
  mediator_.currentPasswordCheckState = PasswordCheckState::kRunning;
  [mediator_ passwordCheckStateDidChange:PasswordCheckState::kQuotaLimit];
  EXPECT_EQ(mediator_.passwordCheckRowState, PasswordCheckRowStateError);
}

// Tests that the content of the `passwordCheckItem` is as expected when in
// error state.
TEST_F(SafetyCheckMediatorTest, PasswordCheckErrorUI) {
  mediator_.passwordCheckRowState = PasswordCheckRowStateError;
  [mediator_ reconfigurePasswordCheckItem];
  EXPECT_NSEQ(mediator_.passwordCheckItem.detailText,
              GetNSString(IDS_IOS_PASSWORD_CHECK_ERROR));
  EXPECT_FALSE(mediator_.passwordCheckItem.infoButtonHidden);
  EXPECT_EQ(mediator_.passwordCheckItem.accessoryType,
            UITableViewCellAccessoryNone);
}

#pragma mark - Update check tests

TEST_F(SafetyCheckMediatorTest, OmahaRespondsUpToDate) {
  mediator_.updateCheckRowState = UpdateCheckRowStateRunning;
  UpgradeRecommendedDetails details;
  details.is_up_to_date = true;
  details.next_version = "9999.9999.9999.9999";
  details.upgrade_url = GURL("http://foobar.org");
  [mediator_ handleOmahaResponse:details];
  EXPECT_EQ(mediator_.updateCheckRowState, UpdateCheckRowStateUpToDate);
  ResetNSUserDefaultsForTesting();
}

TEST_F(SafetyCheckMediatorTest, UpdateCheckUpToDateUI) {
  mediator_.updateCheckRowState = UpdateCheckRowStateUpToDate;
  [mediator_ reconfigureUpdateCheckItem];
  EXPECT_NSEQ(
      mediator_.updateCheckItem.detailText,
      GetNSString(IDS_IOS_SETTINGS_SAFETY_CHECK_UPDATES_UP_TO_DATE_DESC));
  EXPECT_EQ(mediator_.updateCheckItem.trailingImage, SafeImage());
  EXPECT_TRUE(
      [mediator_.updateCheckItem.trailingImageTintColor isEqual:GreenColor()]);
}

TEST_F(SafetyCheckMediatorTest, OmahaRespondsOutOfDateAndUpdatesInfobarTime) {
  mediator_.updateCheckRowState = UpdateCheckRowStateRunning;
  UpgradeRecommendedDetails details;
  details.is_up_to_date = false;
  details.next_version = "9999.9999.9999.9999";
  details.upgrade_url = GURL("http://foobar.org");
  [mediator_ handleOmahaResponse:details];
  EXPECT_EQ(mediator_.updateCheckRowState, UpdateCheckRowStateOutOfDate);

  PrefService* pref_service = GetApplicationContext()->GetLocalState();
  const base::Time last_display =
      pref_service->GetTime(kLastInfobarDisplayTimeKey);
  EXPECT_GE((base::Time::Now() - last_display).InSeconds(), -1);
  EXPECT_LE((base::Time::Now() - last_display).InSeconds(), 1);
  ResetNSUserDefaultsForTesting();
  ResetLocalPrefsForTesting();
}

TEST_F(SafetyCheckMediatorTest, UpdateCheckOutOfDateUI) {
  mediator_.updateCheckRowState = UpdateCheckRowStateOutOfDate;
  [mediator_ reconfigureUpdateCheckItem];
  EXPECT_NSEQ(
      mediator_.updateCheckItem.detailText,
      GetNSString(IDS_IOS_SETTINGS_SAFETY_CHECK_UPDATES_OUT_OF_DATE_DESC));
  EXPECT_EQ(mediator_.updateCheckItem.trailingImage, UnsafeImage());
  EXPECT_TRUE(
      [mediator_.updateCheckItem.trailingImageTintColor isEqual:RedColor()]);
}

TEST_F(SafetyCheckMediatorTest, OmahaRespondsError) {
  mediator_.updateCheckRowState = UpdateCheckRowStateRunning;
  UpgradeRecommendedDetails details;
  details.is_up_to_date = false;
  details.next_version = "";
  details.upgrade_url = GURL("7");
  [mediator_ handleOmahaResponse:details];
  EXPECT_EQ(mediator_.updateCheckRowState, UpdateCheckRowStateOmahaError);
}

TEST_F(SafetyCheckMediatorTest, UpdateCheckOmahaErrorUI) {
  mediator_.updateCheckRowState = UpdateCheckRowStateOmahaError;
  [mediator_ reconfigureUpdateCheckItem];
  EXPECT_NSEQ(mediator_.updateCheckItem.detailText,
              GetNSString(IDS_IOS_SETTINGS_SAFETY_CHECK_UPDATES_ERROR_DESC));
  EXPECT_FALSE(mediator_.updateCheckItem.infoButtonHidden);
}

TEST_F(SafetyCheckMediatorTest, UpdateCheckNetErrorUI) {
  mediator_.updateCheckRowState = UpdateCheckRowStateNetError;
  [mediator_ reconfigureUpdateCheckItem];
  EXPECT_NSEQ(mediator_.updateCheckItem.detailText,
              GetNSString(IDS_IOS_SETTINGS_SAFETY_CHECK_UPDATES_OFFLINE_DESC));
  EXPECT_FALSE(mediator_.updateCheckItem.infoButtonHidden);
}

TEST_F(SafetyCheckMediatorTest, UpdateCheckManagedUI) {
  mediator_.updateCheckRowState = UpdateCheckRowStateManaged;
  [mediator_ reconfigureUpdateCheckItem];
  EXPECT_NSEQ(mediator_.updateCheckItem.detailText,
              GetNSString(IDS_IOS_SETTINGS_SAFETY_CHECK_UPDATES_MANAGED_DESC));
  EXPECT_FALSE(mediator_.updateCheckItem.infoButtonHidden);
}

TEST_F(SafetyCheckMediatorTest, UpdateCheckChannelUI) {
  mediator_.updateCheckRowState = UpdateCheckRowStateChannel;
  [mediator_ reconfigureUpdateCheckItem];
  EXPECT_NSEQ(
      mediator_.updateCheckItem.detailText,
      GetNSString(IDS_IOS_SETTINGS_SAFETY_CHECK_UPDATES_CHANNEL_CANARY_DESC));
  EXPECT_TRUE(mediator_.updateCheckItem.infoButtonHidden);
}

#pragma mark - Clickable tests

TEST_F(SafetyCheckMediatorTest, UpdateClickableOutOfDate) {
  mediator_.updateCheckRowState = UpdateCheckRowStateOutOfDate;
  [mediator_ reconfigureUpdateCheckItem];
  TableViewItem* updateItem =
      [[TableViewItem alloc] initWithType:SafetyCheckItemType::UpdateItemType];
  EXPECT_TRUE([mediator_ isItemClickable:updateItem]);
}

TEST_F(SafetyCheckMediatorTest, UpdateNonclickableUpToDate) {
  mediator_.updateCheckRowState = UpdateCheckRowStateUpToDate;
  [mediator_ reconfigureUpdateCheckItem];
  TableViewItem* updateItem =
      [[TableViewItem alloc] initWithType:SafetyCheckItemType::UpdateItemType];
  EXPECT_FALSE([mediator_ isItemClickable:updateItem]);
}

TEST_F(SafetyCheckMediatorTest, PasswordClickableUnmutedCompromisedPasswords) {
  mediator_.passwordCheckRowState =
      PasswordCheckRowStateUnmutedCompromisedPasswords;
  [mediator_ reconfigurePasswordCheckItem];
  TableViewItem* passwordItem = [[TableViewItem alloc]
      initWithType:SafetyCheckItemType::PasswordItemType];
  EXPECT_TRUE([mediator_ isItemClickable:passwordItem]);
}

// When in safe state, the password check item is clickable.
TEST_F(SafetyCheckMediatorTest, PasswordClickableSafe) {
  mediator_.passwordCheckRowState = PasswordCheckRowStateSafe;
  [mediator_ reconfigurePasswordCheckItem];
  TableViewItem* passwordItem = [[TableViewItem alloc]
      initWithType:SafetyCheckItemType::PasswordItemType];
  EXPECT_TRUE([mediator_ isItemClickable:passwordItem]);
}

// When in reused state, the password check item is clickable.
TEST_F(SafetyCheckMediatorTest, PasswordClickableReusedPasswords) {
  mediator_.passwordCheckRowState = PasswordCheckRowStateReusedPasswords;
  [mediator_ reconfigurePasswordCheckItem];
  TableViewItem* passwordItem = [[TableViewItem alloc]
      initWithType:SafetyCheckItemType::PasswordItemType];
  EXPECT_TRUE([mediator_ isItemClickable:passwordItem]);
}

// When in weak state, the password check item is clickable.
TEST_F(SafetyCheckMediatorTest, PasswordClickableWeakPasswords) {
  mediator_.passwordCheckRowState = PasswordCheckRowStateWeakPasswords;
  [mediator_ reconfigurePasswordCheckItem];
  TableViewItem* passwordItem = [[TableViewItem alloc]
      initWithType:SafetyCheckItemType::PasswordItemType];
  EXPECT_TRUE([mediator_ isItemClickable:passwordItem]);
}

// When in dismissed warnings state, the password check item is clickable.
TEST_F(SafetyCheckMediatorTest, PasswordClickableDismissedWarnings) {
  mediator_.passwordCheckRowState = PasswordCheckRowStateDismissedWarnings;
  [mediator_ reconfigurePasswordCheckItem];
  TableViewItem* passwordItem = [[TableViewItem alloc]
      initWithType:SafetyCheckItemType::PasswordItemType];
  EXPECT_TRUE([mediator_ isItemClickable:passwordItem]);
}

TEST_F(SafetyCheckMediatorTest, SafeBrowsingNonClickableDefault) {
  mediator_.safeBrowsingCheckRowState = SafeBrowsingCheckRowStateDefault;
  [mediator_ reconfigureSafeBrowsingCheckItem];
  TableViewItem* safeBrowsingItem = [[TableViewItem alloc]
      initWithType:SafetyCheckItemType::SafeBrowsingItemType];
  EXPECT_FALSE([mediator_ isItemClickable:safeBrowsingItem]);
}

TEST_F(SafetyCheckMediatorTest, CheckNowClickableAll) {
  mediator_.checkStartState = CheckStartStateCancel;
  [mediator_ reconfigureCheckStartSection];
  TableViewItem* checkStartItem = [[TableViewItem alloc]
      initWithType:SafetyCheckItemType::CheckStartItemType];
  EXPECT_TRUE([mediator_ isItemClickable:checkStartItem]);

  mediator_.checkStartState = CheckStartStateDefault;
  [mediator_ reconfigureCheckStartSection];
  EXPECT_TRUE([mediator_ isItemClickable:checkStartItem]);
}

// Tests that the notifications opt-in button correctly displays the "Turn Off"
// notifications prompt when notifications are currently enabled.
TEST_F(SafetyCheckMediatorTest, NotificationsOptInButtonPromptsTurnOff) {
  feature_list_.InitWithFeatures({kSafetyCheckNotifications}, {});

  mediator_ = [[SafetyCheckMediator alloc]
      initWithUserPrefService:pref_service_
             localPrefService:local_pref_service_
         passwordCheckManager:password_check_
                  authService:auth_service_
                  syncService:syncService()
                     referrer:password_manager::PasswordCheckReferrer::
                                  kSafetyCheck];

  [mediator_ reconfigureNotificationsSection:YES];

  EXPECT_NSEQ(
      mediator_.notificationsOptInItem.text,
      GetNSString(
          IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_TURN_OFF_NOTIFICATIONS_ELLIPSIS));
}

// Tests that the notifications opt-in button correctly displays the "Turn On"
// notifications prompt when notifications are currently disabled.
TEST_F(SafetyCheckMediatorTest, NotificationsOptInButtonPromptsTurnOn) {
  feature_list_.InitWithFeatures({kSafetyCheckNotifications}, {});

  mediator_ = [[SafetyCheckMediator alloc]
      initWithUserPrefService:pref_service_
             localPrefService:local_pref_service_
         passwordCheckManager:password_check_
                  authService:auth_service_
                  syncService:syncService()
                     referrer:password_manager::PasswordCheckReferrer::
                                  kSafetyCheck];

  [mediator_ reconfigureNotificationsSection:NO];

  EXPECT_NSEQ(
      mediator_.notificationsOptInItem.text,
      GetNSString(
          IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_TURN_ON_NOTIFICATIONS_ELLIPSIS));
}
