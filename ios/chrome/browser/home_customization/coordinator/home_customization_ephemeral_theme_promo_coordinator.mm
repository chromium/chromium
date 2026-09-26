// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/coordinator/home_customization_ephemeral_theme_promo_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/home_customization/coordinator/home_customization_ephemeral_theme_promo_mediator.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service_factory.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_ephemeral_theme_promo_view_controller.h"
#import "ios/chrome/browser/promos_manager/coordinator/promos_manager_ui_handler.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/ephemeral_theme_promo_commands.h"
#import "ios/chrome/browser/shared/public/commands/new_tab_page_commands.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

namespace {

// Maximum height ratio of the screen for the promo bottom sheet.
constexpr CGFloat kMaxSheetHeightRatio = 0.75;

}  // namespace

@interface HomeCustomizationEphemeralThemePromoCoordinator () <
    ConfirmationAlertActionHandler,
    UIAdaptivePresentationControllerDelegate>
@end

@implementation HomeCustomizationEphemeralThemePromoCoordinator {
  HomeCustomizationEphemeralThemePromoViewController* _viewController;
  HomeCustomizationEphemeralThemePromoMediator* _mediator;
  BOOL _openCustomizationMenuOnDismiss;
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];

  _viewController =
      [[HomeCustomizationEphemeralThemePromoViewController alloc] init];
  _mediator = [[HomeCustomizationEphemeralThemePromoMediator alloc]
                 initWithPrefService:self.profile->GetPrefs()
      backgroundCustomizationService:HomeBackgroundCustomizationServiceFactory::
                                         GetForProfile(self.profile)
                  promoDataDirectory:self.profile->GetStatePath()];
  _mediator.consumer = _viewController;

  _viewController.actionHandler = self;
  _viewController.presentationController.delegate = self;

  __weak __typeof(self) weakSelf = self;
  auto preferredHeightForSheetContent = ^CGFloat(
      id<UISheetPresentationControllerDetentResolutionContext> context) {
    return [weakSelf
        preferredSheetHeightForMaximumDetentValue:context.maximumDetentValue];
  };

  UISheetPresentationController* sheetController =
      _viewController.sheetPresentationController;
  if (sheetController) {
    sheetController.detents = @[ [UISheetPresentationControllerDetent
        customDetentWithIdentifier:nil
                          resolver:preferredHeightForSheetContent] ];
  }

  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  ProceduralBlock completion = nil;
  if (_openCustomizationMenuOnDismiss) {
    id<NewTabPageCommands> ntpHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), NewTabPageCommands);
    completion = ^{
      [ntpHandler showHomeBackgroundCustomizationPromoWithUIHandler:nil];
    };
  }

  if (_viewController.presentingViewController &&
      !_viewController.isBeingDismissed) {
    [_viewController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:completion];
  }

  [_mediator disconnect];
  _mediator = nil;
  _viewController = nil;
  self.promosUIHandler = nil;

  [super stop];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [_mediator applyEphemeralTheme];
  _openCustomizationMenuOnDismiss = YES;
  [self hidePromo];
}

- (void)confirmationAlertSecondaryAction {
  [self hidePromo];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self hidePromo];
}

#pragma mark - Private

// Dismisses the promo via the command dispatcher handler.
- (void)hidePromo {
  [self.promosUIHandler promoWasDismissed];
  id<EphemeralThemePromoCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), EphemeralThemePromoCommands);
  [handler hideEphemeralThemePromo];
}

// Returns the preferred sheet height clamped to `kMaxSheetHeightRatio` of
// `maximumDetentValue`.
- (CGFloat)preferredSheetHeightForMaximumDetentValue:
    (CGFloat)maximumDetentValue {
  CGFloat height = [_viewController preferredHeightForContent];
  return MIN(height, kMaxSheetHeightRatio * maximumDetentValue);
}

@end
