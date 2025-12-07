// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/enterprise/managed_profile_creation/managed_profile_creation_view_controller.h"

#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/authentication/ui_bundled/enterprise/managed_profile_creation/managed_profile_learn_more_coordinator.h"
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
  BOOL _keepBrowsinDataSeparate;
  BOOL _multiProfileForceMigration;

  UITableView* _tableView;
  NSLayoutConstraint* _tableViewHeightConstraint;
  UITableViewDiffableDataSource<NSNumber*, NSNumber*>* _dataSource;
}
@synthesize mergeBrowsingDataByDefault;
@synthesize canShowBrowsingDataMigration;
@synthesize browsingDataMigrationDisabledByPolicy;

- (instancetype)initWithUserEmail:(NSString*)userEmail
                     hostedDomain:(NSString*)hostedDomain
       multiProfileForceMigration:(BOOL)multiProfileForceMigration {
  self = [super init];
  if (self) {
    _userEmail = userEmail;
    _hostedDomain = hostedDomain;
    _multiProfileForceMigration = multiProfileForceMigration;
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

  // Set banner.
  self.bannerName = kEnterpriseSigninBannerSymbol;

  self.titleText =
      l10n_util::GetNSString(IDS_IOS_ENTERPRISE_PROFILE_CREATION_TITLE);
  if (self.canShowBrowsingDataMigration && mergeBrowsingDataByDefault) {
    self.subtitleText = l10n_util::GetNSString(
        IDS_IOS_ENTERPRISE_PROFILE_CREATION_ACCOUNT_KEEP_BROWSING_DATA_DESCRIPTION);
  } else if (self.browsingDataMigrationDisabledByPolicy) {
    self.subtitleText = l10n_util::GetNSString(
        IDS_IOS_ENTERPRISE_PROFILE_CREATION_ACCOUNT_KEEP_BROWSING_DATA_DISABLED_DESCRIPTION);
  } else if (_multiProfileForceMigration) {
    self.subtitleText =
        l10n_util::GetNSString(IDS_IOS_ENTERPRISE_PROFILE_MIGRATION_SUBTITLE);
  } else {
    self.subtitleText =
        l10n_util::GetNSString(IDS_IOS_ENTERPRISE_PROFILE_CREATION_SUBTITLE);
  }

  self.disclaimerText = l10n_util::GetNSStringF(
      IDS_IOS_ENTERPRISE_PROFILE_CREATION_ACCOUNT_MANAGEMENT_DISCLAIMER,
      base::SysNSStringToUTF16(_userEmail),
      base::SysNSStringToUTF16(_hostedDomain));
  self.disclaimerURLs = @[ [NSURL URLWithString:kManagedProfileLearnMoreURL] ];

  // If _multiProfileForceMigration is YES, the user cannot refuse the
  // migration, and the secondary button is hidden.
  self.configuration.primaryActionString =
      _multiProfileForceMigration
          ? l10n_util::GetNSString(IDS_IOS_ENTERPRISE_PROFILE_CREATION_GOTIT)
          : l10n_util::GetNSString(
                IDS_IOS_ENTERPRISE_PROFILE_CREATION_CONTINUE);
  self.configuration.secondaryActionString =
      _multiProfileForceMigration
          ? nil
          : l10n_util::GetNSString(IDS_IOS_ENTERPRISE_PROFILE_CREATION_CANCEL);

  // Maybe add the data migration button
  if (self.canShowBrowsingDataMigration) {
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

- (void)setKeepBrowsingDataSeparate:(BOOL)keepSeparate {
  _keepBrowsinDataSeparate = keepSeparate;
  [self updateSnapshotForItemIdentifier:ItemIdentifierBrowsingData];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
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
  configuration.trailingText = l10n_util::GetNSString(
      _keepBrowsinDataSeparate
          ? IDS_IOS_ENTERPRISE_PROFILE_CREATION_ACCOUNT_KEEP_BROWSING_DATA_YES
          : IDS_IOS_ENTERPRISE_PROFILE_CREATION_ACCOUNT_KEEP_BROWSING_DATA_NO);

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
