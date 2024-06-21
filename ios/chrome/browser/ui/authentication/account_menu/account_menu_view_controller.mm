// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_view_controller.h"

#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_constants.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_view_controller_presentation_delegate.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

const char kEditAccountListIdentifier[] = "kEditAccountListIdentifier";
const char kManageYourGoogleAccountIdentifier[] =
    "kManageYourGoogleAccountIdentifier";

namespace {

// Height and width of the buttons.
constexpr CGFloat kButtonSize = 22;

constexpr CGFloat kHalfSheetCornerRadius = 20.0;

}  // namespace

@implementation AccountMenuViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier = kAccountMenuTableViewId;
  self.tableView.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  [self setUpBottomSheetPresentationController];
  [self setUpNavigationController];
}

#pragma mark - Private

// Sets up the navigation controller’s buttons.
- (void)setUpNavigationController {
  // Stop button
  UIUserInterfaceIdiom idiom = [[UIDevice currentDevice] userInterfaceIdiom];
  if (idiom != UIUserInterfaceIdiomPad) {
    UIBarButtonItem* closeButton = [[UIBarButtonItem alloc]
        initWithBarButtonSystemItem:UIBarButtonSystemItemClose
                             target:self.delegate
                             action:@selector(viewControllerWantsToBeClosed)];
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
                    "Signin_AccountMenu_ManageAccounts"));
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
  self.navigationItem.leftBarButtonItem =
      [[UIBarButtonItem alloc] initWithImage:ellipsisImage menu:ellipsisMenu];
}

// Sets up the sheet presentation controller and its properties when using
// UIModalPresentationPageSheet mode.
- (void)setUpBottomSheetPresentationController {
  UISheetPresentationController* presentationController =
      self.sheetPresentationController;
  if (!self.sheetPresentationController) {
    return;
  }
  presentationController.prefersEdgeAttachedInCompactHeight = YES;
  presentationController.widthFollowsPreferredContentSizeWhenEdgeAttached = YES;
  presentationController.preferredCornerRadius = kHalfSheetCornerRadius;
  presentationController.detents = @[
    [UISheetPresentationControllerDetent mediumDetent],
    [UISheetPresentationControllerDetent largeDetent]
  ];
}

@end
