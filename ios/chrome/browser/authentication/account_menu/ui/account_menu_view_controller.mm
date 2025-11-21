// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/account_menu/ui/account_menu_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/check_op.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/authentication/account_menu/public/account_menu_constants.h"
#import "ios/chrome/browser/authentication/account_menu/ui/account_menu_data_source.h"
#import "ios/chrome/browser/authentication/account_menu/ui/account_menu_mutator.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/central_account_view.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/table_view_account_item.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/policy/model/management_state.h"
#import "ios/chrome/browser/settings/model/sync/utils/account_error_ui_info.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/image_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The margin between the cell and the sheet.
constexpr CGFloat kSideMargins = 16.;

const CGFloat kButtonImageSize = 18;

// Size of the symbols.
constexpr CGFloat kErrorSymbolSize = 22.;

// The height of the footer of sections, except for last section.
constexpr CGFloat kFooterHeight = 16.;

// The left inset for the separators.
constexpr CGFloat kSeparatorInset = 60.;

// Per Apple guidelines, touch targets should be at least 44x44.
constexpr CGFloat kMinimumTouchTargetSize = 44.0;

// Sections used in the account menu.
typedef NS_ENUM(NSUInteger, SectionIdentifier) {
  // Sync errors.
  SyncErrorsSectionIdentifier = kSectionIdentifierEnumZero,
  // List of accounts.
  AccountsSectionIdentifier,
  // Sign-out.
  SignOutSectionIdentifier,
};

typedef NS_ENUM(NSUInteger, RowIdentifier) {
  // Error section.
  RowIdentifierErrorExplanation = kItemTypeEnumZero,
  RowIdentifierErrorButton,
  // Signout section.
  RowIdentifierSignOut,
  // Accounts section.
  RowIdentifierAddAccount,
  RowIdentifierManageAccounts,
  // The secondary account entries use the gaia ID as item identifier.
};

// Custom detent identifier for when the bottom sheet is minimized.
NSString* const kCustomMinimizedDetentIdentifier = @"customMinimizedDetent";

// Custom detent identifier for when the bottom sheet is expanded.
NSString* const kCustomExpandedDetentIdentifier = @"customExpandedDetent";

}  // namespace

@interface AccountMenuViewController () <UITableViewDelegate>

@property(nonatomic, strong) UITableView* tableView;

@end

@implementation AccountMenuViewController {
  UITableViewDiffableDataSource* _accountMenuDataSource;
  UIBarButtonItem* _closeButton;
  UIBarButtonItem* _ellipsisButton;
  CentralAccountView* _identityAccountView;
  // The index path of the cell on which the user tapped while account switching
  // is in progress. It should be reset to nil before any table content occurs.
  NSIndexPath* _selectedIndexPath;
  // Set to true when the content is set and it is possible to compute the size
  // of the popover.
  // If preferredContentSize is set with different values in the same runloop,
  // UIKit will pick the biggest or the first one (but not the last one).
  BOOL _resizeReady;
  // Whether or not to hide the ellipsis menu.
  BOOL _hideEllipsisMenu;
}

- (instancetype)initWithHideEllipsisMenu:(BOOL)hideEllipsisMenu {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _hideEllipsisMenu = hideEllipsisMenu;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  if (!self.dataSource) {
    // The account menu has been stopped before the view loaded.
    // The content of this view can not actually be computed.
    return;
  }
  _resizeReady = NO;
  self.tableView =
      [[UITableView alloc] initWithFrame:CGRectZero
                                   style:UITableViewStyleInsetGrouped];
  UITableView* tableView = self.tableView;
  tableView.separatorInset = UIEdgeInsetsMake(0., /*left=*/
                                              kSeparatorInset, 0., 0.);

  tableView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:tableView];
  [NSLayoutConstraint activateConstraints:@[
    [self.view.topAnchor constraintEqualToAnchor:tableView.topAnchor],
    [self.view.bottomAnchor constraintEqualToAnchor:tableView.bottomAnchor],
    [self.view.trailingAnchor constraintEqualToAnchor:tableView.trailingAnchor],
    [self.view.leadingAnchor constraintEqualToAnchor:tableView.leadingAnchor],
  ]];
  tableView.delegate = self;
  tableView.accessibilityIdentifier = kAccountMenuTableViewId;
  tableView.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  RegisterTableViewCell<TableViewAccountCell>(tableView);
  [TableViewCellContentConfiguration registerCellForTableView:tableView];
  [self setUpTopButtons];
  [self setUpTableContent];
  [self updatePrimaryAccount];
  tableView.tableFooterView = [[UIView alloc]
      initWithFrame:CGRectMake(0, 0, CGFLOAT_EPSILON, CGFLOAT_EPSILON)];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  [self updateCloseButton];
  // Update the bottom sheet height.
  [self resize];
}

- (void)viewIsAppearing:(BOOL)animated {
  [super viewIsAppearing:animated];
  _resizeReady = YES;
}

#pragma mark - Private

// Sets the activity indicator.
- (void)setActivityIndicator:(TableViewAccountCell*)cell {
  UIActivityIndicatorView* activityIndicatorView =
      [[UIActivityIndicatorView alloc] init];
  activityIndicatorView.translatesAutoresizingMaskIntoConstraints = NO;
  activityIndicatorView.color = [UIColor colorNamed:kTextSecondaryColor];
  [cell setStatusView:activityIndicatorView];
  [activityIndicatorView startAnimating];
}

// Returns the height of the navigation bar.
- (CGFloat)navigationBarHeight {
  return self.navigationController.navigationBar.frame.size.height;
}

// Resizes the view for current content.
- (void)resize {
  if (!_resizeReady) {
    return;
  }
  // Update the bottom sheet height.
  [self.sheetPresentationController invalidateDetents];
  // Update the popover height.
  [_identityAccountView updateTopPadding:[self navigationBarHeight]];
  // Force the layout of the TableView to make sure that it has the right width
  // before using its contentSize.
  [self.tableView setNeedsLayout];
  [self.tableView layoutIfNeeded];
  CGFloat height = self.tableView.contentSize.height;
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
    [UIColor colorNamed:kTertiaryBackgroundColor]
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
  // Close button
  [self updateCloseButton];

  // Ellipsis button
  if (_hideEllipsisMenu) {
    return;
  }
  UIImageSymbolConfiguration* symbolConfiguration = [UIImageSymbolConfiguration
      configurationWithPointSize:kSymbolActionPointSize
                          weight:UIImageSymbolWeightRegular
                           scale:UIImageSymbolScaleMedium];
  UIAction* manageYourAccountAction = [UIAction
      actionWithTitle:
          l10n_util::GetNSString(
              IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_MANAGE_GOOGLE_ACCOUNT_ITEM)
                image:DefaultSymbolWithConfiguration(@"arrow.up.right.square",
                                                     symbolConfiguration)
           identifier:kAccountMenuManageYourGoogleAccountId
              handler:^(UIAction* action) {
                base::RecordAction(base::UserMetricsAction(
                    "Signin_AccountMenu_ManageAccount"));
                [self.mutator didTapManageYourGoogleAccount];
              }];
  manageYourAccountAction.subtitle = [self.dataSource primaryAccountEmail];

  UIMenu* ellipsisMenu;
  UIAction* editAccountListAction =
      [UIAction actionWithTitle:l10n_util::GetNSString(
                                    IDS_IOS_ACCOUNT_MENU_EDIT_ACCOUNT_LIST)
                          image:DefaultSymbolWithConfiguration(
                                    @"pencil", symbolConfiguration)
                     identifier:kAccountMenuEditAccountListId
                        handler:^(UIAction* action) {
                          base::RecordAction(base::UserMetricsAction(
                              "Signin_AccountMenu_EditAccountList"));
                          [self.mutator didTapManageAccounts];
                        }];
  ellipsisMenu = [UIMenu
      menuWithChildren:@[ manageYourAccountAction, editAccountListAction ]];

  _ellipsisButton = [[UIBarButtonItem alloc]
      initWithImage:DefaultSymbolWithPointSize(kMenuSymbol, kButtonImageSize)
               menu:ellipsisMenu];
  _ellipsisButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_ICON_OPTION_MENU);
  _ellipsisButton.accessibilityIdentifier =
      kAccountMenuSecondaryActionMenuButtonId;

  self.navigationItem.leftBarButtonItem = _ellipsisButton;
}

// Decides if the Close button should be shown.
- (BOOL)shouldShowCloseButton {
  UIUserInterfaceIdiom idiom = [[UIDevice currentDevice] userInterfaceIdiom];
  return idiom == UIUserInterfaceIdiomPhone ||
         self.presentingViewController.traitCollection.horizontalSizeClass ==
             UIUserInterfaceSizeClassCompact;
}

// Adds or removes the Close button based on the device type and collection.
- (void)updateCloseButton {
  BOOL isCloseButtonShown = _closeButton;
  BOOL shouldShowCloseButton = [self shouldShowCloseButton];
  if (shouldShowCloseButton == isCloseButtonShown) {
    return;
  }
  if (shouldShowCloseButton) {
    // Add the Close button.
    _closeButton = [[UIBarButtonItem alloc]
        initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                             target:self
                             action:@selector(userTappedOnClose)];
    _closeButton.accessibilityIdentifier = kAccountMenuCloseButtonId;

    self.navigationItem.rightBarButtonItem = _closeButton;

  } else {
    // Remove the Close button.
    self.navigationItem.rightBarButtonItem = nil;
    _closeButton = nil;
  }
}

// Configures and returns a cell.
- (UITableViewCell*)cellForTableView:(UITableView*)tableView
                           indexPath:(NSIndexPath*)indexPath
                      itemIdentifier:(id)itemIdentifier {
  NSString* gaiaIDString = base::apple::ObjCCast<NSString>(itemIdentifier);
  if (gaiaIDString) {
    return [self cellForTableView:tableView
                           gaiaID:GaiaId(gaiaIDString)
                        indexPath:indexPath];
  }

  // Otherwise `itemIdentifier` is a `RowIdentifier`.
  RowIdentifier rowIdentifier = static_cast<RowIdentifier>(
      base::apple::ObjCCastStrict<NSNumber>(itemIdentifier).integerValue);
  NSString* label = nil;
  NSString* accessibilityIdentifier = nil;
  NSString* accessibilityLabel = nil;
  switch (rowIdentifier) {
    case RowIdentifierErrorExplanation: {
      return [self cellForErrorExplanationForTableView:tableView];
    }
    case RowIdentifierErrorButton:
      label = l10n_util::GetNSString(
          self.dataSource.accountErrorUIInfo.buttonLabelID);
      accessibilityLabel =
          l10n_util::GetNSString(self.dataSource.accountErrorUIInfo.messageID);
      accessibilityIdentifier = kAccountMenuErrorActionButtonId;
      break;
    case RowIdentifierAddAccount:
      label =
          l10n_util::GetNSString(IDS_IOS_OPTIONS_ACCOUNTS_ADD_ACCOUNT_BUTTON);
      accessibilityIdentifier = kAccountMenuAddAccountButtonId;
      break;
    case RowIdentifierManageAccounts:
      label = l10n_util::GetNSString(IDS_IOS_ACCOUNT_MENU_EDIT_ACCOUNT_LIST);
      accessibilityIdentifier = kAccountMenuManageAccountsButtonId;
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

  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];
  configuration.title = label;
  configuration.titleColor = [UIColor colorNamed:kBlueColor];

  UITableViewCell* cell =
      [TableViewCellContentConfiguration dequeueTableViewCell:tableView];

  cell.contentConfiguration = configuration;
  cell.accessibilityTraits = UIAccessibilityTraitButton;
  cell.isAccessibilityElement = YES;
  cell.accessibilityLabel = accessibilityLabel ? accessibilityLabel : label;
  cell.userInteractionEnabled = YES;
  cell.accessibilityIdentifier = accessibilityIdentifier;

  return cell;
}

// Returns a cell for signing-in with the account with `gaiaID`.
- (UITableViewCell*)cellForTableView:(UITableView*)tableView
                              gaiaID:(const GaiaId&)gaiaID
                           indexPath:(NSIndexPath*)indexPath {
  // `itemIdentifier` is a gaia id.
  TableViewAccountCell* cell =
      DequeueTableViewCell<TableViewAccountCell>(tableView);
  cell.accessibilityTraits = UIAccessibilityTraitButton;

  cell.imageView.image = [self.dataSource imageForGaiaID:gaiaID];
  cell.textLabel.text = [self.dataSource nameForGaiaID:gaiaID];
  NSString* name = [self.dataSource nameForGaiaID:gaiaID];
  NSString* email = [self.dataSource emailForGaiaID:gaiaID];
  cell.detailTextLabel.text = email;
  cell.detailTextLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  BOOL isGaiaIDManaged = [self.dataSource isGaiaIDManaged:gaiaID];
  if (name) {
    cell.accessibilityLabel = l10n_util::GetNSStringF(
        isGaiaIDManaged
            ? IDS_IOS_OPTIONS_ACCOUNTS_SIGNIN_WITH_NAME_MANAGED_ACCESSIBILITY_LABEL
            : IDS_IOS_OPTIONS_ACCOUNTS_SIGNIN_WITH_NAME_ACCESSIBILITY_LABEL,
        base::SysNSStringToUTF16(name), base::SysNSStringToUTF16(email));
  } else {
    cell.accessibilityLabel = l10n_util::GetNSStringF(
        isGaiaIDManaged
            ? IDS_IOS_OPTIONS_ACCOUNTS_SIGNIN_MANAGED_ACCESSIBILITY_LABEL
            : IDS_IOS_OPTIONS_ACCOUNTS_SIGNIN_ACCESSIBILITY_LABEL,
        base::SysNSStringToUTF16(email));
  }
  cell.userInteractionEnabled = YES;
  cell.accessibilityIdentifier = kAccountMenuSecondaryAccountButtonId;
  // Set the enterprise icon. This may be replaced by the activity indicator
  // when needed.
  [cell showManagementIcon:isGaiaIDManaged];

  if ([indexPath isEqual:_selectedIndexPath]) {
    // In theory, this can occur if, during the account switch process, the
    // user scrolls a lot, and scroll back.
    [self setActivityIndicator:cell];
  }
  return cell;
}

// Returns a cell for the error explanation.
- (UITableViewCell*)cellForErrorExplanationForTableView:
    (UITableView*)tableView {
  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];
  configuration.subtitle =
      l10n_util::GetNSString(self.dataSource.accountErrorUIInfo.messageID);

  ImageContentConfiguration* imageConfiguration =
      [[ImageContentConfiguration alloc] init];
  imageConfiguration.image =
      DefaultSymbolWithPointSize(kErrorCircleFillSymbol, kErrorSymbolSize);
  imageConfiguration.imageTintColor = [UIColor colorNamed:kRed500Color];

  configuration.leadingConfiguration = imageConfiguration;

  UITableViewCell* cell =
      [TableViewCellContentConfiguration dequeueTableViewCell:tableView];

  cell.contentConfiguration = configuration;

  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  cell.accessibilityIdentifier = kAccountMenuErrorMessageId;
  cell.accessibilityElementsHidden = YES;
  return cell;
}

// Sets up bottom sheet presentation controller.
- (void)setUpBottomSheetPresentationController {
  UISheetPresentationController* presentationController =
      self.sheetPresentationController;
  presentationController.prefersEdgeAttachedInCompactHeight = YES;
  presentationController.widthFollowsPreferredContentSizeWhenEdgeAttached = YES;

  // In case of compact width only, adjust detents.
  if (self.traitCollection.horizontalSizeClass ==
      UIUserInterfaceSizeClassCompact) {
    __weak __typeof(self) weakSelf = self;
    auto preferredHeightForSheetContent = ^CGFloat(
        id<UISheetPresentationControllerDetentResolutionContext> context) {
      return [weakSelf preferredHeightForSheetContent];
    };
    UISheetPresentationControllerDetent* customDetent =
        [UISheetPresentationControllerDetent
            customDetentWithIdentifier:kCustomMinimizedDetentIdentifier
                              resolver:preferredHeightForSheetContent];
    presentationController.detents = @[ customDetent ];
  }

  presentationController.selectedDetentIdentifier =
      kCustomMinimizedDetentIdentifier;
}

// Handles tapping on Close button.
- (void)userTappedOnClose {
  base::RecordAction(base::UserMetricsAction("Signin_AccountMenu_Close"));
  [self.mutator viewControllerWantsToBeClosed:self];
}

// Sets up table content.
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
    [self recordAccountMenuUserActionableError:error.errorType];
  }

  [snapshot appendSectionsWithIdentifiers:@[ @(AccountsSectionIdentifier) ]];
  NSMutableArray* accountsIdentifiers = [[NSMutableArray alloc] init];
  const std::vector<GaiaId> gaiaIDs = self.dataSource.secondaryAccountsGaiaIDs;
  for (const GaiaId& gaiaID : gaiaIDs) {
    [accountsIdentifiers addObject:gaiaID.ToNSString()];
  }
  [accountsIdentifiers addObject:@(RowIdentifierAddAccount)];
  [snapshot appendItemsWithIdentifiers:accountsIdentifiers
             intoSectionWithIdentifier:@(AccountsSectionIdentifier)];

  [snapshot appendSectionsWithIdentifiers:@[ @(SignOutSectionIdentifier) ]];
  // The sign-out button has its own section.
  if (_hideEllipsisMenu) {
    [snapshot appendItemsWithIdentifiers:@[ @(RowIdentifierManageAccounts) ]
               intoSectionWithIdentifier:@(SignOutSectionIdentifier)];
  }
  [snapshot appendItemsWithIdentifiers:@[ @(RowIdentifierSignOut) ]
             intoSectionWithIdentifier:@(SignOutSectionIdentifier)];

  [_accountMenuDataSource applySnapshot:snapshot animatingDifferences:YES];
}

// Returns the sheet presentation controller of the used presentation style.
- (UISheetPresentationController*)sheetPresentationController {
  if (self.navigationController.sheetPresentationController) {
    return self.navigationController.sheetPresentationController;
  }
  return self.navigationController.popoverPresentationController
      .adaptiveSheetPresentationController;
}

// Returns preferred height according to the container view width.
- (CGFloat)preferredHeightForSheetContent {
  // This is the size of the content of the table view and the navigation bar.
  return self.tableView.contentSize.height + [self navigationBarHeight];
}

// Records that the `error` has been displayed to the user (either that it was
// visible when they navigated to account menu setting page or that it appeared
// while they were in that page).
- (void)recordAccountMenuUserActionableError:
    (syncer::SyncService::UserActionableError)error {
  base::UmaHistogramEnumeration("Sync.AccountMenu.UserActionableError", error);
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  id itemIdentifier =
      [_accountMenuDataSource itemIdentifierForIndexPath:indexPath];
  NSString* gaiaIDString = base::apple::ObjCCast<NSString>(itemIdentifier);
  if (gaiaIDString) {
    // `itemIdentifier` is a gaiaID.
    base::RecordAction(
        base::UserMetricsAction("Signin_AccountMenu_SelectAccount"));
    CGRect cellRect = [tableView rectForRowAtIndexPath:indexPath];
    _selectedIndexPath = indexPath;
    GaiaId gaiaId(gaiaIDString);
    [self.mutator accountTappedWithGaiaID:&gaiaId targetRect:cellRect];
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
        [self.mutator didTapErrorButton];
        break;
      case RowIdentifierManageAccounts:
        base::RecordAction(
            base::UserMetricsAction("Signin_AccountMenu_EditAccountList"));
        [self.mutator didTapManageAccounts];
        break;
      case RowIdentifierSignOut:
        base::RecordAction(
            base::UserMetricsAction("Signin_AccountMenu_Signout"));
        CGRect cellRect = [tableView rectForRowAtIndexPath:indexPath];
        [self.mutator signOutFromTargetRect:cellRect];
        break;
    }
    [tableView deselectRowAtIndexPath:indexPath animated:YES];
  }
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  return 0;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  if (section == [self.tableView numberOfSections] - 1) {
    //  The last footer’s height is the margin between the table and the bottom
    // of the popover/sheet.
    return kSideMargins;
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

- (void)setUserInteractionsEnabled:(BOOL)enabled {
  self.tableView.allowsSelection = enabled;
  _closeButton.enabled = enabled;
  _ellipsisButton.enabled = enabled;
}

- (void)switchingStarted {
  CHECK(_selectedIndexPath);
  TableViewAccountCell* cell =
      base::apple::ObjCCastStrict<TableViewAccountCell>(
          [self.tableView cellForRowAtIndexPath:_selectedIndexPath]);
  [self setActivityIndicator:cell];
  self.modalInPresentation = YES;
  _ellipsisButton.enabled = NO;
}

- (void)switchingStopped {
  if (!_selectedIndexPath) {
    return;
  }
  TableViewAccountCell* cell =
      base::apple::ObjCCastStrict<TableViewAccountCell>(
          [self.tableView cellForRowAtIndexPath:_selectedIndexPath]);
  [cell setStatusView:nil];
  [self.tableView deselectRowAtIndexPath:_selectedIndexPath animated:YES];
  _selectedIndexPath = nil;
  self.modalInPresentation = NO;
  _ellipsisButton.enabled = YES;
}

- (void)updatePrimaryAccount {
  _identityAccountView = [[CentralAccountView alloc]
              initWithFrame:CGRectMake(0, 0, self.tableView.frame.size.width, 0)
                avatarImage:self.dataSource.primaryAccountAvatar
                       name:self.dataSource.primaryAccountUserFullName
                      email:self.dataSource.primaryAccountEmail
      managementDescription:self.dataSource.managementDescription
            useLargeMargins:NO];
  [_identityAccountView updateTopPadding:[self navigationBarHeight]];
  self.tableView.tableHeaderView = _identityAccountView;
  [self.tableView reloadData];
  [self resize];
}

- (void)updateErrorSection:(AccountErrorUIInfo*)error {
  CHECK(!_selectedIndexPath);
  NSDiffableDataSourceSnapshot* snapshot = _accountMenuDataSource.snapshot;
  if (error == nil) {
    // The error disappeared.
    CHECK_EQ([snapshot indexOfSectionIdentifier:@(SyncErrorsSectionIdentifier)],
             0);
    [snapshot
        deleteSectionsWithIdentifiers:@[ @(SyncErrorsSectionIdentifier) ]];
  } else {
    [self recordAccountMenuUserActionableError:error.errorType];
    if ([snapshot indexOfSectionIdentifier:@(SyncErrorsSectionIdentifier)] ==
        NSNotFound) {
      [snapshot
          insertSectionsWithIdentifiers:@[ @(SyncErrorsSectionIdentifier) ]
            beforeSectionWithIdentifier:@(AccountsSectionIdentifier)];
      [snapshot appendItemsWithIdentifiers:@[
        @(RowIdentifierErrorExplanation), @(RowIdentifierErrorButton)
      ]
                 intoSectionWithIdentifier:@(SyncErrorsSectionIdentifier)];
    }
  }
  [_accountMenuDataSource applySnapshot:snapshot animatingDifferences:YES];
  [self resize];
}

- (void)updateAccountListWithGaiaIDsToAdd:(NSArray<NSString*>*)gaiaIDsToAdd
                          gaiaIDsToRemove:(NSArray<NSString*>*)gaiaIDsToRemove
                            gaiaIDsToKeep:(NSArray<NSString*>*)gaiaIDsToKeep {
  CHECK(!_selectedIndexPath);
  NSDiffableDataSourceSnapshot* snapshot = _accountMenuDataSource.snapshot;

  NSMutableArray* accountsIdentifiersToAdd = [[NSMutableArray alloc] init];
  for (NSString* gaiaIDString in gaiaIDsToAdd) {
    [accountsIdentifiersToAdd addObject:gaiaIDString];
  }
  [snapshot insertItemsWithIdentifiers:accountsIdentifiersToAdd
              beforeItemWithIdentifier:@(RowIdentifierAddAccount)];

  NSMutableArray* accountsIdentifiersToRemove = [[NSMutableArray alloc] init];
  for (NSString* gaiaIDString in gaiaIDsToRemove) {
    [accountsIdentifiersToRemove addObject:gaiaIDString];
  }
  [snapshot deleteItemsWithIdentifiers:accountsIdentifiersToRemove];

  [snapshot reconfigureItemsWithIdentifiers:gaiaIDsToKeep];
  [_accountMenuDataSource applySnapshot:snapshot animatingDifferences:YES];
  [self resize];
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
  base::RecordAction(base::UserMetricsAction(kMobileKeyCommandClose));
  [self.mutator viewControllerWantsToBeClosed:self];
}

@end
