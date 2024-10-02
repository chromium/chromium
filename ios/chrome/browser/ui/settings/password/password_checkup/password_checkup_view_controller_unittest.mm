// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_view_controller.h"

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
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/cells/settings_check_item.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_commands.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_mediator+Testing.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_mediator.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
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
        base::BindRepeating(
            &password_manager::BuildPasswordStore<web::BrowserState,
                                                  TestPasswordStore>));
    builder.AddTestingFactory(
        IOSChromeAffiliationServiceFactory::GetInstance(),
        base::BindRepeating(base::BindLambdaForTesting([](web::BrowserState*) {
          return std::unique_ptr<KeyedService>(
              std::make_unique<affiliations::FakeAffiliationService>());
        })));
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    CreateController();

    PasswordCheckupViewController* view_controller =
        GetPasswordCheckupViewController();

    mediator_ = [[PasswordCheckupMediator alloc]
        initWithPasswordCheckManager:IOSChromePasswordCheckManagerFactory::
                                         GetForProfile(profile_.get())];
    view_controller.delegate = mediator_;
    mediator_.consumer = view_controller;

    handler_ = OCMStrictProtocolMock(@protocol(PasswordCheckupCommands));
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
    for (const auto& signon_realm_forms : GetTestStore().stored_passwords()) {
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

  // Adds a form to the test password store.
  void AddPasswordForm(std::unique_ptr<password_manager::PasswordForm> form) {
    GetTestStore().AddLogin(*form);
    RunUntilIdle();
  }

  // Creates and adds a saved password form.
  void AddSavedForm(std::string url = "http://www.example1.com/") {
    auto form = std::make_unique<password_manager::PasswordForm>();
    form->url = GURL(url);
    form->username_element = u"Email";
    form->username_value = u"test@egmail.com";
    form->password_element = u"Passwd";
    form->password_value = u"test";
    form->signon_realm = url;
    form->scheme = password_manager::PasswordForm::Scheme::kHtml;
    form->in_store = password_manager::PasswordForm::Store::kProfileStore;
    AddPasswordForm(std::move(form));
  }

  // Creates and adds a saved insecure password form.
  void AddSavedInsecureForm(InsecureType insecure_type,
                            bool is_muted = false,
                            std::string url = "http://www.example2.com/") {
    auto form = std::make_unique<password_manager::PasswordForm>();
    form->url = GURL(url);
    form->username_element = u"Email";
    form->username_value = u"test@egmail.com";
    form->password_element = u"Passwd";
    form->password_value = u"test";
    form->signon_realm = url;
    form->scheme = password_manager::PasswordForm::Scheme::kHtml;
    form->in_store = password_manager::PasswordForm::Store::kProfileStore;
    form->password_issues = {
        {insecure_type,
         password_manager::InsecurityMetadata(
             base::Time::Now(), password_manager::IsMuted(is_muted),
             password_manager::TriggerBackendNotification(false))}};
    AddPasswordForm(std::move(form));
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

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
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
  OCMExpect([handler_ dismissAfterAllPasswordsGone]);

  ChangePasswordCheckupHomepageState(PasswordCheckupHomepageStateDisabled);

  EXPECT_OCMOCK_VERIFY((id)handler_);
}
