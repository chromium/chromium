// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_view_controller.h"

#import "base/check_op.h"
#import "base/memory/raw_ptr.h"
#import "base/test/metrics/user_action_tester.h"
#import "ios/chrome/browser/policy/model/management_state.h"
#import "ios/chrome/browser/settings/model/sync/utils/account_error_ui_info.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_data_source.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_mutator.h"
#import "ios/chrome/browser/ui/authentication/cells/central_account_view.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_account_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_cell.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

const FakeSystemIdentity* kPrimaryIdentity = [FakeSystemIdentity fakeIdentity1];
const FakeSystemIdentity* kSecondaryIdentity =
    [FakeSystemIdentity fakeIdentity2];
const FakeSystemIdentity* kSecondaryIdentity2 =
    [FakeSystemIdentity fakeIdentity3];
UIImage* kPrimaryAccountAvatar = [[UIImage alloc] init];

}  // namespace

// An account menu data source with a primary and a secondary identities.
@interface FakeAccountMenuDataSource : NSObject <AccountMenuDataSource>
@property(nonatomic, assign) ChromeAccountManagerService* accountManagerService;
@property(nonatomic, strong) AccountErrorUIInfo* accountErrorUIInfo;
@end

@implementation FakeAccountMenuDataSource
@synthesize secondaryAccountsGaiaIDs = _secondaryAccountsGaiaIDs;
@synthesize primaryAccountEmail = _primaryAccountEmail;
@synthesize primaryAccountAvatar = _primaryAccountAvatar;
@synthesize primaryAccountUserFullName = _primaryAccountUserFullName;
@synthesize managementState = _managementState;

- (instancetype)init {
  self = [super init];
  if (self) {
    _accountErrorUIInfo = nil;
    _secondaryAccountsGaiaIDs = @[ kSecondaryIdentity.gaiaID ];
    _primaryAccountEmail = kPrimaryIdentity.userEmail;
    _primaryAccountAvatar = kPrimaryAccountAvatar;
    _primaryAccountUserFullName = kPrimaryIdentity.userFullName;
    _managementState.user_level_domain = "acme.com";
  }
  return self;
}

// The only acceptable argument is the ID of a secondary id.
- (const FakeSystemIdentity*)identityForGaiaID:(NSString*)gaiaID {
  if (gaiaID == kSecondaryIdentity.gaiaID) {
    return kSecondaryIdentity;
  } else if (gaiaID == kSecondaryIdentity2.gaiaID) {
    return kSecondaryIdentity2;
  } else {
    NOTREACHED();
  }
}

- (NSString*)nameForGaiaID:(NSString*)gaiaID {
  return [self identityForGaiaID:gaiaID].userFullName;
}

- (NSString*)emailForGaiaID:(NSString*)gaiaID {
  return [self identityForGaiaID:gaiaID].userEmail;
}

- (UIImage*)imageForGaiaID:(NSString*)gaiaID {
  return _accountManagerService->GetIdentityAvatarWithIdentity(
      [self identityForGaiaID:gaiaID], IdentityAvatarSize::TableViewIcon);
}

@end

class AccountMenuViewControllerTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();
    fake_system_identity_manager_ =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    AuthenticationServiceFactory::CreateAndInitializeForProfile(
        profile_.get(), std::make_unique<FakeAuthenticationServiceDelegate>());
    data_source_.accountManagerService =
        ChromeAccountManagerServiceFactory::GetForProfile(profile_.get());
    authentication_service_ =
        AuthenticationServiceFactory::GetForProfile(profile_.get());

    AddPrimaryIdentity();
    AddSecondaryIdentity();

    view_controller_ = [[AccountMenuViewController alloc]
        initWithStyle:UITableViewStyleInsetGrouped];
    mutator_ = OCMStrictProtocolMock(@protocol(AccountMenuMutator));

    view_controller_.dataSource = data_source_;
    view_controller_.mutator = mutator_;
    navigation_controller_ = [[UINavigationController alloc]
        initWithRootViewController:view_controller_];
    [view_controller_ viewDidLoad];
  }

  void TearDown() override {
    VerifyMock();
    PlatformTest::TearDown();
  }

 protected:
  // The navigation controller that displays the view_controller_.
  // It is not used in test. However, it’s accessed by the view controller, so
  // we must not let it be deallocated until tests are done.
  UINavigationController* navigation_controller_;
  AccountMenuViewController* view_controller_;
  raw_ptr<ChromeAccountManagerService> account_manager_service_;
  id<AccountMenuMutator> mutator_;
  FakeAccountMenuDataSource* data_source_ =
      [[FakeAccountMenuDataSource alloc] init];
  NSIndexPath* path_for_secondary_account_ = [NSIndexPath indexPathForRow:0
                                                                inSection:0];
  NSIndexPath* path_for_sign_out_ = [NSIndexPath indexPathForRow:0 inSection:1];
  NSIndexPath* path_for_add_account_ = [NSIndexPath indexPathForRow:1
                                                          inSection:0];
  raw_ptr<AuthenticationService> authentication_service_;
  raw_ptr<FakeSystemIdentityManager> fake_system_identity_manager_;
  base::UserActionTester user_actions_;

  // Verify that all mocks expectation are fulfilled.
  void VerifyMock() {
    EXPECT_OCMOCK_VERIFY((id)mutator_);
  }

  // The UITableView* of the account menu view controller.
  UITableView* TableView() { return view_controller_.tableView; }

  //  Returns the cell at `path`.
  UITableViewCell* GetCell(NSIndexPath* path) {
    return [TableView().dataSource tableView:TableView()
                       cellForRowAtIndexPath:path];
  }

  // Expects that the cell at `path` is a `TableViewTextCell` whose label’s text
  // is `text`.
  void ExpectTextAtPath(NSString* text, NSIndexPath* path) {
    UITableViewCell* add_account_cell_ = GetCell(path);
    EXPECT_TRUE([add_account_cell_ isKindOfClass:[TableViewTextCell class]]);
    TableViewTextCell* add_account_cell =
        static_cast<TableViewTextCell*>(add_account_cell_);
    EXPECT_NSEQ(add_account_cell.textLabel.text, text);
  }

  // Expects that the cell at `path` is a `TableViewTextCell` whose label’s text
  // is `text`.
  void SelectCell(NSIndexPath* path) {
    [view_controller_ tableView:TableView() didSelectRowAtIndexPath:path];
  }

 private:
  // Signs in kPrimaryIdentity as primary identity.
  void AddPrimaryIdentity() {
    fake_system_identity_manager_->AddIdentity(kPrimaryIdentity);
    authentication_service_->SignIn(
        kPrimaryIdentity, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  }

  // Add kSecondaryIdentity as a secondary identity.
  void AddSecondaryIdentity() {
    fake_system_identity_manager_->AddIdentity(kSecondaryIdentity);
  }

  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// Test the view controller when it starts.
TEST_F(AccountMenuViewControllerTest, TestDefaultSetting) {
  EXPECT_EQ(2, TableView().numberOfSections);
  // The secondary account and Add Account...
  EXPECT_EQ(2, [TableView() numberOfRowsInSection:0]);
  // Sign Out
  EXPECT_EQ(1, [TableView() numberOfRowsInSection:1]);
  UITableViewCell* secondary_account_cell =
      GetCell(path_for_secondary_account_);
  EXPECT_TRUE(
      [secondary_account_cell isKindOfClass:[TableViewAccountCell class]]);
  ExpectTextAtPath(
      l10n_util::GetNSString(IDS_IOS_OPTIONS_ACCOUNTS_ADD_ACCOUNT_BUTTON),
      path_for_add_account_);
  ExpectTextAtPath(
      l10n_util::GetNSString(IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_SIGN_OUT_ITEM),
      path_for_sign_out_);
  UIView* table_header_view_ = TableView().tableHeaderView;
  EXPECT_TRUE([table_header_view_ isKindOfClass:[CentralAccountView class]]);
  CentralAccountView* table_header_view =
      static_cast<CentralAccountView*>(table_header_view_);
  EXPECT_EQ(table_header_view.avatarImage, kPrimaryAccountAvatar);
  EXPECT_EQ(table_header_view.name, kPrimaryIdentity.userFullName);
  EXPECT_EQ(table_header_view.email, kPrimaryIdentity.userEmail);
  EXPECT_EQ(table_header_view.managed, true);
}

#pragma mark - Test tapping on the views.

// Tests tapping on the secondary account cell.
TEST_F(AccountMenuViewControllerTest, TestTapSecondaryAccount) {
  OCMExpect([mutator_ accountTappedWithGaiaID:kSecondaryIdentity.gaiaID
                                   targetRect:CGRect()])
      .ignoringNonObjectArgs();
  SelectCell(path_for_secondary_account_);
  EXPECT_EQ(1,
            user_actions_.GetActionCount("Signin_AccountMenu_SelectAccount"));
}

// Tests tapping on the add account cell.
TEST_F(AccountMenuViewControllerTest, TestTapAddAccount) {
  OCMExpect([mutator_ didTapAddAccount]);
  SelectCell(path_for_add_account_);
  EXPECT_EQ(1, user_actions_.GetActionCount("Signin_AccountMenu_AddAccount"));
}

// Tests tapping on the sign-out cell.
TEST_F(AccountMenuViewControllerTest, TestTapSignOut) {
  OCMExpect([mutator_ signOutFromTargetRect:CGRect()]).ignoringNonObjectArgs();
  SelectCell(path_for_sign_out_);
  EXPECT_EQ(1, user_actions_.GetActionCount("Signin_AccountMenu_Signout"));
}

#pragma mark - AccountMenuConsumer

// Tests tapping on error action button.
TEST_F(AccountMenuViewControllerTest, TestSetError) {
  AccountErrorUIInfo* errorInfo = [[AccountErrorUIInfo alloc]
       initWithErrorType:syncer::SyncService::UserActionableError::
                             kNeedsPassphrase
      userActionableType:AccountErrorUserActionableType::kEnterPassphrase
               messageID:IDS_IOS_ACCOUNT_TABLE_ERROR_ENTER_PASSPHRASE_MESSAGE
           buttonLabelID:IDS_IOS_ACCOUNT_TABLE_ERROR_ENTER_PASSPHRASE_BUTTON];
  data_source_.accountErrorUIInfo = errorInfo;
  [view_controller_ updateErrorSection:errorInfo];
  EXPECT_EQ(3, TableView().numberOfSections);
  // The error section
  EXPECT_EQ(2, [TableView() numberOfRowsInSection:0]);
  // The secondary account and Add Account...
  EXPECT_EQ(2, [TableView() numberOfRowsInSection:0]);
  // Sign Out
  EXPECT_EQ(1, [TableView() numberOfRowsInSection:2]);

  NSIndexPath* path_for_error_message = [NSIndexPath indexPathForRow:0
                                                           inSection:0];
  UITableViewCell* error_message_cell_ = GetCell(path_for_error_message);
  EXPECT_TRUE(
      [error_message_cell_ isKindOfClass:[SettingsImageDetailTextCell class]]);
  SettingsImageDetailTextCell* error_message_cell =
      static_cast<SettingsImageDetailTextCell*>(error_message_cell_);
  EXPECT_NSEQ(error_message_cell.detailTextLabel.text,
              l10n_util::GetNSString(
                  IDS_IOS_ACCOUNT_TABLE_ERROR_ENTER_PASSPHRASE_MESSAGE));
  NSIndexPath* path_for_error_button = [NSIndexPath indexPathForRow:1
                                                          inSection:0];
  ExpectTextAtPath(l10n_util::GetNSString(
                       IDS_IOS_ACCOUNT_TABLE_ERROR_ENTER_PASSPHRASE_BUTTON),
                   path_for_error_button);

  OCMExpect([mutator_ didTapErrorButton]);
  SelectCell(path_for_error_button);
  EXPECT_EQ(1, user_actions_.GetActionCount("Signin_AccountMenu_ErrorButton"));
}

// Tests that adding an account adds an extra row in the secondary account
// section.
TEST_F(AccountMenuViewControllerTest, TestAddAccount) {
  fake_system_identity_manager_->AddIdentity(kSecondaryIdentity2);
  [view_controller_
      updateAccountListWithGaiaIDsToAdd:@[ kSecondaryIdentity2.gaiaID ]
                        gaiaIDsToRemove:@[]];
  EXPECT_EQ(2, TableView().numberOfSections);
  // The secondary accounts and Add Account...
  EXPECT_EQ(3, [TableView() numberOfRowsInSection:0]);
  // Sign Out
  EXPECT_EQ(1, [TableView() numberOfRowsInSection:1]);
}

// Test that removing a secondary account remove a row in the secondary account
// section.
TEST_F(AccountMenuViewControllerTest, TestRemoveAccount) {
  [view_controller_
      updateAccountListWithGaiaIDsToAdd:@[]
                        gaiaIDsToRemove:@[ kSecondaryIdentity.gaiaID ]];
  EXPECT_EQ(2, TableView().numberOfSections);
  // No Secondary account. Just Add Account...
  EXPECT_EQ(1, [TableView() numberOfRowsInSection:0]);
  // Sign Out
  EXPECT_EQ(1, [TableView() numberOfRowsInSection:1]);
}

// Test that updating the primary account has no discernable impact on the view
// controller.
TEST_F(AccountMenuViewControllerTest, TestUpdatePrimaryAccount) {
  [view_controller_ updatePrimaryAccount];
  EXPECT_EQ(2, TableView().numberOfSections);
  // The secondary account and Add Account...
  EXPECT_EQ(2, [TableView() numberOfRowsInSection:0]);
  // Sign Out
  EXPECT_EQ(1, [TableView() numberOfRowsInSection:1]);
}

