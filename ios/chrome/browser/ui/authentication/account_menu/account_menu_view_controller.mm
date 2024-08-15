// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/check_op.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/settings/model/sync/utils/account_error_ui_info.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_constants.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_data_source.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_mutator.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_view_controller_presentation_delegate.h"
#import "ios/chrome/browser/ui/authentication/cells/central_account_view.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_account_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_cell.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

const char kEditAccountListIdentifier[] = "kEditAccountListIdentifier";
const char kManageYourGoogleAccountIdentifier[] =
    "kManageYourGoogleAccountIdentifier";

namespace {

// Size of the symbols.
constexpr CGFloat kErrorSymbolSize = 22.;

// Height and width of the buttons.
constexpr CGFloat kButtonSize = 22;

constexpr CGFloat kHalfSheetCornerRadius = 20.0;

// Sections used in the account menu.
typedef NS_ENUM(NSUInteger, SectionIdentifier) {
  // Sync errors.
  SyncErrorsSectionIdentifier = kSectionIdentifierEnumZero,
  // List of accounts
  AccountsSectionIdentifier,
  // manage accounts, sign-out
  SignOutSectionIdentifier,
};

typedef NS_ENUM(NSUInteger, RowIdentifier) {
  // Error section
  RowIdentifierErrorExplanation = kItemTypeEnumZero,
  RowIdentifierErrorButton,
  // Signout section
  RowIdentifierSignOut,
  // Accounts section.
  RowIdentifierAddAccount,
  // The secondary account entries use the gaia ID as item identifier.
};
// Custom detent identifier for when the bottom sheet is minimized.
NSString* const kCustomMinimizedDetentIdentifier = @"customMinimizedDetent";

// Custom detent identifier for when the bottom sheet is expanded.
NSString* const kCustomExpandedDetentIdentifier = @"customExpandedDetent";

}  // namespace

@implementation AccountMenuViewController {
  UITableViewDiffableDataSource* _accountMenuDataSource;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier = kAccountMenuTableViewId;
  self.tableView.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  RegisterTableViewCell<TableViewAccountCell>(self.tableView);
  RegisterTableViewCell<SettingsImageDetailTextCell>(self.tableView);
  RegisterTableViewCell<TableViewTextCell>(self.tableView);
  [self setUpNavigationController];
  [self setUpTableContent];
  [self updatePrimaryAccount];
  [self resize];
}

- (void)viewWillLayoutSubviews {
  [super viewWillLayoutSubviews];
  // Update the bottom sheet height.
  [self resize];
}

#pragma mark - Private

// Resizes the view for current content.
- (void)resize {
  // Update the bottom sheet height.
  [self.sheetPresentationController invalidateDetents];
  // Update the popover height.
  CGFloat height =
      [self.tableView
          systemLayoutSizeFittingSize:self.popoverPresentationController
                                          .containerView.bounds.size]
          .height;
  self.preferredContentSize = CGSize(self.preferredContentSize.width, height);
}

// Sets up the navigation controller’s buttons.
- (void)setUpNavigationController {
  // Stop button
  UIUserInterfaceIdiom idiom = [[UIDevice currentDevice] userInterfaceIdiom];
  if (idiom != UIUserInterfaceIdiomPad) {
    UIBarButtonItem* closeButton = [[UIBarButtonItem alloc]
        initWithBarButtonSystemItem:UIBarButtonSystemItemClose
                             target:self
                             action:@selector(userTappedOnClose)];
    closeButton.accessibilityIdentifier = kAccountMenuCloseButtonId;
    self.navigationItem.rightBarButtonItem = closeButton;
  }

  // Ellipsis button
  UIImageSymbolConfiguration* symbolConfiguration = [UIImageSymbolConfiguration
      configurationWithPointSize:kButtonSize
                          weight:UIImageSymbolWeightSemibold
                           scale:UIImageSymbolScaleMedium];
  UIAction* manageYourAccountAction = [UIAction
      actionWithTitle:
          l10n_util::GetNSString(
              IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_MANAGE_GOOGLE_ACCOUNT_ITEM)
                image:DefaultSymbolWithConfiguration(@"arrow.up.right.square",
                                                     symbolConfiguration)
           identifier:base::SysUTF8ToNSString(
                          kManageYourGoogleAccountIdentifier)
              handler:^(UIAction* action) {
                base::RecordAction(base::UserMetricsAction(
                    "Signin_AccountMenu_ManageAccount"));
                [self.delegate didTapManageYourGoogleAccount];
              }];
  // TODO(crbug.com/336719423): Add the primary account email as subtitle.

  UIAction* editAccountListAction = [UIAction
      actionWithTitle:l10n_util::GetNSString(
                          IDS_IOS_ACCOUNT_MENU_EDIT_ACCOUNT_LIST)
                image:DefaultSymbolWithConfiguration(@"pencil",
                                                     symbolConfiguration)
           identifier:base::SysUTF8ToNSString(kEditAccountListIdentifier)
              handler:^(UIAction* action) {
                base::RecordAction(base::UserMetricsAction(
                    "Signin_AccountMenu_EditAccountList"));
                [self.delegate didTapEditAccountList];
              }];

  UIMenu* ellipsisMenu = [UIMenu
      menuWithChildren:@[ manageYourAccountAction, editAccountListAction ]];
  UIImage* ellipsisImage = SymbolWithPalette(
      DefaultSymbolWithConfiguration(@"ellipsis.circle.fill",
                                     symbolConfiguration),
      @[
        [UIColor colorNamed:kGrey500Color], [UIColor colorNamed:kGrey300Color]
      ]);
  UIBarButtonItem* ellipsisButton =
      [[UIBarButtonItem alloc] initWithImage:ellipsisImage menu:ellipsisMenu];
  ellipsisButton.accessibilityIdentifier =
      kAccountMenuSecondaryActionMenuButtonId;
  self.navigationItem.leftBarButtonItem = ellipsisButton;
}

- (UITableViewCell*)cellForTableView:(UITableView*)tableView
                           indexPath:(NSIndexPath*)indexPath
                      itemIdentifier:(id)itemIdentifier {
  NSString* gaiaID = base::apple::ObjCCast<NSString>(itemIdentifier);
  if (gaiaID) {
    // `itemIdentifier` is a gaia id.
    TableViewAccountItem* item = [self.dataSource identityItemForGaiaID:gaiaID];
    TableViewAccountCell* cell =
        DequeueTableViewCell<TableViewAccountCell>(tableView);
    [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
    cell.accessibilityIdentifier = kAccountMenuSecondaryAccountButtonId;
    return cell;
  }

  // Otherwise `itemIdentifier` is a `RowIdentifier`.
  RowIdentifier rowIdentifier = static_cast<RowIdentifier>(
      base::apple::ObjCCastStrict<NSNumber>(itemIdentifier).integerValue);
  NSString* label = nil;
  NSString* accessibilityIdentifier = nil;
  switch (rowIdentifier) {
    case RowIdentifierErrorExplanation: {
      SettingsImageDetailTextCell* cell =
          DequeueTableViewCell<SettingsImageDetailTextCell>(tableView);
      SettingsImageDetailTextItem* item =
          [[SettingsImageDetailTextItem alloc] initWithType:0];
      item.detailText =
          l10n_util::GetNSString(self.dataSource.accountErrorUIInfo.messageID);
      item.image =
          DefaultSymbolWithPointSize(kErrorCircleFillSymbol, kErrorSymbolSize);
      item.imageViewTintColor = [UIColor colorNamed:kRed500Color];
      [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
      cell.selectionStyle = UITableViewCellSelectionStyleNone;
      cell.accessibilityIdentifier = kAccountMenuErrorMessageId;
      return cell;
    }
    case RowIdentifierErrorButton:
      label = l10n_util::GetNSString(
          self.dataSource.accountErrorUIInfo.buttonLabelID);
      accessibilityIdentifier = kAccountMenuErrorActionButtonId;
      break;
    case RowIdentifierAddAccount:
      label =
          l10n_util::GetNSString(IDS_IOS_OPTIONS_ACCOUNTS_ADD_ACCOUNT_BUTTON);
      accessibilityIdentifier = kAccountMenuAddAccountButtonId;
      break;
    case RowIdentifierSignOut:
      label =
          l10n_util::GetNSString(IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_SIGN_OUT_ITEM);
      accessibilityIdentifier = kAccountMenuSignoutButtonId;
      break;
    default:
      NOTREACHED();
  }
  // If the function has not returned yet. This cell contains only text.

  TableViewTextItem* item = [[TableViewTextItem alloc] init];
  item.textColor = [UIColor colorNamed:kBlueColor];
  item.accessibilityTraits = UIAccessibilityTraitButton;
  item.text = label;
  TableViewTextCell* cell = DequeueTableViewCell<TableViewTextCell>(tableView);
  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  cell.accessibilityIdentifier = accessibilityIdentifier;
  return cell;
}

- (void)setUpBottomSheetPresentationController {
  UISheetPresentationController* presentationController =
      self.sheetPresentationController;
  presentationController.prefersEdgeAttachedInCompactHeight = YES;
  presentationController.widthFollowsPreferredContentSizeWhenEdgeAttached = YES;
  presentationController.preferredCornerRadius = kHalfSheetCornerRadius;
  __weak __typeof(self) weakSelf = self;
  auto preferredHeightForContent = ^CGFloat(
      id<UISheetPresentationControllerDetentResolutionContext> context) {
    return [weakSelf preferredHeightForContent];
  };
  UISheetPresentationControllerDetent* customDetent =
      [UISheetPresentationControllerDetent
          customDetentWithIdentifier:kCustomMinimizedDetentIdentifier
                            resolver:preferredHeightForContent];
  presentationController.detents = @[ customDetent ];
  presentationController.selectedDetentIdentifier =
      kCustomMinimizedDetentIdentifier;
}

- (void)userTappedOnClose {
  base::RecordAction(base::UserMetricsAction("Signin_AccountMenu_Close"));
  [self.delegate viewControllerWantsToBeClosed:self];
}

- (void)setUpTableContent {
  // Configure the table items.
  __weak __typeof(self) weakSelf = self;
  _accountMenuDataSource = [[UITableViewDiffableDataSource alloc]
      initWithTableView:self.tableView
           cellProvider:^UITableViewCell*(UITableView* tableView,
                                          NSIndexPath* indexPath, id itemId) {
             return [weakSelf cellForTableView:tableView
                                     indexPath:indexPath
                                itemIdentifier:itemId];
           }];
  self.tableView.dataSource = _accountMenuDataSource;

  NSDiffableDataSourceSnapshot* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];

  AccountErrorUIInfo* error = self.dataSource.accountErrorUIInfo;
  if (error) {
    [snapshot appendSectionsWithIdentifiers:@[
      @(SyncErrorsSectionIdentifier),
    ]];
    [snapshot appendItemsWithIdentifiers:@[
      @(RowIdentifierErrorExplanation), @(RowIdentifierErrorButton)
    ]
               intoSectionWithIdentifier:@(SyncErrorsSectionIdentifier)];
  }

  [snapshot appendSectionsWithIdentifiers:@[ @(AccountsSectionIdentifier) ]];
  NSMutableArray* accountsIdentifiers = [[NSMutableArray alloc] init];
  NSArray<NSString*>* gaiaIDs = self.dataSource.secondaryAccountsGaiaIDs;
  for (NSString* gaiaID in gaiaIDs) {
    [accountsIdentifiers addObject:gaiaID];
  }
  [accountsIdentifiers addObject:@(RowIdentifierAddAccount)];
  [snapshot appendItemsWithIdentifiers:accountsIdentifiers
             intoSectionWithIdentifier:@(AccountsSectionIdentifier)];

  [snapshot appendSectionsWithIdentifiers:@[ @(SignOutSectionIdentifier) ]];
  [snapshot appendItemsWithIdentifiers:@[ @(RowIdentifierSignOut) ]
             intoSectionWithIdentifier:@(SignOutSectionIdentifier)];

  [_accountMenuDataSource applySnapshot:snapshot animatingDifferences:YES];
}

// Returns the sheet presentation controller if it exists.
- (UISheetPresentationController*)sheetPresentationController {
  return self.navigationController.popoverPresentationController
      .adaptiveSheetPresentationController;
}

// Returns preferred height according to the container view width.
- (CGFloat)preferredHeightForContent {
  // Get the size of the container view which is the maximum available size.
  UIView* containerView = self.sheetPresentationController.containerView;
  CGSize fittingSize = containerView.bounds.size;
  CGFloat height =
      [self.tableView systemLayoutSizeFittingSize:fittingSize].height;
  // Add the navigation bar.
  height += self.navigationController.navigationBar.frame.size.height;
  return height;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  id itemIdentifier =
      [_accountMenuDataSource itemIdentifierForIndexPath:indexPath];
  NSString* gaiaID = base::apple::ObjCCast<NSString>(itemIdentifier);
  if (gaiaID) {
    // `itemIdentifier` is a gaiaID.
    base::RecordAction(
        base::UserMetricsAction("Signin_AccountMenu_SelectAccount"));
    CGRect cellRect = [tableView rectForRowAtIndexPath:indexPath];
    [self.mutator accountTappedWithGaiaID:gaiaID targetRect:cellRect];
  } else {
    // Otherwise `itemIdentifier` is a `RowIdentifier`.
    RowIdentifier rowIdentifier = static_cast<RowIdentifier>(
        base::apple::ObjCCastStrict<NSNumber>(itemIdentifier).integerValue);
    switch (rowIdentifier) {
      case RowIdentifierAddAccount:
        base::RecordAction(
            base::UserMetricsAction("Signin_AccountMenu_AddAccount"));
        [self.delegate didTapAddAccount];
        break;
      case RowIdentifierErrorExplanation:
        break;
      case RowIdentifierErrorButton:
        base::RecordAction(
            base::UserMetricsAction("Signin_AccountMenu_ErrorButton"));
        [self.mutator didTapErrorButton];
        break;
      case RowIdentifierSignOut:
        base::RecordAction(
            base::UserMetricsAction("Signin_AccountMenu_Signout"));
        CGRect cellRect = [tableView rectForRowAtIndexPath:indexPath];
        [self.delegate signOutFromTargetRect:cellRect callback:nil];
        break;
    }
  }
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

#pragma mark - AccountMenuConsumer

- (void)updatePrimaryAccount {
  CentralAccountView* identityAccountItem = [[CentralAccountView alloc]
      initWithFrame:CGRectMake(0, 0, self.tableView.frame.size.width, 0)
        avatarImage:self.dataSource.primaryAccountAvatar
               name:self.dataSource.primaryAccountUserFullName
              email:self.dataSource.primaryAccountEmail];
  self.tableView.tableHeaderView = identityAccountItem;
  [self.tableView reloadData];
}

- (void)updateErrorSection:(AccountErrorUIInfo*)error {
  NSDiffableDataSourceSnapshot* snapshot = _accountMenuDataSource.snapshot;
  if (error == nil) {
    // The error disappeared.
    CHECK_EQ([snapshot indexOfSectionIdentifier:@(SyncErrorsSectionIdentifier)],
             0);
    [snapshot
        deleteSectionsWithIdentifiers:@[ @(SyncErrorsSectionIdentifier) ]];
  } else if ([snapshot
                 indexOfSectionIdentifier:@(SyncErrorsSectionIdentifier)] ==
             NSNotFound) {
    // The error appeared.
    [snapshot insertSectionsWithIdentifiers:@[ @(SyncErrorsSectionIdentifier) ]
                beforeSectionWithIdentifier:@(AccountsSectionIdentifier)];
    [snapshot appendItemsWithIdentifiers:@[
      @(RowIdentifierErrorExplanation), @(RowIdentifierErrorButton)
    ]
               intoSectionWithIdentifier:@(SyncErrorsSectionIdentifier)];
  } else {
    // The error changed. No need to change the sections, only their content.
  }
  [_accountMenuDataSource applySnapshot:snapshot animatingDifferences:YES];
}

- (void)updateAccountListWithGaiaIDsToAdd:(NSArray<NSString*>*)indicesToAdd
                          gaiaIDsToRemove:(NSArray<NSString*>*)gaiaIDsToRemove {
  NSDiffableDataSourceSnapshot* snapshot = _accountMenuDataSource.snapshot;

  NSMutableArray* accountsIdentifiersToAdd = [[NSMutableArray alloc] init];
  for (NSString* gaiaID in indicesToAdd) {
    [accountsIdentifiersToAdd addObject:gaiaID];
  }
  [snapshot insertItemsWithIdentifiers:accountsIdentifiersToAdd
              beforeItemWithIdentifier:@(RowIdentifierAddAccount)];

  NSMutableArray* accountsIdentifiersToRemove = [[NSMutableArray alloc] init];
  for (NSString* gaiaID in gaiaIDsToRemove) {
    [accountsIdentifiersToRemove addObject:gaiaID];
  }
  [snapshot deleteItemsWithIdentifiers:accountsIdentifiersToRemove];
  [_accountMenuDataSource applySnapshot:snapshot animatingDifferences:YES];
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
  [self.delegate viewControllerWantsToBeClosed:self];
}

@end
