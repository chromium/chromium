// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/enterprise/managed_profile_creation/learn_more_table_view_controller.h"

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
CGFloat constexpr kTableViewSeparatorInsetHide = 10000;
CGFloat constexpr kSymbolImagePointSize = 24.;
CGFloat constexpr kTextMarginTop = 8.0;
CGFloat constexpr kTextLabelSpacing = 10.0;

// Section identifiers in the browsing data page table view.
enum SectionIdentifier : NSInteger {
  SectionIdentifierManagementInfo = kSectionIdentifierEnumZero,
};

// Item identifiers in the browsing data page table view.
enum ItemIdentifier : NSInteger {
  ItemIdentifierGeneralInfo = kItemTypeEnumZero,
  ItemIdentifierBrowserInfo,
  ItemIdentifierDeviceInfo,
};
}  // namespace

@interface LearnMoreTableViewController () <UITableViewDelegate>
@end

@implementation LearnMoreTableViewController {
  NSString* _userEmail;
  NSString* _hostedDomain;
  UITableViewDiffableDataSource<NSNumber*, NSNumber*>* _dataSource;
}

- (instancetype)initWithUserEmail:(NSString*)userEmail
                     hostedDomain:(NSString*)hostedDomain {
  self = [super initWithStyle:UITableViewStylePlain];
  if (self) {
    _userEmail = userEmail;
    _hostedDomain = hostedDomain;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  self.title = l10n_util::GetNSString(
      IDS_IOS_ENTERPRISE_PROFILE_CREATION_LEARN_MORE_TITLE);
  self.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeNever;
  self.tableView.allowsSelection = NO;

  UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(doneButtonAction:)];
  self.navigationItem.rightBarButtonItem = doneButton;

  [self loadBrowsingDataTableModel];
}

#pragma mark - Actions

- (void)doneButtonAction:(id)sender {
  [self.presentationDelegate dismissLearnMoreTableViewController:self];
}

#pragma mark - UIResponder

// To always be able to register key commands via -keyCommands, the VC must be
// able to become first responder.
- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (NSArray<UIKeyCommand*>*)keyCommands {
  return @[ UIKeyCommand.cr_close ];
}

- (void)keyCommand_close {
  base::RecordAction(base::UserMetricsAction("MobileKeyCommandClose"));
  [self.presentationDelegate dismissLearnMoreTableViewController:self];
  // Request the coordinator to be stopped here
}

#pragma mark - Private

// Creates the Cell that tells the user how their data will be managed.
- (TableViewDetailIconCell*)
    createManagementInformationCellItem:(NSString*)title
                                details:(NSString*)details
                                 symbol:(NSString*)symbol {
  TableViewDetailIconCell* cell =
      DequeueTableViewCell<TableViewDetailIconCell>(self.tableView);

  cell.accessoryType = UITableViewCellAccessoryNone;
  cell.textLabel.text = title;
  cell.textLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  [cell setDetailText:details];
  cell.textLayoutConstraintAxis = UILayoutConstraintAxisVertical;
  [cell setTextLabelMarginTop:kTextMarginTop];
  if (title) {
    [cell setTextLabelSpacing:kTextLabelSpacing];
  }
  cell.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  cell.iconCenteredVertically = NO;
  cell.detailTextNumberOfLines = 0;
  cell.separatorInset =
      UIEdgeInsetsMake(0.f, kTableViewSeparatorInsetHide, 0.f, 0.f);

  if (symbol) {
    UIImageConfiguration* configuration = [UIImageSymbolConfiguration
        configurationWithPointSize:kSymbolImagePointSize
                            weight:UIImageSymbolWeightSemibold
                             scale:UIImageSymbolScaleMedium];
    [cell setIconImage:MakeSymbolMonochrome(DefaultSymbolWithConfiguration(
                           symbol, configuration))
              tintColor:[UIColor colorNamed:kGrey600Color]
        backgroundColor:[UIColor clearColor]
           cornerRadius:0.f];
  }

  return cell;
}

// Initializes the table view data model.
- (void)loadBrowsingDataTableModel {
  __weak __typeof(self) weakSelf = self;
  _dataSource = [[UITableViewDiffableDataSource alloc]
      initWithTableView:self.tableView
           cellProvider:^UITableViewCell*(UITableView* tableView,
                                          NSIndexPath* indexPath,
                                          NSNumber* itemIdentifier) {
             return
                 [weakSelf cellForTableView:tableView
                                  indexPath:indexPath
                             itemIdentifier:static_cast<ItemIdentifier>(
                                                itemIdentifier.integerValue)];
           }];

  RegisterTableViewCell<TableViewDetailIconCell>(self.tableView);

  NSDiffableDataSourceSnapshot* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot
      appendSectionsWithIdentifiers:@[ @(SectionIdentifierManagementInfo) ]];
  [snapshot appendItemsWithIdentifiers:@[
    @(ItemIdentifierGeneralInfo), @(ItemIdentifierBrowserInfo),
    @(ItemIdentifierDeviceInfo)
  ]];

  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

// Returns the cell for the corresponding `itemIdentifier`.
- (UITableViewCell*)cellForTableView:(UITableView*)tableView
                           indexPath:(NSIndexPath*)indexPath
                      itemIdentifier:(ItemIdentifier)itemIdentifier {
  switch (itemIdentifier) {
    case ItemIdentifierGeneralInfo: {
      auto details = l10n_util::GetNSStringF(
          IDS_IOS_ENTERPRISE_PROFILE_CREATION_LEARN_MORE_HEADER,
          base::SysNSStringToUTF16(_userEmail),
          base::SysNSStringToUTF16(_hostedDomain));
      return [self createManagementInformationCellItem:nil
                                               details:details
                                                symbol:nil];
    }
    case ItemIdentifierBrowserInfo: {
      auto title = l10n_util::GetNSString(
          IDS_IOS_ENTERPRISE_PROFILE_CREATION_LEARN_MORE_BROWSER_INFORMATION_TITLE);
      auto details = l10n_util::GetNSString(
          IDS_IOS_ENTERPRISE_PROFILE_CREATION_LEARN_MORE_BROWSER_INFORMATION_SUBTITLE);
      return [self createManagementInformationCellItem:title
                                               details:details
                                                symbol:kCheckmarkShieldSymbol];
    }
    case ItemIdentifierDeviceInfo: {
      auto title = l10n_util::GetNSString(
          IDS_IOS_ENTERPRISE_PROFILE_CREATION_LEARN_MORE_DEVICE_INFORMATION_TITLE);
      auto details = l10n_util::GetNSString(
          IDS_IOS_ENTERPRISE_PROFILE_CREATION_LEARN_MORE_DEVICE_INFORMATION_SUBTITLE);
      return [self createManagementInformationCellItem:title
                                               details:details
                                                symbol:kIPhoneSymbol];
    }
  }
  NOTREACHED();
}

@end
