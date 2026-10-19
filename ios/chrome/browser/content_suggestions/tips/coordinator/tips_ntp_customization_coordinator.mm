// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/tips/coordinator/tips_ntp_customization_coordinator.h"

#import "base/notreached.h"
#import "ios/chrome/browser/content_suggestions/tips/ui/ntp_customization_promo_view_controller.h"
#import "ios/chrome/common/ui/button_stack/button_stack_action_delegate.h"

@interface TipsNTPCustomizationCoordinator () <ButtonStackActionDelegate>
@end

@implementation TipsNTPCustomizationCoordinator {
  NTPCustomizationPromoViewController* _viewController;
}

#pragma mark - ChromeCoordinator

- (void)start {
  _viewController = [[NTPCustomizationPromoViewController alloc] init];
  _viewController.actionDelegate = self;
  _viewController.modalPresentationStyle = UIModalPresentationPageSheet;

  UISheetPresentationController* sheetController =
      _viewController.sheetPresentationController;
  sheetController.prefersEdgeAttachedInCompactHeight = YES;
  sheetController.widthFollowsPreferredContentSizeWhenEdgeAttached = YES;
  sheetController.detents = @[
    [_viewController preferredHeightDetent],
    [UISheetPresentationControllerDetent largeDetent],
  ];

  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  _viewController.actionDelegate = nil;
  if (_viewController.presentingViewController) {
    [_viewController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:nil];
  }
  _viewController = nil;
  _delegate = nil;
}

#pragma mark - ButtonStackActionDelegate

- (void)didTapPrimaryActionButton {
  // TODO(crbug.com/571447764): Handle the primary button tap.
  NOTREACHED();
}

- (void)didTapSecondaryActionButton {
  // TODO(crbug.com/571447764): Handle the secondary button tap.
  NOTREACHED();
}

- (void)didTapTertiaryActionButton {
  NOTREACHED();
}

- (void)didDismissButtonStackViewController {
  [self.delegate tipsNTPCustomizationCoordinator:self didFinishWithAccept:NO];
}

@end
