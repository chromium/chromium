// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/tips/coordinator/tips_passwords_coordinator.h"

#import "base/check.h"
#import "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/tips/ui/save_passwords_instructional_view_controller.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/tips/ui/use_autofill_instructional_view_controller.h"
#import "ios/chrome/browser/instructions_bottom_sheet/ui/instructions_bottom_sheet_view_controller.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

using segmentation_platform::TipIdentifier;

@implementation TipsPasswordsCoordinator {
  // The Tip identifier that determines which Passwords tip to show.
  segmentation_platform::TipIdentifier _identifier;

  // The view controller responsible for displaying the animated promo.
  AnimatedPromoViewController* _tipsInstructionalViewController;

  // The view controller responsible for displaying the instructional overlay.
  InstructionsBottomSheetViewController*
      _tipsInstructionalOverlayViewController;

  // Navigation controller for the instructions.
  UINavigationController* _instructionsNavigationController;
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

  _tipsInstructionalViewController.navigationItem.rightBarButtonItem =
      [[UIBarButtonItem alloc]
          initWithBarButtonSystemItem:UIBarButtonSystemItemClose
                               target:self
                               action:@selector(dismissSheet)];

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
  _instructionsNavigationController = nil;
  _tipsInstructionalViewController.actionHandler = nil;
  _tipsInstructionalViewController = nil;
  _delegate = nil;
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self dismissSheet];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  // TODO(crbug.com/369457289): Track user interactions with the Tips module
  // using new metrics.
  [self dismissSheet];
}

- (void)confirmationAlertSecondaryAction {
  // TODO(crbug.com/369457289): Track user interactions with the Tips module
  // using new metrics.

  if (_tipsInstructionalViewController) {
    _tipsInstructionalOverlayViewController =
        [[InstructionsBottomSheetViewController alloc]
            initWithTitle:l10n_util::GetNSString(
                              IDS_IOS_MAGIC_STACK_TIP_TUTORIAL_TITLE)
             instructions:[self instructionsSteps]];

    _tipsInstructionalOverlayViewController.actionHandler = self;

    _instructionsNavigationController = [[UINavigationController alloc]
        initWithRootViewController:_tipsInstructionalOverlayViewController];
    _tipsInstructionalOverlayViewController.navigationItem.rightBarButtonItem =
        [[UIBarButtonItem alloc]
            initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                                 target:self
                                 action:@selector(dismissInstructions)];

    [_tipsInstructionalViewController
        presentViewController:_instructionsNavigationController
                     animated:YES
                   completion:nil];
  }
}

#pragma mark - Private

// Dismisses the sheet.
- (void)dismissSheet {
  [_delegate tipsPasswordsCoordinatorDidFinish:self];
}

// Dismisses the instructions.
- (void)dismissInstructions {
  [_instructionsNavigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  _instructionsNavigationController = nil;
  _tipsInstructionalOverlayViewController = nil;
}

// Returns the instruction steps.
- (NSArray<NSString*>*)instructionsSteps {
  if (_identifier == TipIdentifier::kSavePasswords) {
    return @[
      l10n_util::GetNSString(
          IDS_IOS_MAGIC_STACK_TIP_SAVE_PASSWORDS_TUTORIAL_STEP_1),
      l10n_util::GetNSString(
          IDS_IOS_MAGIC_STACK_TIP_SAVE_PASSWORDS_TUTORIAL_STEP_2),
      l10n_util::GetNSString(
          IDS_IOS_MAGIC_STACK_TIP_SAVE_PASSWORDS_TUTORIAL_STEP_3),
    ];
  }
  return @[
    l10n_util::GetNSString(
        IDS_IOS_MAGIC_STACK_TIP_AUTOFILL_PASSWORDS_TUTORIAL_STEP_1),
    l10n_util::GetNSString(
        IDS_IOS_MAGIC_STACK_TIP_AUTOFILL_PASSWORDS_TUTORIAL_STEP_2),
    l10n_util::GetNSString(
        IDS_IOS_MAGIC_STACK_TIP_AUTOFILL_PASSWORDS_TUTORIAL_STEP_3),
  ];
}

@end
