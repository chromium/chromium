// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/enterprise/managed_profile_creation/ui/managed_profile_creation_view_controller.h"

#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/authentication/enterprise/public/managed_profile_creation_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
CGFloat constexpr kTableViewCornerRadius = 10;

// Section identifiers in the browsing data page table view.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierBrowsingData = kSectionIdentifierEnumZero,
};

// Item identifiers in the browsing data page table view.
typedef NS_ENUM(NSInteger, ItemIdentifier) {
  ItemIdentifierBrowsingData = kItemTypeEnumZero
};
}  // namespace

@interface ManagedProfileCreationViewController () <UITableViewDelegate>

@end

@implementation ManagedProfileCreationViewController {
  NSString* _userEmail;
  NSString* _hostedDomain;

  UITableView* _tableView;
  NSLayoutConstraint* _tableViewHeightConstraint;
  UITableViewDiffableDataSource<NSNumber*, NSNumber*>* _dataSource;
}

- (instancetype)initWithUserEmail:(NSString*)userEmail
                     hostedDomain:(NSString*)hostedDomain {
  self = [super init];
  if (self) {
    _userEmail = userEmail;
    _hostedDomain = hostedDomain;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.view.accessibilityIdentifier =
      kManagedProfileCreationScreenAccessibilityIdentifier;
  self.bannerSize = BannerImageSizeType::kStandard;
  self.scrollToEndMandatory = YES;
  self.readMoreString =
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SCREEN_READ_MORE);
  signin::ManagedAccountSigninMode mode =
      self.managedProfileCreationDataSource.mode;

  // Set banner.
  self.bannerName = kEnterpriseSigninBannerSymbol;

  self.titleText =
      l10n_util::GetNSString(IDS_IOS_ENTERPRISE_PROFILE_CREATION_TITLE);
  switch (mode) {
    case signin::ManagedAccountSigninMode::kMustSeparateBecauseSignedIn:
    case signin::ManagedAccountSigninMode::kAutoMergeDuringFRE:
    case signin::ManagedAccountSigninMode::kSeparateProfileData:
      // Explains what the user gets from signing-in their account.
      self.subtitleText =
          l10n_util::GetNSString(IDS_IOS_ENTERPRISE_PROFILE_CREATION_SUBTITLE);
      break;
    case signin::ManagedAccountSigninMode::kMergeProfileData:
      // Same as first case, and also tells the user to select what to do with
      // their data. This should ensure the user is aware that, by default,
      // local data will become managed.
      self.subtitleText = l10n_util::GetNSString(
          IDS_IOS_ENTERPRISE_PROFILE_CREATION_ACCOUNT_KEEP_BROWSING_DATA_DESCRIPTION);
      break;
    case signin::ManagedAccountSigninMode::kForceSeparateProfileDataByPolicy:
      // Same as first case, and also informs the user that they’ll get current
      // data by signing-out.
      self.subtitleText = l10n_util::GetNSString(
          IDS_IOS_ENTERPRISE_PROFILE_CREATION_ACCOUNT_KEEP_BROWSING_DATA_DISABLED_DESCRIPTION);
      break;
    case signin::ManagedAccountSigninMode::kInformOfForcedMigration:
      // Similar to first case, except that the user is not invited to sign-in
      // as they already are in the managed account.
      self.subtitleText =
          l10n_util::GetNSString(IDS_IOS_ENTERPRISE_PROFILE_MIGRATION_SUBTITLE);
      break;
  }

  self.disclaimerText = l10n_util::GetNSStringF(
      IDS_IOS_ENTERPRISE_PROFILE_CREATION_ACCOUNT_MANAGEMENT_DISCLAIMER,
      base::SysNSStringToUTF16(_userEmail),
      base::SysNSStringToUTF16(_hostedDomain));
  self.disclaimerURLs = @[ [NSURL URLWithString:kManagedProfileLearnMoreURL] ];

  BOOL forcedMigrationDone =
      mode == signin::ManagedAccountSigninMode::kInformOfForcedMigration;
  // The migration is already done. This screens informs the user. The user has
  // thus no option to cancel.
  self.configuration.primaryActionString =
      forcedMigrationDone
          ? l10n_util::GetNSString(IDS_IOS_ENTERPRISE_PROFILE_CREATION_GOTIT)
          : l10n_util::GetNSString(
                IDS_IOS_ENTERPRISE_PROFILE_CREATION_CONTINUE);
  self.configuration.secondaryActionString =
      forcedMigrationDone
          ? nil
          : l10n_util::GetNSString(IDS_IOS_ENTERPRISE_PROFILE_CREATION_CANCEL);

  switch (mode) {
    case signin::ManagedAccountSigninMode::kForceSeparateProfileDataByPolicy:
      // Do not offer selection as it’s disabled.
      break;
    case signin::ManagedAccountSigninMode::kMustSeparateBecauseSignedIn:
      // Do not offer selection as the data belongs to the currently signed-in
      // account.
      break;
    case signin::ManagedAccountSigninMode::kAutoMergeDuringFRE:
      // Do not offer selection as the user just installed chrome and there is
      // no local data to migrate.
      break;
    case signin::ManagedAccountSigninMode::kInformOfForcedMigration:
      // Do not offer selection as migration is already done.
      break;
    case signin::ManagedAccountSigninMode::kSeparateProfileData:
    case signin::ManagedAccountSigninMode::kMergeProfileData:
      _tableView = [self createTableView];
      _tableView.accessibilityIdentifier =
          kBrowsingDataButtonAccessibilityIdentifier;
      [self.specificContentView addSubview:_tableView];
      [NSLayoutConstraint activateConstraints:@[
        [_tableView.topAnchor
            constraintEqualToAnchor:self.specificContentView.topAnchor],
        [_tableView.centerXAnchor
            constraintEqualToAnchor:self.specificContentView.centerXAnchor],
        [_tableView.widthAnchor
            constraintEqualToAnchor:self.specificContentView.widthAnchor],
        [_tableView.bottomAnchor
            constraintLessThanOrEqualToAnchor:self.specificContentView
                                                  .bottomAnchor],
      ]];
      [self loadBrowsingDataTableModel];
      break;
  }

  // Call super after setting up the strings and others, as required per super
  // class.
  [super viewDidLoad];
}

- (void)viewWillLayoutSubviews {
  [super viewWillLayoutSubviews];
  [self updateTableViewHeightConstraint];
}

#pragma mark - ManagedProfileCreationConsumer

- (void)userChangedSelection {
  [self updateSnapshotForItemIdentifier:ItemIdentifierBrowsingData];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  // The only cell of this table is the button "Keep data separate ?"; no need
  // to check the value of the index path.
  [_tableView deselectRowAtIndexPath:indexPath animated:YES];
  [self.managedProfileCreationViewControllerPresentationDelegate
          showMergeBrowsingDataScreen];
}

#pragma mark - Private

// Creates the Cell that allows the user to select how to handle browsing data.
- (UITableViewCell*)createBrowsingDataMigrationCellItem {
  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];
  configuration.title = l10n_util::GetNSString(
      IDS_IOS_ENTERPRISE_PROFILE_CREATION_ACCOUNT_KEEP_BROWSING_DATA_LABEL);
  switch (self.managedProfileCreationDataSource.mode) {
    case signin::ManagedAccountSigninMode::kSeparateProfileData:
      configuration.trailingText = l10n_util::GetNSString(
          IDS_IOS_ENTERPRISE_PROFILE_CREATION_ACCOUNT_KEEP_BROWSING_DATA_YES);
      break;
    case signin::ManagedAccountSigninMode::kMergeProfileData:
      configuration.trailingText = l10n_util::GetNSString(
          IDS_IOS_ENTERPRISE_PROFILE_CREATION_ACCOUNT_KEEP_BROWSING_DATA_NO);
      break;
    case signin::ManagedAccountSigninMode::kForceSeparateProfileDataByPolicy:
    case signin::ManagedAccountSigninMode::kMustSeparateBecauseSignedIn:
    case signin::ManagedAccountSigninMode::kAutoMergeDuringFRE:
    case signin::ManagedAccountSigninMode::kInformOfForcedMigration:
      NOTREACHED();
  }
  UITableViewCell* cell =
      [TableViewCellContentConfiguration dequeueTableViewCell:_tableView];

  cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  cell.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
  cell.contentConfiguration = configuration;
  return cell;
}

// Creates the table view.
- (UITableView*)createTableView {
  UITableView* tableView =
      [[UITableView alloc] initWithFrame:CGRectZero
                                   style:UITableViewStylePlain];
  tableView.tableFooterView =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, CGFLOAT_MIN)];
  tableView.estimatedRowHeight = UITableViewAutomaticDimension;
  tableView.layer.cornerRadius = kTableViewCornerRadius;
  tableView.scrollEnabled = NO;
  tableView.showsVerticalScrollIndicator = NO;
  tableView.delegate = self;
  tableView.userInteractionEnabled = YES;
  tableView.translatesAutoresizingMaskIntoConstraints = NO;
  _tableViewHeightConstraint =
      [tableView.heightAnchor constraintEqualToConstant:0];
  _tableViewHeightConstraint.active = YES;

  return tableView;
}

- (void)loadBrowsingDataTableModel {
  __weak __typeof(self) weakSelf = self;
  _dataSource = [[UITableViewDiffableDataSource alloc]
      initWithTableView:_tableView
           cellProvider:^UITableViewCell*(UITableView* tableView,
                                          NSIndexPath* indexPath,
                                          NSNumber* itemIdentifier) {
             return
                 [weakSelf cellForTableView:tableView
                                  indexPath:indexPath
                             itemIdentifier:static_cast<ItemIdentifier>(
                                                itemIdentifier.integerValue)];
           }];

  [TableViewCellContentConfiguration registerCellForTableView:_tableView];

  NSDiffableDataSourceSnapshot* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot
      appendSectionsWithIdentifiers:@[ @(SectionIdentifierBrowsingData) ]];
  [snapshot appendItemsWithIdentifiers:@[ @(ItemIdentifierBrowsingData) ]];

  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

// Returns the cell for the corresponding `itemIdentifier`.
- (UITableViewCell*)cellForTableView:(UITableView*)tableView
                           indexPath:(NSIndexPath*)indexPath
                      itemIdentifier:(ItemIdentifier)itemIdentifier {
  switch (itemIdentifier) {
    case ItemIdentifierBrowsingData: {
      return [self createBrowsingDataMigrationCellItem];
    }
  }
  NOTREACHED();
}

// Reloads the snapshot for the cell with the given `itemIdentifier`.
- (void)updateSnapshotForItemIdentifier:(ItemIdentifier)itemIdentifier {
  NSDiffableDataSourceSnapshot<NSNumber*, NSNumber*>* snapshot =
      [_dataSource snapshot];
  [snapshot reloadItemsWithIdentifiers:@[ @(itemIdentifier) ]];
  [_dataSource applySnapshot:snapshot animatingDifferences:YES];
}

// Updates the tableView's height constraint.
- (void)updateTableViewHeightConstraint {
  [_tableView layoutIfNeeded];
  _tableViewHeightConstraint.constant = _tableView.contentSize.height;
}

@end
