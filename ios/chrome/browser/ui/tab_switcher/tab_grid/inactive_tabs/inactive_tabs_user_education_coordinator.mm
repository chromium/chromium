// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_user_education_coordinator.h"

#import <CoreGraphics/CoreGraphics.h>

#import "base/i18n/message_formatter.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_constants.h"
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

// Returns the image to show in the user-education confirmation alert.
UIImage* ConfirmationAlertImage() {
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
    [[UIColor colorNamed:kBlue500Color] setFill];
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
    [UIColor.whiteColor setFill];
    UIImage* icon =
        DefaultSymbolTemplateWithPointSize(kSquareOnSquareDashedSymbol, 0);
    if (icon.size.width > icon.size.height) {
      CGFloat ratio = icon.size.height / icon.size.width;
      CGFloat drawingWidth = kTileSize - 2 * kIconPadding;
      CGFloat drawingHeight = drawingWidth * ratio;
      CGFloat verticalPadding = (kTileSize - drawingHeight) / 2;
      [icon drawInRect:CGRectInset(tileFrame, kIconPadding, verticalPadding)];
    } else {
      CGFloat ratio = icon.size.width / icon.size.height;
      CGFloat drawingHeight = kTileSize - 2 * kIconPadding;
      CGFloat drawingWidth = drawingHeight * ratio;
      CGFloat horizontalPadding = (kTileSize - drawingWidth) / 2;
      [icon drawInRect:CGRectInset(tileFrame, horizontalPadding, kIconPadding)];
    }
  }];
}

}  // namespace

@interface InactiveTabsUserEducationCoordinator () <
    ConfirmationAlertActionHandler,
    UIAdaptivePresentationControllerDelegate>
@end

@implementation InactiveTabsUserEducationCoordinator {
  // The confirmation alert showing the user education infos.
  ConfirmationAlertViewController* _confirmationAlert;
}

- (void)start {
  [super start];

  _confirmationAlert = [[ConfirmationAlertViewController alloc] init];
  _confirmationAlert.titleString =
      l10n_util::GetNSString(IDS_IOS_INACTIVE_TABS_USER_EDU_TITLE);
  _confirmationAlert.titleTextStyle = UIFontTextStyleTitle2;
  _confirmationAlert.subtitleString = base::SysUTF16ToNSString(
      base::i18n::MessageFormatter::FormatWithNumberedArgs(
          l10n_util::GetStringUTF16(IDS_IOS_INACTIVE_TABS_USER_EDU_SUBTITLE),
          InactiveTabsTimeThreshold().InDays()));
  _confirmationAlert.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_INACTIVE_TABS_USER_EDU_DONE);
  _confirmationAlert.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_INACTIVE_TABS_USER_EDU_GO_TO_SETTINGS);
  _confirmationAlert.image = ConfirmationAlertImage();
  _confirmationAlert.imageHasFixedSize = YES;
  _confirmationAlert.customSpacingBeforeImageIfNoNavigationBar =
      kImageTopSpacing;
  _confirmationAlert.customSpacingAfterImage = kImageBottomSpacing;
  _confirmationAlert.customSpacing = kSpacing;
  _confirmationAlert.topAlignedLayout = YES;
  _confirmationAlert.showDismissBarButton = NO;
  _confirmationAlert.actionHandler = self;
  _confirmationAlert.presentationController.delegate = self;
  _confirmationAlert.modalPresentationStyle = UIModalPresentationPageSheet;
  UISheetPresentationController* presentationController =
      _confirmationAlert.sheetPresentationController;
  presentationController.prefersEdgeAttachedInCompactHeight = YES;
  presentationController.detents = @[
    UISheetPresentationControllerDetent.mediumDetent,
    UISheetPresentationControllerDetent.largeDetent
  ];
  presentationController.preferredCornerRadius = 20;
  _confirmationAlert.view.accessibilityIdentifier =
      kInactiveTabsUserEducationAccessibilityIdentifier;

  [self.baseViewController presentViewController:_confirmationAlert
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [super stop];
  [_confirmationAlert dismissViewControllerAnimated:YES completion:nil];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [_delegate inactiveTabsUserEducationCoordinatorDidFinish:self];
}

- (void)confirmationAlertSecondaryAction {
  [_delegate inactiveTabsUserEducationCoordinatorDidTapSettingsButton:self];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [_delegate inactiveTabsUserEducationCoordinatorDidFinish:self];
}

@end
