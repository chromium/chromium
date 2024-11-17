// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/tips/tips_passwords_coordinator.h"

#import "base/check.h"
#import "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"
#import "ios/chrome/browser/ui/content_suggestions/tips/ui/save_passwords_instructional_overlay_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/tips/ui/save_passwords_instructional_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/tips/ui/use_autofill_instructional_overlay_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/tips/ui/use_autofill_instructional_view_controller.h"

using segmentation_platform::TipIdentifier;

@implementation TipsPasswordsCoordinator {
  // The Tip identifier that determines which Passwords tip to show.
  segmentation_platform::TipIdentifier _identifier;

  // The view controller responsible for displaying the animated promo.
  AnimatedPromoViewController* _tipsInstructionalViewController;

  // The view controller responsible for displaying the instructional overlay.
  InstructionsBottomSheetViewController*
      _tipsInstructionalOverlayViewController;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                identifier:
                                    (segmentation_platform::TipIdentifier)
                                        identifier {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _identifier = identifier;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  _tipsInstructionalViewController =
      _identifier == TipIdentifier::kSavePasswords
          ? [[SavePasswordsInstructionalViewController alloc] init]
          : [[UseAutofillInstructionalViewController alloc] init];

  CHECK(_tipsInstructionalViewController);

  _tipsInstructionalViewController.actionHandler = self;

  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:_tipsInstructionalViewController];

  navigationController.modalPresentationStyle = UIModalPresentationFormSheet;

  [self.baseViewController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];

  navigationController.presentationController.delegate = self;
}

- (void)stop {
  [_tipsInstructionalViewController dismissViewControllerAnimated:YES
                                                       completion:nil];
  _tipsInstructionalOverlayViewController.actionHandler = nil;
  _tipsInstructionalOverlayViewController = nil;
  _tipsInstructionalViewController.actionHandler = nil;
  _tipsInstructionalViewController = nil;
  _delegate = nil;
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [_delegate tipsPasswordsCoordinatorDidFinish:self];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  // TODO(crbug.com/369457289): Track user interactions with the Tips module
  // using new metrics.
  [_delegate tipsPasswordsCoordinatorDidFinish:self];
}

- (void)confirmationAlertSecondaryAction {
  // TODO(crbug.com/369457289): Track user interactions with the Tips module
  // using new metrics.

  if (_tipsInstructionalViewController) {
    _tipsInstructionalOverlayViewController =
        _identifier == TipIdentifier::kSavePasswords
            ? [[SavePasswordsInstructionalOverlayViewController alloc] init]
            : [[UseAutofillInstructionalOverlayViewController alloc] init];

    _tipsInstructionalOverlayViewController.actionHandler = self;

    [_tipsInstructionalViewController
        presentViewController:_tipsInstructionalOverlayViewController
                     animated:YES
                   completion:nil];
  }
}

- (void)confirmationAlertDismissAction {
  if (_tipsInstructionalOverlayViewController) {
    [_tipsInstructionalOverlayViewController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:nil];
    _tipsInstructionalOverlayViewController = nil;

    return;
  }

  [_delegate tipsPasswordsCoordinatorDidFinish:self];
}

@end
