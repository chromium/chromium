// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/ui/auto_deletion/auto_deletion_promo_view_controller.h"

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/download/ui/auto_deletion/auto_deletion_promo_commands.h"
#import "ios/chrome/browser/download/ui/auto_deletion/auto_deletion_promo_mutator.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
// The size of the trash icon.
constexpr CGFloat kTrashSymbolSize = 32.0;
// The border radius of the icon's container.
constexpr CGFloat kSymbolBackgroundCornerRadius = 15.0;
// The height and width of the trash symbol's container.
constexpr CGFloat kSymbolContainerSize = 64;

// Creates a `UIView` that contains the promo's icon as a subview.
UIView* CreateIconContainer() {
  // Create the trash icon.
  UIImage* symbol = SymbolWithPointSize(SymbolArrowUpTrash, kTrashSymbolSize);
  UIImageView* symbolView = [[UIImageView alloc] initWithImage:symbol];
  symbolView.translatesAutoresizingMaskIntoConstraints = NO;
  [symbolView setTintColor:UIColor.whiteColor];

  // Create the retaining UIView that decorates the icon.
  UIView* symbolBackground = [[UIView alloc] init];
  symbolBackground.backgroundColor = [UIColor colorNamed:kPurple500Color];
  symbolBackground.layer.cornerRadius = kSymbolBackgroundCornerRadius;
  symbolBackground.translatesAutoresizingMaskIntoConstraints = NO;
  [symbolBackground addSubview:symbolView];

  // Center the icon within the container.
  [NSLayoutConstraint activateConstraints:@[
    [symbolBackground.widthAnchor
        constraintEqualToConstant:kSymbolContainerSize],
    [symbolBackground.heightAnchor
        constraintEqualToConstant:kSymbolContainerSize],
  ]];
  AddSameCenterConstraints(symbolBackground, symbolView);

  // Add a container to allow for taking the full width.
  UIView* container = [[UIView alloc] init];
  container.translatesAutoresizingMaskIntoConstraints = NO;
  [container addSubview:symbolBackground];
  [NSLayoutConstraint activateConstraints:@[
    [container.widthAnchor
        constraintGreaterThanOrEqualToAnchor:symbolBackground.widthAnchor],
    [container.heightAnchor
        constraintEqualToAnchor:symbolBackground.heightAnchor],
  ]];
  AddSameCenterConstraints(container, symbolBackground);

  return container;
}

}  // namespace

@interface AutoDeletionPromoViewController () <ConfirmationAlertActionHandler>
@end

@implementation AutoDeletionPromoViewController {
  ConfirmationAlertViewController* _promoScreen;
}

- (void)viewDidLoad {
  _promoScreen = [[ConfirmationAlertViewController alloc] init];
  _promoScreen.titleString =
      l10n_util::GetNSString(IDS_IOS_AUTO_DELETION_IPH_TITLE);
  _promoScreen.subtitleString =
      l10n_util::GetNSString(IDS_IOS_AUTO_DELETION_IPH_DESCRIPTION);
  _promoScreen.configuration.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_AUTO_DELETION_IPH_PRIMARY_ACTION);
  _promoScreen.configuration.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_AUTO_DELETION_IPH_REJECTION);
  [_promoScreen reloadConfiguration];
  _promoScreen.aboveTitleView = CreateIconContainer();
  _promoScreen.actionHandler = self;

  self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(dismissPromo)];

  [self addChildViewController:_promoScreen];
  [self.view addSubview:_promoScreen.view];
  _promoScreen.view.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(_promoScreen.view, self.view);
  [_promoScreen didMoveToParentViewController:self];
  [super viewDidLoad];
}

#pragma mark - ConfirmationAlertActionHandler

// Enables the Auto-deletion feature and displays the Auto-deletion
// action-sheet.
- (void)confirmationAlertPrimaryAction {
  base::RecordAction(
      base::UserMetricsAction("IOS.AutoDeletion.IPH.EnableButtonTapped"));
  [self.mutator enableAutoDeletion];
}

// Does not enable the Auto-deletion feature and dismisses the promo.
- (void)confirmationAlertSecondaryAction {
  base::RecordAction(
      base::UserMetricsAction("IOS.AutoDeletion.IPH.RejectButtonTapped"));
  [self.autoDeletionPromoHandler dismissAutoDeletion];
}

#pragma mark - Private

// Dismisses the promo.
- (void)dismissPromo {
  base::RecordAction(
      base::UserMetricsAction("IOS.AutoDeletion.IPH.DismissButtonTapped"));
  [self.autoDeletionPromoHandler dismissAutoDeletion];
}

@end
