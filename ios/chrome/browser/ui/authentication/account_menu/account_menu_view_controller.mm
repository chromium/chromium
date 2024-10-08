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
#import "ios/chrome/browser/policy/model/management_state.h"
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
#import "ios/chrome/browser/ui/authentication/cells/central_account_view.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_account_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_cell.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

const char kEditAccountListIdentifier[] = "kEditAccountListIdentifier";
const char kManageYourGoogleAccountIdentifier[] =
    "kManageYourGoogleAccountIdentifier";

namespace {

// The margin between the cell and the sheet.
constexpr CGFloat kSideMargins = 16.;

// Size of the symbols.
constexpr CGFloat kErrorSymbolSize = 22.;

// Height and width of the buttons.
constexpr CGFloat kButtonSize = 30.;

// The height of the footer of sections, except for last section.
constexpr CGFloat kFooterHeight = 16.;

// The left separator inset between two secondary accounts.
constexpr CGFloat kSecondaryAccountsLeftSeparatorInset = 16.;

// The left separator inset between the last secondary account and Add Account.
constexpr CGFloat kLastSecondaryAccountLeftSeparatorInset = 60.;

// Per Apple guidelines, touch targets should be at least 44x44.
constexpr CGFloat kMinimumTouchTargetSize = 44.0;

constexpr CGFloat kHalfSheetCornerRadius = 10.0;

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
  UIButton* _closeButton;
  UIButton* _ellipsisButton;
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
  [self setUpTopButtons];
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

// Creates a button for the navigation bar.
- (UIButton*)addTopButtonWithSymbolName:(NSString*)symbolName
                    symbolConfiguration:
                        (UIImageSymbolConfiguration*)symbolConfiguration
                              isLeading:(BOOL)isLeading
                accessibilityIdentifier:(NSString*)accessibilityIdentifier {
  NSArray<UIColor*>* colors = @[
    [UIColor colorNamed:kTextSecondaryColor],
    [UIColor colorNamed:kUpdatedTertiaryBackgroundColor]
  ];

  UIImage* image = SymbolWithPalette(
      DefaultSymbolWithConfiguration(symbolName, symbolConfiguration), colors);

  // Add padding on all sides of the button, to make it a 44x44 touch target.
  CGFloat verticalInsets = (kMinimumTouchTargetSize - image.size.height) / 2.0;
  CGFloat horizontalInsets = (kMinimumTouchTargetSize - image.size.width) / 2.0;
  CGFloat distanceToSide = kSideMargins - horizontalInsets;
  CGFloat distanceToTop = kSideMargins - verticalInsets;

  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(
      verticalInsets, horizontalInsets, verticalInsets, horizontalInsets);
  UIButton* button = [UIButton buttonWithConfiguration:buttonConfiguration
                                         primaryAction:nil];
  [button setImage:image forState:UIControlStateNormal];
  button.translatesAutoresizingMaskIntoConstraints = NO;

  button.accessibilityIdentifier = accessibilityIdentifier;
  [self.navigationController.navigationBar addSubview:button];
  if (isLeading) {
    [button.leadingAnchor
        constraintEqualToAnchor:self.navigationController.navigationBar
                                    .leadingAnchor
                       constant:distanceToSide]
        .active = YES;
  } else {
    [button.trailingAnchor
        constraintEqualToAnchor:self.navigationController.navigationBar
                                    .trailingAnchor
                       constant:-distanceToSide]
        .active = YES;
  }
  [button.topAnchor
      constraintEqualToAnchor:self.navigationController.navigationBar.topAnchor
                     constant:distanceToTop]
      .active = YES;

  return button;
}

// Sets up the buttons.
- (void)setUpTopButtons {
  UIImageSymbolConfiguration* symbolConfiguration = [UIImageSymbolConfiguration
      configurationWithPointSize:kButtonSize
                          weight:UIImageSymbolWeightRegular
                           scale:UIImageSymbolScaleMedium];
  // Stop button
  UIUserInterfaceIdiom idiom = [[UIDevice currentDevice] userInterfaceIdiom];
  if (idiom != UIUserInterfaceIdiomPad) {
    _closeButton = [self addTopButtonWithSymbolName:kXMarkCircleFillSymbol
                                symbolConfiguration:symbolConfiguration
                                          isLeading:NO
                            accessibilityIdentifier:kAccountMenuCloseButtonId];
    [_closeButton addTarget:self
                     action:@selector(userTappedOnClose)
           forControlEvents:UIControlEventTouchUpInside];
  }

  // Ellipsis button
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
                [self.mutator didTapManageYourGoogleAccount];
              }];
  manageYourAccountAction.subtitle = [self.dataSource primaryAccountEmail];

  UIAction* editAccountListAction = [UIAction
      actionWithTitle:l10n_util::GetNSString(
                          IDS_IOS_ACCOUNT_MENU_EDIT_ACCOUNT_LIST)
                image:DefaultSymbolWithConfiguration(@"pencil",
                                                     symbolConfiguration)
           identifier:base::SysUTF8ToNSString(kEditAccountListIdentifier)
              handler:^(UIAction* action) {
                base::RecordAction(base::UserMetricsAction(
                    "Signin_AccountMenu_EditAccountList"));
                [self.mutator didTapEditAccountList];
              }];

  UIMenu* ellipsisMenu = [UIMenu
      menuWithChildren:@[ manageYourAccountAction, editAccountListAction ]];

  _ellipsisButton =
      [self addTopButtonWithSymbolName:kEllipsisCircleFillSymbol
                   symbolConfiguration:symbolConfiguration
                             isLeading:YES
               accessibilityIdentifier:kAccountMenuSecondaryActionMenuButtonId];
  _ellipsisButton.menu = ellipsisMenu;
  _ellipsisButton.showsMenuAsPrimaryAction = true;
}

- (UITableViewCell*)cellForTableView:(UITableView*)tableView
                           indexPath:(NSIndexPath*)indexPath
                      itemIdentifier:(id)itemIdentifier {
  NSString* gaiaID = base::apple::ObjCCast<NSString>(itemIdentifier);
  if (gaiaID) {
    // `itemIdentifier` is a gaia id.
    TableViewAccountCell* cell =
        DequeueTableViewCell<TableViewAccountCell>(tableView);
    cell.accessibilityTraits = UIAccessibilityTraitButton;

    cell.imageView.image = [self.dataSource imageForGaiaID:gaiaID];
    cell.textLabel.text = [self.dataSource nameForGaiaID:gaiaID];
    cell.detailTextLabel.text = [self.dataSource emailForGaiaID:gaiaID];
    cell.detailTextLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    cell.userInteractionEnabled = YES;
    cell.accessibilityIdentifier = kAccountMenuSecondaryAccountButtonId;
    BOOL lastSecondaryIdentity =
        (indexPath.row == [_accountMenuDataSource tableView:self.tableView
                                      numberOfRowsInSection:indexPath.section] -
                              2);
    cell.separatorInset = UIEdgeInsetsMake(
        0., /*left=*/
        (lastSecondaryIdentity) ? kSecondaryAccountsLeftSeparatorInset
                                : kLastSecondaryAccountLeftSeparatorInset,
        0., 0.);
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
      cell.selectionStyle = UITableViewCellSelectionStyleNone;
      cell.accessibilityIdentifier = kAccountMenuErrorMessageId;
      cell.detailTextLabel.text =
          l10n_util::GetNSString(self.dataSource.accountErrorUIInfo.messageID);
      cell.image =
          DefaultSymbolWithPointSize(kErrorCircleFillSymbol, kErrorSymbolSize);
      cell.detailTextLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
      [cell setImageViewTintColor:[UIColor colorNamed:kRed500Color]];
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

  TableViewTextCell* cell = DequeueTableViewCell<TableViewTextCell>(tableView);
  cell.accessibilityTraits = UIAccessibilityTraitButton;
  cell.isAccessibilityElement = YES;
  cell.textLabel.text = label;
  cell.accessibilityLabel = label;
  cell.textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  cell.textLabel.textColor = [UIColor colorNamed:kBlueColor];
  cell.userInteractionEnabled = YES;
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
  [self.mutator viewControllerWantsToBeClosed:self];
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
        [self.mutator didTapAddAccount];
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
        [self.mutator signOutFromTargetRect:cellRect];
        break;
    }
  }
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  return 0;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  if (section == [self.tableView numberOfSections] - 1) {
    //  No footer space for last section.
    return 0;
  }
  return kFooterHeight;
}

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  return [[UIView alloc] initWithFrame:CGRectZero];
}

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  return [[UIView alloc] initWithFrame:CGRectZero];
}

#pragma mark - AccountMenuConsumer

- (void)updatePrimaryAccount {
  CentralAccountView* identityAccountItem = [[CentralAccountView alloc]
        initWithFrame:CGRectMake(0, 0, self.tableView.frame.size.width, 0)
          avatarImage:self.dataSource.primaryAccountAvatar
                 name:self.dataSource.primaryAccountUserFullName
                email:self.dataSource.primaryAccountEmail
      managementState:self.dataSource.managementState
      useLargeMargins:NO];
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

  [self.tableView reloadData];
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
  [self.mutator viewControllerWantsToBeClosed:self];
}

@end
