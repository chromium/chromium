// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/password/password_checkup/password_checkup_view_controller.h"

#import "base/memory/raw_ptr.h"
#import "base/strings/string_number_conversions.h"
#import "base/test/bind.h"
#import "base/test/scoped_feature_list.h"
#import "components/affiliations/core/browser/fake_affiliation_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/password_store/test_password_store.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "ios/chrome/browser/affiliations/model/ios_chrome_affiliation_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/password_checkup_utils.h"
#import "ios/chrome/browser/settings/ui_bundled/cells/settings_check_item.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_checkup/password_checkup_commands.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_checkup/password_checkup_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_checkup/password_checkup_mediator+Testing.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_checkup/password_checkup_mediator.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"

using password_manager::CredentialUIEntry;
using password_manager::InsecureType;
using password_manager::PasswordForm;
using password_manager::TestPasswordStore;
using password_manager::WarningType;

// Test fixture for testing PasswordCheckupViewController class.
class PasswordCheckupViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  PasswordCheckupViewControllerTest() = default;

  void SetUp() override {
    LegacyChromeTableViewControllerTest::SetUp();
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        IOSChromeProfilePasswordStoreFactory::GetInstance(),
        base::BindOnce(
            &password_manager::BuildPasswordStore<ProfileIOS,
                                                  TestPasswordStore>));
    builder.AddTestingFactory(
        IOSChromeAffiliationServiceFactory::GetInstance(),
        base::BindOnce([](ProfileIOS*) -> std::unique_ptr<KeyedService> {
          return std::make_unique<affiliations::FakeAffiliationService>();
        }));
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    CreateController();

    // Embed the controller in a navigation controller to test navigation.
    __unused UINavigationController* navigationController =
        [[UINavigationController alloc]
            initWithRootViewController:GetPasswordCheckupViewController()];

    PasswordCheckupViewController* view_controller =
        GetPasswordCheckupViewController();

    mediator_ = [[PasswordCheckupMediator alloc]
        initWithPasswordCheckManager:IOSChromePasswordCheckManagerFactory::
                                         GetForProfile(profile_.get())];
    view_controller.delegate = mediator_;
    mediator_.consumer = view_controller;

    handler_ = OCMStrictProtocolMock(@protocol(PasswordCheckupCommands));
    // Stub dismissAfterAllPasswordsGone by default to tolerate async state
    // transitions that can happen during test setup/execution.
    OCMStub([handler_ dismissAfterAllPasswordsGone]);
    view_controller.handler = handler_;

    // Add a saved password since Password Checkup is not available when the
    // user doesn't have any saved passwords.
    AddSavedForm();
  }

  TestPasswordStore& GetTestStore() {
    return *static_cast<TestPasswordStore*>(
        IOSChromeProfilePasswordStoreFactory::GetForProfile(
            browser_->GetProfile(), ServiceAccessType::EXPLICIT_ACCESS)
            .get());
  }

  LegacyChromeTableViewController* InstantiateController() override {
    return [[PasswordCheckupViewController alloc]
        initWithStyle:ChromeTableViewStyle()];
  }

  PasswordCheckupViewController* GetPasswordCheckupViewController() {
    return static_cast<PasswordCheckupViewController*>(controller());
  }

  // Changes the PasswordCheckupHomepageState.
  void ChangePasswordCheckupHomepageState(PasswordCheckupHomepageState state) {
    PasswordCheckupViewController* view_controller =
        GetPasswordCheckupViewController();

    password_manager::InsecurePasswordCounts counts = {};
    for (const auto& signon_realm_forms : GetAllLoginsSync(&GetTestStore())) {
      for (const PasswordForm& form : signon_realm_forms.second) {
        CredentialUIEntry credential = CredentialUIEntry(form);
        if (credential.IsMuted()) {
          counts.dismissed_count++;
        } else if (IsCompromised(credential)) {
          counts.compromised_count++;
        }
        if (credential.IsReused()) {
          counts.reused_count++;
        }
        if (credential.IsWeak()) {
          counts.weak_count++;
        }
      }
    }

    [view_controller setPasswordCheckupHomepageState:state
                              insecurePasswordCounts:counts
                  formattedElapsedTimeSinceLastCheck:
                      [mediator_ formattedElapsedTimeSinceLastCheck]];
  }

  // Adds a credential to the test password store.
  void AddStoredCredential(password_manager::StoredCredential cred) {
    GetTestStore().AddLogin(std::move(cred));
    RunUntilIdle();
  }

  // Creates and adds a saved password form.
  void AddSavedForm(std::string url = "http://www.example1.com/") {
    password_manager::StoredCredential cred;
    cred.url = GURL(url);
    cred.username_element = u"Email";
    cred.username_value = u"test@egmail.com";
    cred.password_element = u"Passwd";
    cred.password_value = password_manager::PasswordString(u"test");
    cred.signon_realm = url;
    cred.scheme = password_manager::PasswordForm::Scheme::kHtml;
    cred.in_store = password_manager::PasswordForm::Store::kProfileStore;
    AddStoredCredential(std::move(cred));
  }

  // Creates and adds a saved insecure password form.
  void AddSavedInsecureForm(InsecureType insecure_type,
                            bool is_muted = false,
                            std::string url = "http://www.example2.com/") {
    password_manager::StoredCredential cred;
    cred.url = GURL(url);
    cred.username_element = u"Email";
    cred.username_value = u"test@egmail.com";
    cred.password_element = u"Passwd";
    cred.password_value = password_manager::PasswordString(u"test");
    cred.signon_realm = url;
    cred.scheme = password_manager::PasswordForm::Scheme::kHtml;
    cred.in_store = password_manager::PasswordForm::Store::kProfileStore;
    cred.password_issues = {
        {insecure_type,
         password_manager::InsecurityMetadata(
             base::Time::Now(), password_manager::IsMuted(is_muted),
             password_manager::TriggerBackendNotification(false))}};
    AddStoredCredential(std::move(cred));
  }

  // Checks if the header image of the table view is as expected.
  void CheckHeaderImage(NSString* image_name) {
    UIImageView* headerImageView =
        (UIImageView*)GetPasswordCheckupViewController()
            .tableView.tableHeaderView;
    EXPECT_NSEQ([UIImage imageNamed:image_name], headerImageView.image);
  }

  // Checks if the item at the given index of the insecure types section is as
  // expected.
  void CheckItemFromInsecureTypesSection(
      int index,
      NSString* text,
      NSString* detail_text,
      bool indicator_hidden,
      bool trailing_icon_hidden,
      NSString* trailing_icon_name,
      NSString* trailing_icon_color_name,
      UITableViewCellAccessoryType accessory_type) {
    SettingsCheckItem* cell =
        static_cast<SettingsCheckItem*>(GetTableViewItem(0, index));
    EXPECT_NSEQ(text, cell.text);
    EXPECT_NSEQ(detail_text, cell.detailText);
    EXPECT_TRUE(cell.enabled);
    EXPECT_TRUE(cell.indicatorHidden == indicator_hidden);
    EXPECT_TRUE(cell.infoButtonHidden);
    if (trailing_icon_hidden) {
      EXPECT_TRUE(nil == cell.trailingImage);
    } else {
      EXPECT_NSEQ(DefaultSymbolTemplateWithPointSize(trailing_icon_name, 22),
                  cell.trailingImage);
      EXPECT_TRUE([cell.trailingImageTintColor
          isEqual:[UIColor colorNamed:trailing_icon_color_name]]);
    }
    EXPECT_TRUE(accessory_type == cell.accessoryType);
  }

  // Checks if the timestamp item is as expected.
  void CheckPasswordCheckupTimestampItem(NSString* text,
                                         int detail_text_id,
                                         int affiliated_group_count,
                                         bool indicator_hidden) {
    SettingsCheckItem* cell =
        static_cast<SettingsCheckItem*>(GetTableViewItem(1, 0));
    EXPECT_NSEQ(text, cell.text);
    EXPECT_NSEQ(
        l10n_util::GetPluralNSStringF(detail_text_id, affiliated_group_count),
        cell.detailText);
    EXPECT_TRUE(cell.enabled);
    EXPECT_TRUE(cell.indicatorHidden == indicator_hidden);
    EXPECT_TRUE(cell.infoButtonHidden);
  }

  // Checks if the check passwords button item is as expected.
  void CheckCheckPasswordsButtonItem(NSString* text_color_name,
                                     bool is_enabled) {
    TableViewTextItem* cell =
        static_cast<TableViewTextItem*>(GetTableViewItem(1, 1));
    EXPECT_NSEQ(l10n_util::GetNSString(
                    IDS_IOS_PASSWORD_CHECKUP_HOMEPAGE_CHECK_AGAIN_BUTTON),
                cell.text);
    EXPECT_TRUE([cell.textColor isEqual:[UIColor colorNamed:text_color_name]]);
    EXPECT_TRUE(cell.enabled == is_enabled);
  }

  // Initializes the strings for the items in the insecure types section as if
  // all saved passwords were safe.
  void InitializeStringsForInsecureTypeSection() {
    compromised_text_ = l10n_util::GetPluralNSStringF(
        IDS_IOS_PASSWORD_CHECKUP_HOMEPAGE_COMPROMISED_PASSWORDS_TITLE, 0);
    compromised_detail_text_ = l10n_util::GetNSString(
        IDS_IOS_PASSWORD_CHECKUP_HOMEPAGE_NO_COMPROMISED_PASSWORDS_SUBTITLE);
    reused_text_ = l10n_util::GetNSString(
        IDS_IOS_PASSWORD_CHECKUP_HOMEPAGE_NO_REUSED_PASSWORDS_TITLE);
    reused_detail_text_ = l10n_util::GetNSString(
        IDS_IOS_PASSWORD_CHECKUP_HOMEPAGE_NO_REUSED_PASSWORDS_SUBTITLE);
    weak_text_ = l10n_util::GetNSString(
        IDS_IOS_PASSWORD_CHECKUP_HOMEPAGE_NO_WEAK_PASSWORDS_TITLE);
    weak_detail_text_ = l10n_util::GetNSString(
        IDS_IOS_PASSWORD_CHECKUP_HOMEPAGE_NO_WEAK_PASSWORDS_SUBTITLE);
  }

  // Simulates a tap on an item in the tableView.
  void SimulateTap(int index, int section) {
    [controller() tableView:controller().tableView
        didSelectRowAtIndexPath:[NSIndexPath indexPathForItem:index
                                                    inSection:section]];
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  PasswordCheckupMediator* mediator_;
  base::test::ScopedFeatureList feature_list;
  id<PasswordCheckupCommands> handler_;
  // Strings for the insecure types section.
  NSString* compromised_text_;
  NSString* compromised_detail_text_;
  NSString* reused_text_;
  NSString* reused_detail_text_;
  NSString* weak_text_;
  NSString* weak_detail_text_;
};

// Tests the running state of the Password Checkup homepage.
TEST_F(PasswordCheckupViewControllerTest, PasswordCheckupHomepageStateRunning) {
  ChangePasswordCheckupHomepageState(PasswordCheckupHomepageStateRunning);

  CheckHeaderImage(password_manager::kPasswordCheckupHeaderImageLoading);

  InitializeStringsForInsecureTypeSection();
  CheckItemFromInsecureTypesSection(
      /*index=*/0, /*text=*/compromised_text_,
      /*detail_text=*/compromised_detail_text_,
      /*indicator_hidden=*/NO,
      /*trailing_icon_hidden=*/YES,
      /*trailing_icon_name=*/@"",
      /*trailing_icon_color_name=*/@"",
      /*accessory_type=*/UITableViewCellAccessoryNone);
  CheckItemFromInsecureTypesSection(
      /*index=*/1, /*text=*/reused_text_,
      /*detail_text=*/reused_detail_text_,
      /*indicator_hidden=*/NO,
      /*trailing_icon_hidden=*/YES,
      /*trailing_icon_name=*/@"",
      /*trailing_icon_color_name=*/@"",
      /*accessory_type=*/UITableViewCellAccessoryNone);
  CheckItemFromInsecureTypesSection(
      /*index=*/2, /*text=*/weak_text_,
      /*detail_text=*/weak_detail_text_,
      /*indicator_hidden=*/NO,
      /*trailing_icon_hidden=*/YES,
      /*trailing_icon_name=*/@"",
      /*trailing_icon_color_name=*/@"",
      /*accessory_type=*/UITableViewCellAccessoryNone);

  CheckPasswordCheckupTimestampItem(
      /*text=*/l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECKUP_ONGOING),
      /*detail_text_id=*/
      IDS_IOS_PASSWORD_CHECKUP_SITES_AND_APPS_COUNT,
      /*affiliated_group_count=*/1, /*indicator_hidden=*/false);
  CheckCheckPasswordsButtonItem(/*text_color_name=*/kTextSecondaryColor,
                                /*is_enabled=*/false);

  [GetPasswordCheckupViewController() settingsWillBeDismissed];
}

// Tests the "done" state of the Password Checkup homepage with no insecure
// passwords.
TEST_F(PasswordCheckupViewControllerTest, PasswordCheckupHomepageStateSafe) {
  ChangePasswordCheckupHomepageState(PasswordCheckupHomepageStateDone);

  CheckHeaderImage(password_manager::kPasswordCheckupHeaderImageGreen);

  InitializeStringsForInsecureTypeSection();
  CheckItemFromInsecureTypesSection(
      /*index=*/0, /*text=*/compromised_text_,
      /*detail_text=*/compromised_detail_text_,
      /*indicator_hidden=*/YES,
      /*trailing_icon_hidden=*/NO,
      /*trailing_icon_name=*/kCheckmarkCircleFillSymbol,
      /*trailing_icon_color_name=*/kGreen500Color,
      /*accessory_type=*/UITableViewCellAccessoryNone);
  CheckItemFromInsecureTypesSection(
      /*index=*/1, /*text=*/reused_text_,
      /*detail_text=*/reused_detail_text_,
      /*indicator_hidden=*/YES,
      /*trailing_icon_hidden=*/NO,
      /*trailing_icon_name=*/kCheckmarkCircleFillSymbol,
      /*trailing_icon_color_name=*/kGreen500Color,
      /*accessory_type=*/UITableViewCellAccessoryNone);
  CheckItemFromInsecureTypesSection(
      /*index=*/2, /*text=*/weak_text_,
      /*detail_text=*/weak_detail_text_,
      /*indicator_hidden=*/YES,
      /*trailing_icon_hidden=*/NO,
      /*trailing_icon_name=*/kCheckmarkCircleFillSymbol,
      /*trailing_icon_color_name=*/kGreen500Color,
      /*accessory_type=*/UITableViewCellAccessoryNone);

  CheckPasswordCheckupTimestampItem(
      /*text=*/[mediator_ formattedElapsedTimeSinceLastCheck],
      /*detail_text_id=*/IDS_IOS_PASSWORD_CHECKUP_SITES_AND_APPS_COUNT,
      /*affiliated_group_count=*/1, /*indicator_hidden=*/true);
  CheckCheckPasswordsButtonItem(/*text_color_name=*/kBlueColor,
                                /*is_enabled=*/true);

  [GetPasswordCheckupViewController() settingsWillBeDismissed];
}

// Tests the "done" state of the Password Checkup homepage with compromised
// passwords.
TEST_F(PasswordCheckupViewControllerTest,
       PasswordCheckupHomepageStateWithCompromisedPasswords) {
  AddSavedInsecureForm(InsecureType::kLeaked);
  ChangePasswordCheckupHomepageState(PasswordCheckupHomepageStateDone);

  CheckHeaderImage(password_manager::kPasswordCheckupHeaderImageRed);

  InitializeStringsForInsecureTypeSection();
  compromised_text_ = l10n_util::GetPluralNSStringF(
      IDS_IOS_PASSWORD_CHECKUP_HOMEPAGE_COMPROMISED_PASSWORDS_TITLE, 1);
  compromised_detail_text_ = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_CHECKUP_HOMEPAGE_COMPROMISED_PASSWORDS_SUBTITLE);
  CheckItemFromInsecureTypesSection(
      /*index=*/0, /*text=*/compromised_text_,
      /*detail_text=*/compromised_detail_text_,
      /*indicator_hidden=*/YES,
      /*trailing_icon_hidden=*/NO,
      /*trailing_icon_name=*/kErrorCircleFillSymbol,
      /*trailing_icon_color_name=*/kRed500Color,
      /*accessory_type=*/UITableViewCellAccessoryDisclosureIndicator);
  CheckItemFromInsecureTypesSection(
      /*index=*/1, /*text=*/reused_text_,
      /*detail_text=*/reused_detail_text_,
      /*indicator_hidden=*/YES,
      /*trailing_icon_hidden=*/NO,
      /*trailing_icon_name=*/kCheckmarkCircleFillSymbol,
      /*trailing_icon_color_name=*/kGreen500Color,
      /*accessory_type=*/UITableViewCellAccessoryNone);
  CheckItemFromInsecureTypesSection(
      /*index=*/2, /*text=*/weak_text_,
      /*detail_text=*/weak_detail_text_,
      /*indicator_hidden=*/YES,
      /*trailing_icon_hidden=*/NO,
      /*trailing_icon_name=*/kCheckmarkCircleFillSymbol,
      /*trailing_icon_color_name=*/kGreen500Color,
      /*accessory_type=*/UITableViewCellAccessoryNone);

  CheckPasswordCheckupTimestampItem(
      /*text=*/[mediator_ formattedElapsedTimeSinceLastCheck],
      /*detail_text_id=*/IDS_IOS_PASSWORD_CHECKUP_SITES_AND_APPS_COUNT,
      /*affiliated_group_count=*/2, /*indicator_hidden=*/true);
  CheckCheckPasswordsButtonItem(/*text_color_name=*/kBlueColor,
                                /*is_enabled=*/true);

  [GetPasswordCheckupViewController() settingsWillBeDismissed];
}

// Tests the "done" state of the Password Checkup homepage with muted
// compromised passwords.
TEST_F(PasswordCheckupViewControllerTest,
       PasswordCheckupHomepageStateWithMutedCompromisedPasswords) {
  AddSavedInsecureForm(InsecureType::kLeaked, /*is_muted=*/true);
  ChangePasswordCheckupHomepageState(PasswordCheckupHomepageStateDone);

  CheckHeaderImage(password_manager::kPasswordCheckupHeaderImageYellow);

  InitializeStringsForInsecureTypeSection();
  compromised_detail_text_ = l10n_util::GetPluralNSStringF(
      IDS_IOS_PASSWORD_CHECKUP_HOMEPAGE_DISMISSED_WARNINGS_SUBTITLE, 1);
  CheckItemFromInsecureTypesSection(
      /*index=*/0, /*text=*/compromised_text_,
      /*detail_text=*/compromised_detail_text_,
      /*indicator_hidden=*/YES,
      /*trailing_icon_hidden=*/NO,
      /*trailing_icon_name=*/kErrorCircleFillSymbol,
      /*trailing_icon_color_name=*/kYellow500Color,
      /*accessory_type=*/UITableViewCellAccessoryDisclosureIndicator);
  CheckItemFromInsecureTypesSection(
      /*index=*/1, /*text=*/reused_text_,
      /*detail_text=*/reused_detail_text_,
      /*indicator_hidden=*/YES,
      /*trailing_icon_hidden=*/NO,
      /*trailing_icon_name=*/kCheckmarkCircleFillSymbol,
      /*trailing_icon_color_name=*/kGreen500Color,
      /*accessory_type=*/UITableViewCellAccessoryNone);
  CheckItemFromInsecureTypesSection(
      /*index=*/2, /*text=*/weak_text_,
      /*detail_text=*/weak_detail_text_,
      /*indicator_hidden=*/YES,
      /*trailing_icon_hidden=*/NO,
      /*trailing_icon_name=*/kCheckmarkCircleFillSymbol,
      /*trailing_icon_color_name=*/kGreen500Color,
      /*accessory_type=*/UITableViewCellAccessoryNone);

  CheckPasswordCheckupTimestampItem(
      /*text=*/[mediator_ formattedElapsedTimeSinceLastCheck],
      /*detail_text_id=*/IDS_IOS_PASSWORD_CHECKUP_SITES_AND_APPS_COUNT,
      /*affiliated_group_count=*/2, /*indicator_hidden=*/true);
  CheckCheckPasswordsButtonItem(/*text_color_name=*/kBlueColor,
                                /*is_enabled=*/true);

  [GetPasswordCheckupViewController() settingsWillBeDismissed];
}

// Tests the "done" state of the Password Checkup homepage with reused
// passwords.
TEST_F(PasswordCheckupViewControllerTest,
       PasswordCheckupHomepageStateWithReusedPasswords) {
  AddSavedInsecureForm(InsecureType::kReused);
  ChangePasswordCheckupHomepageState(PasswordCheckupHomepageStateDone);

  CheckHeaderImage(password_manager::kPasswordCheckupHeaderImageYellow);

  InitializeStringsForInsecureTypeSection();
  reused_text_ = l10n_util::GetNSStringF(
      IDS_IOS_PASSWORD_CHECKUP_HOMEPAGE_REUSED_PASSWORDS_TITLE,
      base::NumberToString16(1));
  reused_detail_text_ = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_CHECKUP_HOMEPAGE_REUSED_PASSWORDS_SUBTITLE);
  CheckItemFromInsecureTypesSection(
      /*index=*/0, /*text=*/compromised_text_,
      /*detail_text=*/compromised_detail_text_,
      /*indicator_hidden=*/YES,
      /*trailing_icon_hidden=*/NO,
      /*trailing_icon_name=*/kCheckmarkCircleFillSymbol,
      /*trailing_icon_color_name=*/kGreen500Color,
      /*accessory_type=*/UITableViewCellAccessoryNone);
  CheckItemFromInsecureTypesSection(
      /*index=*/1, /*text=*/reused_text_,
      /*detail_text=*/reused_detail_text_,
      /*indicator_hidden=*/YES,
      /*trailing_icon_hidden=*/NO,
      /*trailing_icon_name=*/kErrorCircleFillSymbol,
      /*trailing_icon_color_name=*/kYellow500Color,
      /*accessory_type=*/UITableViewCellAccessoryDisclosureIndicator);
  CheckItemFromInsecureTypesSection(
      /*index=*/2, /*text=*/weak_text_,
      /*detail_text=*/weak_detail_text_,
      /*indicator_hidden=*/YES,
      /*trailing_icon_hidden=*/NO,
      /*trailing_icon_name=*/kCheckmarkCircleFillSymbol,
      /*trailing_icon_color_name=*/kGreen500Color,
      /*accessory_type=*/UITableViewCellAccessoryNone);

  CheckPasswordCheckupTimestampItem(
      /*text=*/[mediator_ formattedElapsedTimeSinceLastCheck],
      /*text_id=*/IDS_IOS_PASSWORD_CHECKUP_SITES_AND_APPS_COUNT,
      /*affiliated_group_count=*/2, /*indicator_hidden=*/true);
  CheckCheckPasswordsButtonItem(/*text_color_name=*/kBlueColor,
                                /*is_enabled=*/true);

  [GetPasswordCheckupViewController() settingsWillBeDismissed];
}

// Tests the "done" state of the Password Checkup homepage with weak passwords.
TEST_F(PasswordCheckupViewControllerTest,
       PasswordCheckupHomepageStateWithWeakPasswords) {
  AddSavedInsecureForm(InsecureType::kWeak);
  ChangePasswordCheckupHomepageState(PasswordCheckupHomepageStateDone);

  CheckHeaderImage(password_manager::kPasswordCheckupHeaderImageYellow);

  InitializeStringsForInsecureTypeSection();
  weak_text_ = l10n_util::GetPluralNSStringF(
      IDS_IOS_PASSWORD_CHECKUP_HOMEPAGE_WEAK_PASSWORDS_TITLE, 1);
  weak_detail_text_ = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_CHECKUP_HOMEPAGE_WEAK_PASSWORDS_SUBTITLE);
  CheckItemFromInsecureTypesSection(
      /*index=*/0, /*text=*/compromised_text_,
      /*detail_text=*/compromised_detail_text_,
      /*indicator_hidden=*/YES,
      /*trailing_icon_hidden=*/NO,
      /*trailing_icon_name=*/kCheckmarkCircleFillSymbol,
      /*trailing_icon_color_name=*/kGreen500Color,
      /*accessory_type=*/UITableViewCellAccessoryNone);
  CheckItemFromInsecureTypesSection(
      /*index=*/1, /*text=*/reused_text_,
      /*detail_text=*/reused_detail_text_,
      /*indicator_hidden=*/YES,
      /*trailing_icon_hidden=*/NO,
      /*trailing_icon_name=*/kCheckmarkCircleFillSymbol,
      /*trailing_icon_color_name=*/kGreen500Color,
      /*accessory_type=*/UITableViewCellAccessoryNone);
  CheckItemFromInsecureTypesSection(
      /*index=*/2, /*text=*/weak_text_,
      /*detail_text=*/weak_detail_text_,
      /*indicator_hidden=*/YES,
      /*trailing_icon_hidden=*/NO,
      /*trailing_icon_name=*/kErrorCircleFillSymbol,
      /*trailing_icon_color_name=*/kYellow500Color,
      /*accessory_type=*/UITableViewCellAccessoryDisclosureIndicator);

  CheckPasswordCheckupTimestampItem(
      /*text=*/[mediator_ formattedElapsedTimeSinceLastCheck],
      /*detail_text_id=*/IDS_IOS_PASSWORD_CHECKUP_SITES_AND_APPS_COUNT,
      /*affiliated_group_count=*/2, /*indicator_hidden=*/true);
  CheckCheckPasswordsButtonItem(/*text_color_name=*/kBlueColor,
                                /*is_enabled=*/true);

  [GetPasswordCheckupViewController() settingsWillBeDismissed];
}

// Verifies that tapping on the compromised passwords cell tells the handler to
// show compromised issues.
TEST_F(PasswordCheckupViewControllerTest,
       TestTapCompromisedPasswordsNotifiesHandler) {
  AddSavedInsecureForm(InsecureType::kLeaked);
  ChangePasswordCheckupHomepageState(PasswordCheckupHomepageStateDone);

  OCMExpect([handler_ showPasswordIssuesWithWarningType:
                          WarningType::kCompromisedPasswordsWarning]);

  SimulateTap(/*index=*/0, /*section=*/0);

  EXPECT_OCMOCK_VERIFY((id)handler_);
}

// Verifies that tapping on the reused passwords cell tells the handler to show
// reused issues.
TEST_F(PasswordCheckupViewControllerTest,
       TestTapReusedPasswordsNotifiesHandler) {
  AddSavedInsecureForm(InsecureType::kReused);
  ChangePasswordCheckupHomepageState(PasswordCheckupHomepageStateDone);

  OCMExpect([handler_
      showPasswordIssuesWithWarningType:WarningType::kReusedPasswordsWarning]);

  SimulateTap(/*index=*/1, /*section=*/0);

  EXPECT_OCMOCK_VERIFY((id)handler_);
}

// Verifies that tapping on the weak passwords cell tells the handler to
// show weak issues.
TEST_F(PasswordCheckupViewControllerTest, TestTapWeakPasswordsNotifiesHandler) {
  AddSavedInsecureForm(InsecureType::kWeak);
  ChangePasswordCheckupHomepageState(PasswordCheckupHomepageStateDone);

  OCMExpect([handler_
      showPasswordIssuesWithWarningType:WarningType::kWeakPasswordsWarning]);

  SimulateTap(/*index=*/2, /*section=*/0);

  EXPECT_OCMOCK_VERIFY((id)handler_);
}

// Verifies that deleting all saved passwords through Password Checkup triggers
// a dismissal in the handler.
TEST_F(PasswordCheckupViewControllerTest, TestDismissAfterPasswordsGone) {
  // Re-create a fresh strict mock without the default stub to verify dismissal.
  handler_ = OCMStrictProtocolMock(@protocol(PasswordCheckupCommands));
  GetPasswordCheckupViewController().handler = handler_;

  OCMExpect([handler_ dismissAfterAllPasswordsGone]);

  ChangePasswordCheckupHomepageState(PasswordCheckupHomepageStateDisabled);

  EXPECT_OCMOCK_VERIFY((id)handler_);
}

// Verifies that a successful navigation blocks subsequent taps.
TEST_F(PasswordCheckupViewControllerTest, TestSuccessfulNavigationBlocksTaps) {
  AddSavedInsecureForm(InsecureType::kLeaked);
  ChangePasswordCheckupHomepageState(PasswordCheckupHomepageStateDone);

  PasswordCheckupViewController* pwd_checkup_vc =
      GetPasswordCheckupViewController();
  UIViewController* dummy_vc = [[UIViewController alloc] init];

  // Stub the handler to perform a successful push.
  OCMStub([handler_ showPasswordIssuesWithWarningType:
                        WarningType::kCompromisedPasswordsWarning])
      .andDo(^(NSInvocation* invocation) {
        [pwd_checkup_vc.navigationController pushViewController:dummy_vc
                                                       animated:NO];
      });

  // 1. First tap should succeed and trigger the push.
  SimulateTap(/*index=*/0, /*section=*/0);
  EXPECT_EQ(pwd_checkup_vc.navigationController.topViewController, dummy_vc);

  // 2. Second tap should be ignored because navigating is active.
  // We reject any calls to the handler.
  OCMReject([handler_ showPasswordIssuesWithWarningType:
                          WarningType::kCompromisedPasswordsWarning]);
  SimulateTap(/*index=*/0, /*section=*/0);
  EXPECT_OCMOCK_VERIFY((id)handler_);

  // Verify the first part (reject was not violated)
  EXPECT_OCMOCK_VERIFY((id)handler_);

  // Re-setup a new mock for the second part. Use a regular mock to tolerate
  // async noise (like dismissAfterAllPasswordsGone).
  id<PasswordCheckupCommands> newHandler =
      OCMProtocolMock(@protocol(PasswordCheckupCommands));
  pwd_checkup_vc.handler = newHandler;

  // 4. Simulate returning to the screen (pop the dummy and call
  // viewWillAppear).
  [pwd_checkup_vc.navigationController popViewControllerAnimated:NO];
  EXPECT_EQ(pwd_checkup_vc.navigationController.topViewController,
            pwd_checkup_vc);
  [pwd_checkup_vc viewWillAppear:NO];

  // 5. Third tap should now succeed again.
  OCMExpect([newHandler showPasswordIssuesWithWarningType:
                            WarningType::kCompromisedPasswordsWarning]);
  SimulateTap(/*index=*/0, /*section=*/0);
  EXPECT_OCMOCK_VERIFY((id)newHandler);
}

// Verifies that a failed navigation (no push) does not block subsequent taps.
TEST_F(PasswordCheckupViewControllerTest,
       TestFailedNavigationDoesNotBlockTaps) {
  AddSavedInsecureForm(InsecureType::kLeaked);
  ChangePasswordCheckupHomepageState(PasswordCheckupHomepageStateDone);

  PasswordCheckupViewController* pwd_checkup_vc =
      GetPasswordCheckupViewController();

  // Stub the handler to do nothing (failed navigation).
  OCMStub([handler_ showPasswordIssuesWithWarningType:
                        WarningType::kCompromisedPasswordsWarning]);

  // 1. First tap should succeed (calls handler, but no push).
  SimulateTap(/*index=*/0, /*section=*/0);
  EXPECT_EQ(pwd_checkup_vc.navigationController.topViewController,
            pwd_checkup_vc);

  // Re-setup a new mock for the second part to avoid stub/expect collision.
  // Use a regular mock to tolerate async noise (like
  // dismissAfterAllPasswordsGone).
  id<PasswordCheckupCommands> newHandler =
      OCMProtocolMock(@protocol(PasswordCheckupCommands));
  pwd_checkup_vc.handler = newHandler;

  // 2. Second tap should succeed again because navigating was never set.
  OCMExpect([newHandler showPasswordIssuesWithWarningType:
                            WarningType::kCompromisedPasswordsWarning]);
  SimulateTap(/*index=*/0, /*section=*/0);
  EXPECT_OCMOCK_VERIFY((id)newHandler);
}
