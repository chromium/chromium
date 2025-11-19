// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/shared_tab_group_user_education_coordinator.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_constants.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Layout constants for the image.
const CGFloat kTileSize = 60;
const CGFloat kTileCornerRadius = 13;
const CGFloat kTileShadowRadius = 20;
const CGFloat kTileShadowOffsetX = 0;
const CGFloat kTileShadowOffsetY = 5;
const CGFloat kTileShadowOpacity = 0.1;
const CGFloat kIconPadding = 12;
const CGFloat kImageTopSpacing = 20;
const CGFloat kImageBottomSpacing = 8;
const CGFloat kSpacing = 1;

// Returns the image to be displayed. `trait_collection` is used to ensure the
// right colours are used.
UIImage* TabGroupImage(UITraitCollection* trait_collection) {
  CGRect tileFrame = CGRectMake(0, 0, kTileSize, kTileSize);
  // Enlarge the frame to account for the drop shadow.
  CGRect frame = CGRectInset(tileFrame, -kTileShadowRadius, -kTileShadowRadius);
  // Recenter the tile frame.
  tileFrame = CGRectOffset(tileFrame, kTileShadowRadius, kTileShadowRadius);

  UIGraphicsImageRendererFormat* format =
      [UIGraphicsImageRendererFormat preferredFormat];
  format.opaque = NO;

  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:frame.size format:format];

  return [renderer imageWithActions:^(
                       UIGraphicsImageRendererContext* UIContext) {
    CGContextRef context = UIContext.CGContext;

    // Draw the background with a shadow.
    [[[UIColor colorNamed:kBlue500Color]
        resolvedColorWithTraitCollection:trait_collection] setFill];
    UIBezierPath* path =
        [UIBezierPath bezierPathWithRoundedRect:tileFrame
                                   cornerRadius:kTileCornerRadius];
    CGContextSaveGState(context);
    CGColorRef shadowColor =
        [UIColor.blackColor colorWithAlphaComponent:kTileShadowOpacity].CGColor;
    CGContextSetShadowWithColor(
        context, CGSizeMake(kTileShadowOffsetX, kTileShadowOffsetY),
        kTileShadowRadius, shadowColor);
    [path fill];
    CGContextRestoreGState(context);

    // Draw the icon.
    [[[UIColor colorNamed:kSolidWhiteColor]
        resolvedColorWithTraitCollection:trait_collection] setFill];
    UIImage* icon = DefaultSymbolTemplateWithPointSize(kTabGroupsSymbol, 0);
    CGFloat ratio = icon.size.height / icon.size.width;
    CGFloat drawingWidth = kTileSize - 2 * kIconPadding;
    CGFloat drawingHeight = drawingWidth * ratio;
    CGFloat verticalPadding = (kTileSize - drawingHeight) / 2;
    [icon drawInRect:CGRectInset(tileFrame, kIconPadding, verticalPadding)];
  }];
}

}  // namespace

@interface SharedTabGroupUserEducationCoordinator () <
    ConfirmationAlertActionHandler,
    UIAdaptivePresentationControllerDelegate>
@end

@implementation SharedTabGroupUserEducationCoordinator {
  // The view controller displaying the user education.
  ConfirmationAlertViewController* _viewController;
}

- (void)start {
  ButtonStackConfiguration* configuration =
      [[ButtonStackConfiguration alloc] init];
  configuration.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_SHARED_GROUP_USER_EDUCATION_SHEET_GOT_IT);
  ConfirmationAlertViewController* confirmationAlert =
      [[ConfirmationAlertViewController alloc]
          initWithConfiguration:configuration];
  confirmationAlert.titleString =
      l10n_util::GetNSString(IDS_IOS_SHARED_GROUP_USER_EDUCATION_SHEET_TITLE);
  confirmationAlert.subtitleString = l10n_util::GetNSString(
      IDS_IOS_SHARED_GROUP_USER_EDUCATION_SHEET_SUBTITLE);
  confirmationAlert.image = TabGroupImage(confirmationAlert.traitCollection);
  confirmationAlert.imageHasFixedSize = YES;
  confirmationAlert.customSpacingBeforeImageIfNoNavigationBar =
      kImageTopSpacing;
  confirmationAlert.customSpacingAfterImage = kImageBottomSpacing;
  confirmationAlert.customSpacing = kSpacing;
  confirmationAlert.topAlignedLayout = YES;
  confirmationAlert.titleTextStyle = UIFontTextStyleTitle2;
  confirmationAlert.actionHandler = self;

  confirmationAlert.presentationController.delegate = self;
  confirmationAlert.modalPresentationStyle = UIModalPresentationPageSheet;
  UISheetPresentationController* presentationController =
      confirmationAlert.sheetPresentationController;
  presentationController.prefersEdgeAttachedInCompactHeight = YES;
  presentationController.detents = @[
    [UISheetPresentationControllerDetent mediumDetent],
    [UISheetPresentationControllerDetent largeDetent]
  ];
  presentationController.preferredCornerRadius = 20;
  confirmationAlert.view.accessibilityIdentifier =
      kSharedTabGroupUserEducationAccessibilityIdentifier;

  __weak ConfirmationAlertViewController* weakAlert = confirmationAlert;
  [confirmationAlert
      registerForTraitChanges:@[ UITraitUserInterfaceStyle.class ]
                  withHandler:^(id<UITraitEnvironment> traitEnvironment,
                                UITraitCollection* previousCollection) {
                    weakAlert.image = TabGroupImage(weakAlert.traitCollection);
                  }];

  _viewController = confirmationAlert;

  [self.baseViewController presentViewController:confirmationAlert
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_viewController.presentingViewController dismissViewControllerAnimated:YES
                                                               completion:nil];
  _viewController = nil;
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [self.delegate userEducationCoordinatorDidDismiss:self];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.delegate userEducationCoordinatorDidDismiss:self];
}

@end
