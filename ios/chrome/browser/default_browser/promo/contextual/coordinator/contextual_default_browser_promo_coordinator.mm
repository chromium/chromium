// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/promo/contextual/coordinator/contextual_default_browser_promo_coordinator.h"

#import <UIKit/UIKit.h>

#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/default_browser/promo/contextual/coordinator/contextual_default_browser_promo_mediator.h"
#import "ios/chrome/browser/default_browser/promo/contextual/public/contextual_default_browser_promo_metrics.h"
#import "ios/chrome/browser/default_browser/promo/contextual/ui/contextual_default_browser_promo_view_controller.h"
#import "ios/chrome/browser/default_browser/promo/public/features.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/contextual_default_browser_promo_commands.h"
#import "ios/chrome/browser/shared/public/commands/picture_in_picture_commands.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

namespace {

// Maximum height ratio of the screen for the promo bottom sheet.
constexpr CGFloat kMaxSheetHeightRatio = 0.75;

}  // namespace

@interface ContextualDefaultBrowserPromoCoordinator () <
    ConfirmationAlertActionHandler,
    UIAdaptivePresentationControllerDelegate>
@end

@implementation ContextualDefaultBrowserPromoCoordinator {
  ContextualDefaultBrowserPromoType _promoType;
  ContextualDefaultBrowserPromoViewController* _viewController;
  ContextualDefaultBrowserPromoMediator* _mediator;
}

#pragma mark - Initializers

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                 promoType:(ContextualDefaultBrowserPromoType)
                                               promoType {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _promoType = promoType;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];

  RecordContextualDefaultBrowserPromoShown(_promoType);

  _viewController = [[ContextualDefaultBrowserPromoViewController alloc] init];
  _mediator = [[ContextualDefaultBrowserPromoMediator alloc]
      initWithPromoType:_promoType];
  _mediator.consumer = _viewController;

  _viewController.actionHandler = self;
  _viewController.presentationController.delegate = self;

  __weak ContextualDefaultBrowserPromoViewController* weakViewController =
      _viewController;
  auto preferredHeightForSheetContent = ^CGFloat(
      id<UISheetPresentationControllerDetentResolutionContext> context) {
    CGFloat height = [weakViewController preferredHeightForContent];
    return MIN(height, kMaxSheetHeightRatio * context.maximumDetentValue);
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
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForProfile(self.profile);
  if (tracker) {
    switch (_promoType) {
      case ContextualDefaultBrowserPromoType::kGemini:
        tracker->Dismissed(
            feature_engagement::
                kIPHiOSPromoContextualDefaultBrowserGeminiFeature);
        break;
    }
  }

  if (_viewController.presentingViewController &&
      !_viewController.isBeingDismissed) {
    [_viewController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:nil];
  }

  [_mediator disconnect];
  _mediator = nil;
  _viewController = nil;

  [super stop];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [self notifyPromoActionTaken];
  RecordContextualDefaultBrowserPromoAction(
      _promoType, ContextualDefaultBrowserPromoAction::kPrimaryActionTapped);

  if (IsDefaultBrowserPictureInPictureEnabled()) {
    id<PictureInPictureCommands> pictureInPictureHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), PictureInPictureCommands);
    [self hidePromo];
    OpenIOSDefaultBrowserSettingsPage(IsDefaultAppsPictureInPictureVariant(),
                                      /*ui_application_to_use=*/nil,
                                      pictureInPictureHandler);
    return;
  }

  OpenIOSDefaultBrowserSettingsPage(/*force_default_apps_if_available=*/false,
                                    /*ui_application_to_use=*/nil,
                                    /*picture_in_picture_handler=*/nil);
  [self hidePromo];
}

- (void)confirmationAlertSecondaryAction {
  [self notifyPromoActionTaken];
  RecordContextualDefaultBrowserPromoAction(
      _promoType, ContextualDefaultBrowserPromoAction::kSecondaryActionTapped);
  [self hidePromo];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  RecordContextualDefaultBrowserPromoAction(
      _promoType, ContextualDefaultBrowserPromoAction::kSwipeDown);
  [self hidePromo];
}

#pragma mark - Private

// Dismisses the promo via the command dispatcher handler.
- (void)hidePromo {
  id<ContextualDefaultBrowserPromoCommands> handler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(),
                         ContextualDefaultBrowserPromoCommands);
  [handler hideContextualDefaultBrowserPromo];
}

// Notifies the feature engagement tracker that a promo action was taken.
- (void)notifyPromoActionTaken {
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForProfile(self.profile);
  if (!tracker) {
    return;
  }

  switch (_promoType) {
    case ContextualDefaultBrowserPromoType::kGemini:
      tracker->NotifyEvent(
          feature_engagement::events::kDefaultBrowserPromoContextualGeminiUsed);
      break;
  }
}

@end
