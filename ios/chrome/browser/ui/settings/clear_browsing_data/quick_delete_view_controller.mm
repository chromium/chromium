// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_view_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_ui_delegate.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_presentation_commands.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Trash icon view size.
CGFloat const kTrashIconContainerViewSize = 64;

// Trash icon view corner radius.
CGFloat const kTrashIconContainerViewCornerRadius = 15;

// Trash icon size that sits inside the entire view.
CGFloat const kTrashIconSize = 32;

// Bottom padding for the trash icon view.
CGFloat const kTrashIconContainerViewBottomPadding = 18;

// Top padding for the trash icon view.
CGFloat const kTrashIconContainerViewTopPadding = 33;

}  // namespace

@interface QuickDeleteViewController () <ConfirmationAlertActionHandler>
@end

@implementation QuickDeleteViewController

#pragma mark - UIViewController

- (instancetype)init {
  self = [super init];
  return self;
}

- (void)viewDidLoad {
  // TODO(crbug.com/335387869): Add time range and browsing data rows.

  self.aboveTitleView = [self trashIconView];
  self.titleTextStyle = UIFontTextStyleTitle2;
  self.titleString = l10n_util::GetNSString(IDS_IOS_CLEAR_BROWSING_DATA_TITLE);
  self.primaryActionString = l10n_util::GetNSString(IDS_IOS_CLEAR_BUTTON);
  self.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_DELETE_BROWSING_DATA_CANCEL);

  self.actionHandler = self;

  [super viewDidLoad];

  UIButtonConfiguration* buttonConfiguration =
      self.primaryActionButton.configuration;
  buttonConfiguration.background.backgroundColor =
      [UIColor colorNamed:kRedColor];
  self.primaryActionButton.configuration = buttonConfiguration;
}

#pragma mark - TableViewBottomSheetViewController

- (NSUInteger)rowCount {
  // TODO(crbug.com/335387869): Add rows to select the time range and the data
  // to be deleted.
  return 0;
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  // TODO(crbug.com/335387869): Trigger deletion.
}

- (void)confirmationAlertSecondaryAction {
  CHECK(self.presentationHandler);
  [self.presentationHandler dismissQuickDelete];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  CHECK(self.presentationHandler);
  [self.presentationHandler dismissQuickDelete];
}

#pragma mark - Private

// Returns a view of a trash icon with a red background with vertical padding.
- (UIView*)trashIconView {
  // Container of the trash icon that has the red background.
  UIView* iconContainerView = [[UIView alloc] init];
  iconContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  iconContainerView.layer.cornerRadius = kTrashIconContainerViewCornerRadius;
  iconContainerView.backgroundColor = [UIColor colorNamed:kRed50Color];

  // Trash icon that inside the container with the red background.
  UIImageView* icon =
      [[UIImageView alloc] initWithImage:DefaultSymbolTemplateWithPointSize(
                                             kTrashSymbol, kTrashIconSize)];
  icon.clipsToBounds = YES;
  icon.translatesAutoresizingMaskIntoConstraints = NO;
  icon.tintColor = [UIColor colorNamed:kRedColor];
  [iconContainerView addSubview:icon];

  [NSLayoutConstraint activateConstraints:@[
    [iconContainerView.widthAnchor
        constraintEqualToConstant:kTrashIconContainerViewSize],
    [iconContainerView.heightAnchor
        constraintEqualToConstant:kTrashIconContainerViewSize],
  ]];
  AddSameCenterConstraints(iconContainerView, icon);

  // Padding for the icon container view.
  UIView* outerView = [[UIView alloc] init];
  [outerView addSubview:iconContainerView];
  AddSameCenterXConstraint(outerView, iconContainerView);
  AddSameConstraintsToSidesWithInsets(
      iconContainerView, outerView, LayoutSides::kTop | LayoutSides::kBottom,
      NSDirectionalEdgeInsetsMake(kTrashIconContainerViewTopPadding, 0,
                                  kTrashIconContainerViewBottomPadding, 0));

  return outerView;
}

@end
